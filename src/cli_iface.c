#include "oryx.h"
#include "vty.h"
#include "command.h"
#include "prefix.h"

#include "common_private.h"
#include "map_private.h"
#include "cli_iface.h"

#define PORT_LOCK
#ifdef PORT_LOCK
static INIT_MUTEX(port_lock);
#else
#undef do_lock(lock)
#undef do_unlock(lock)
#define do_lock(lock)
#define do_unlock(lock)
#endif

struct iface_t {
#define IS_PANLE_PORT 1
	const char *if_name;
	int if_type;
	int is_a_panel_port;
	void (*linkstate_poll)(struct port_t *this);
	int  (*ethdev_up)(const char *if_name);
};

#define VTY_ERROR_PORT(prefix, alias)\
	vty_out (vty, "%s(Error)%s %s port \"%s\"%s", \
		draw_color(COLOR_RED), draw_color(COLOR_FIN), prefix, alias, VTY_NEWLINE)

#define VTY_SUCCESS_PORT(prefix, v)\
	vty_out (vty, "%s(Success)%s %s port \"%s\"(%u)%s", \
		draw_color(COLOR_GREEN), draw_color(COLOR_FIN), prefix, v->sc_alias, v->ul_id, VTY_NEWLINE)

atomic_t n_intf_elements = ATOMIC_INIT(0);

static __oryx_always_inline__
void mk_alias (struct port_t *entry)
{

	switch (entry->type) {
	case ETH_GE:
			sprintf (entry->sc_alias, "%s%u", (char *)"G0/", entry->ul_id);
			break;
	case ETH_XE:
			sprintf (entry->sc_alias, "%s%u", (char *)"XG0/", entry->ul_id);
			break;
	default:
			sprintf (entry->sc_alias, "%s%u", (char *)"UnknownG0/", entry->ul_id);
			break;
	}
}


static __oryx_always_inline__
void port_free (ht_value_t v)
{
	/** Never free here! */
	v = v;
}

static uint32_t
port_hval (struct oryx_htable_t *ht,
		ht_value_t v, uint32_t s) 
{
     uint8_t *d = (uint8_t *)v;
     uint32_t i;
     uint32_t hv = 0;

     for (i = 0; i < s; i++) {
         if (i == 0)      hv += (((uint32_t)*d++));
         else if (i == 1) hv += (((uint32_t)*d++) * s);
         else             hv *= (((uint32_t)*d++) * i) + s + i;
     }

     hv *= s;
     hv %= ht->array_size;
     
     return hv;
}

static int
port_cmp (ht_value_t v1, 
		uint32_t s1,
		ht_value_t v2,
		uint32_t s2)
{
	int xret = 0;

	if (!v1 || !v2 ||s1 != s2 ||
		memcmp(v1, v2, s2))
		xret = 1;	/** Compare failed. */

	return xret;
}

void port_entry_new (struct port_t **port, const struct iface_t *iface)
{
	int id;
	
	/** create an port */
	(*port) = kmalloc (sizeof (struct port_t), MPF_CLR, __oryx_unused_val__);

	ASSERT ((*port));

	(*port)->us_mtu = 1500;
	(*port)->belong_maps = vec_init(1024);
	memcpy(&(*port)->sc_alias[0], iface->if_name, strlen(iface->if_name));
	(*port)->sc_alias_fixed = iface->if_name;
	(*port)->type = iface->if_type;
	(*port)->ethdev_state_poll = iface->linkstate_poll;
	(*port)->ethdev_up = iface->ethdev_up;

	/** port counters */
	id = COUNTER_RX;
	(*port)->counter_bytes[id] = oryx_register_counter("port.rx.bytes", 
			"bytes Rx for this port", &(*port)->perf_private_ctx);
	(*port)->counter_pkts[id] = oryx_register_counter("port.rx.pkts",
			"pkts Rx for this port", &(*port)->perf_private_ctx);

	id = COUNTER_TX;
	(*port)->counter_bytes[id] = oryx_register_counter("port.tx.bytes", 
			"bytes Tx for this port", &(*port)->perf_private_ctx);
	(*port)->counter_pkts[id] = oryx_register_counter("port.tx.pkts",
			"pkts Tx for this port", &(*port)->perf_private_ctx);

	/**
	 * More Counters here.
	 * TODO ...
	 */
	
	/** last step */
	oryx_counter_get_array_range(COUNTER_RANGE_START(&(*port)->perf_private_ctx), 
			COUNTER_RANGE_END(&(*port)->perf_private_ctx), 
			&(*port)->perf_private_ctx);
}

static void port_entry_destroy (struct port_t *port)
{
	vec_free (port->belong_maps);
	kfree (port);
}

/** if a null port specified, map_entry_output display all */
static __oryx_always_inline__
void port_entry_output (struct port_t *port, struct vty *vty)
{

	ASSERT (port);

	vty_out (vty, "%s", VTY_NEWLINE);
	/** find this port named 'alias'. */
	vty_out (vty, "%16s\"%s\"(%u)%s", "Port ", port->sc_alias, port->ul_id, VTY_NEWLINE);

	vty_out (vty, "		%16s%02X:%02X:%02X:%02X:%02X:%02X%s", 
		"Mac: ", 
		port->eth_addr[0], port->eth_addr[1], port->eth_addr[2],
		port->eth_addr[3], port->eth_addr[4], port->eth_addr[5], VTY_NEWLINE);
	
	vty_out (vty, "		%16s%s%s", 
		"Duplex: ", (port->ul_flags & NETDEV_DUPLEX_FULL) ? "Full" : "Half", VTY_NEWLINE);

	vty_out (vty, " 	%16s%s%s", 
		"State: ", (port->ul_flags & NETDEV_ADMIN_UP) ? "Up" : "Down", VTY_NEWLINE);

	vty_out (vty, "		%16s%d%s", 
		"MTU: ", port->us_mtu, VTY_NEWLINE);

	vty_out (vty, "		%16s", "Maps: ");

	oryx_vector v = port->belong_maps;
	int actives = vec_active(v);
	if (!actives) vty_out (vty, "N/A%s", VTY_NEWLINE);
	else {
		int i;
		
		struct map_t *p;
		vec_foreach_element (v, i, p) {
			if (p) {
				vty_out (vty, "%s", p->sc_alias);
				-- actives;
				if (actives) vty_out (vty, ", ");
				else actives = actives;
			}
			else {p = p;}
		}
		vty_out (vty, "%s", VTY_NEWLINE);
	}
	vty_out (vty, "%16s%s", "_______________________________________________", VTY_NEWLINE);

}

	
/** if a null port specified, map_entry_output display all */
static __oryx_always_inline__
void port_entry_stat_output (struct port_t *port, struct vty *vty)
{
	char format[256] = {0};
	u64 pkts[RX_TX];
	u64 bytes[RX_TX];
	int id = 0;
	
	if (unlikely (!port)) 
		return;

	id = COUNTER_RX;
	pkts[id]  = oryx_counter_get(&port->perf_private_ctx, port->counter_pkts[id]);
	bytes[id] = oryx_counter_get(&port->perf_private_ctx, port->counter_bytes[id]);
	
	id = COUNTER_TX;
	pkts[id]  = oryx_counter_get(&port->perf_private_ctx, port->counter_pkts[id]);
	bytes[id] = oryx_counter_get(&port->perf_private_ctx, port->counter_bytes[id]);
	{
		/** find this port named 'alias'. */
		vty_out (vty, "%15s(%u)", port->sc_alias, port->ul_id);

		sprintf (format, "%llu/%llu", bytes[COUNTER_RX], bytes[COUNTER_TX]);
		vty_out (vty, "%20s", format);

		sprintf (format, "%llu/%llu", pkts[COUNTER_RX], pkts[COUNTER_TX]);
		vty_out (vty, "%20s", format);

		vty_newline(vty);
	} 
}

static __oryx_always_inline__
void do_port_rename (struct port_t *port, struct prefix_t *new_alias)
{
	vlib_port_main_t *vp = &vlib_port_main;

	/** Delete old alias from hash table. */
	oryx_htable_del (vp->htable, port_alias(port), strlen (port_alias(port)));
	memset (port_alias(port), 0, strlen (port_alias(port)));
	memcpy (port_alias(port), (char *)new_alias->v, 
		strlen ((char *)new_alias->v));
	/** New alias should be rewrite to hash table. */
	oryx_htable_add (vp->htable, port_alias(port), strlen (port_alias(port)));		
}

static __oryx_always_inline__
void port_entry_config (struct port_t *port, void __oryx_unused__ *vty_, void *arg)
{

	struct vty *vty = vty_;
	struct prefix_t *var;
	u32 ul_flags = port->ul_flags;
	
	var = (struct prefix_t *)arg;

	switch (var->cmd)
	{
		case INTERFACE_SET_ALIAS:
			if (strlen ((char *)var->v) > sizeof (port->sc_alias)) {
				VTY_ERROR_PORT("invalid alias", (char *)port->sc_alias);
				break;
			}
			struct port_t *exist;/** check same alias. */
			struct prefix_t lp = {
				.cmd = LOOKUP_ALIAS,
				.v = var->v,
				.s = __oryx_unused_val__,
			};
			port_table_entry_lookup(&lp, &exist);
			if (exist) {
				VTY_ERROR_PORT("same", (char *)exist->sc_alias);
				break;
			}
			do_port_rename (port, var);
			break;
			
		case INTERFACE_SET_MTU:
			port->us_mtu = atoi (var->v);
			break;

		case INTERFACE_SET_LOOPBACK:
			ul_flags |= NETDEV_LOOPBACK;
			if (!strncmp ((char *)var->v, "d", 1))
				ul_flags &= ~NETDEV_LOOPBACK;
			port->ul_flags = ul_flags;
			break;
			
		case INTERFACE_CLEAR_STATS:
			break;
			
		default:
			break;
	}
}

static __oryx_always_inline__
int port_table_entry_remove (struct port_t *port)
{
	vlib_port_main_t *vp = &vlib_port_main;
	
	do_lock (&port_lock);
	int r = oryx_htable_del(vp->htable, port->sc_alias, strlen((const char *)port->sc_alias));
	if (r == 0) {
		vec_unset (vp->entry_vec, port->ul_id);
	}
	do_unlock (&port_lock);

	/** Should you free port here ? */

	return r;  
}

static __oryx_always_inline__
void port_table_entry_lookup_alias (char *alias, struct port_t **p)
{
	vlib_port_main_t *vp = &vlib_port_main;

	(*p) = NULL;

	/** ALIASE validate check. */
	if (unlikely (!alias)) 
		return;

	void *s = oryx_htable_lookup(vp->htable, alias, strlen(alias));

	if (likely (s)) {
		/** THIS IS A VERY CRITICAL POINT. */
		do_lock (&port_lock);
		(*p) = (struct port_t *) container_of (s, struct port_t, sc_alias);
		do_unlock (&port_lock);
	}
}

static __oryx_always_inline__
void port_table_entry_lookup_id (u32 id, struct port_t **p)
{
	vlib_port_main_t *vp = &vlib_port_main;
	/** ID validate check. */

	(*p) = NULL;

	do_lock (&port_lock);
	(*p) = (struct port_t *) vec_lookup_ensure (vp->entry_vec, id);
	do_unlock (&port_lock);

}

void port_table_entry_lookup (struct prefix_t *lp, 
	struct port_t **p)
{

	ASSERT (lp);
	ASSERT (p);
	
	switch (lp->cmd) {
		case LOOKUP_ID:
			port_table_entry_lookup_id ((*(u32*)lp->v), p);
			break;

		case LOOKUP_ALIAS:
			port_table_entry_lookup_alias ((char*)lp->v, p);
			break;

		default:
			break;
	}
}
	
__oryx_always_extern__
int port_table_entry_add (struct port_t *port)
{
	vlib_port_main_t *vp = &vlib_port_main;

	ASSERT (port);

	do_lock (&port_lock);
	int r = oryx_htable_add(vp->htable, port->sc_alias, strlen((const char *)port->sc_alias));
	if (r == 0) {
		vec_set_index (vp->entry_vec, port->ul_id, port);
		oryx_logn("registering interface %s ...", port->sc_alias);
	}
	do_unlock (&port_lock);

	return r;
}

static int port_table_entry_remove_and_destroy (struct port_t *port)
{

	int r;

	ASSERT (port);
	
	r = port_table_entry_remove (port);
	if (likely (r)) {
		port_entry_destroy (port);
	}
	
	return r;  
}

#define PRINT_SUMMARY	\
	vty_out (vty, "matched %d element(s), total %d element(s)%s", \
		atomic_read(&n_intf_elements), (int)vec_count(vp->entry_vec), VTY_NEWLINE);

DEFUN(show_interfacce,
      show_interface_cmd,
      "show interface [WORD]",
      SHOW_STR SHOW_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR)
{
	vlib_port_main_t *vp = &vlib_port_main;
	vty_out (vty, "Trying to display %s%d%s elements ...%s", 
		draw_color(COLOR_RED), vec_active(vp->entry_vec), draw_color(COLOR_FIN), 
		VTY_NEWLINE);
	
	vty_out (vty, "%16s%s", "_______________________________________________", VTY_NEWLINE);

	if (argc == 0) {
		foreach_port_func1_param1 (
			argv[0], port_entry_output, vty);
	}
	else {
		split_foreach_port_func1_param1 (
			argv[0], port_entry_output, vty);
	}

	PRINT_SUMMARY;

    return CMD_SUCCESS;
}

DEFUN(show_interfacce_stats,
      show_interfacce_stats_cmd,
      "show interface stats [WORD]",
      SHOW_STR SHOW_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR)
{
	vlib_port_main_t *vp = &vlib_port_main;
	vty_out(vty, "Trying to display %d elements ...%s", 
			vec_active(vp->entry_vec), VTY_NEWLINE);
	vty_out(vty, "%15s%20s%20s%s", "Port", "Bytes(I/O)", "Packets(I/O)", VTY_NEWLINE);
	
	if (argc == 0) {
		foreach_port_func1_param1 (
			argv[0], port_entry_stat_output, vty);
	}
	else {
		split_foreach_port_func1_param1 (
			argv[0], port_entry_stat_output, vty);
	}

	PRINT_SUMMARY;

    return CMD_SUCCESS;
}

DEFUN(interface_alias,
      interface_alias_cmd,
      "interface WORD alias WORD",
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
	  KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR)
{
	vlib_port_main_t *vp = &vlib_port_main;
	struct prefix_t var = {
		.cmd = INTERFACE_SET_ALIAS,
		.v = (char *)argv[1],
		.s = __oryx_unused_val__,
	};
	
	split_foreach_port_func1_param2 (
		argv[0], port_entry_config, vty, (void *)&var);

	PRINT_SUMMARY;
	
	return CMD_SUCCESS;
}

DEFUN(interface_mtu,
      interface_mtu_cmd,
      "interface WORD mtu <64-65535>",
      KEEP_QUITE_STR
      KEEP_QUITE_CSTR
      KEEP_QUITE_STR
      KEEP_QUITE_CSTR
      KEEP_QUITE_STR
      KEEP_QUITE_CSTR
      KEEP_QUITE_STR
      KEEP_QUITE_CSTR)
{
	vlib_port_main_t *vp = &vlib_port_main;
	struct prefix_t var = {
		.cmd = INTERFACE_SET_MTU,
		.v = (char *)argv[1],
		.s = __oryx_unused_val__,
	};
	
	split_foreach_port_func1_param2 (
		argv[0], port_entry_config, vty, (void *)&var);

	PRINT_SUMMARY;
	
	return CMD_SUCCESS;
}

DEFUN(interface_looback,
      interface_looback_cmd,
      "interface WORD loopback (enable|disable)",
      KEEP_QUITE_STR
      KEEP_QUITE_CSTR
      KEEP_QUITE_STR
      KEEP_QUITE_CSTR
      KEEP_QUITE_STR
      KEEP_QUITE_CSTR
      KEEP_QUITE_STR
      KEEP_QUITE_CSTR
      KEEP_QUITE_STR
      KEEP_QUITE_CSTR)
{
	vlib_port_main_t *vp = &vlib_port_main;
	struct prefix_t var = {
		.cmd = INTERFACE_SET_LOOPBACK,
		.v = (char *)argv[1],
		.s = __oryx_unused_val__,
	};
	
	split_foreach_port_func1_param2 (
		argv[0], port_entry_config, vty, (void *)&var);

	PRINT_SUMMARY;
	
	return CMD_SUCCESS;
}

DEFUN(interface_stats_clear,
      interface_stats_clear_cmd,
      "clear interface stats WORD",
      KEEP_QUITE_STR
      KEEP_QUITE_CSTR
      KEEP_QUITE_STR
      KEEP_QUITE_CSTR
      KEEP_QUITE_STR
      KEEP_QUITE_CSTR
      KEEP_QUITE_STR
      KEEP_QUITE_CSTR)
{
	vlib_port_main_t *vp = &vlib_port_main;
	struct prefix_t var = {
		.cmd = INTERFACE_CLEAR_STATS,
		.v = (char *)argv[1],
		.s = __oryx_unused_val__,
	};
	
	split_foreach_port_func1_param2 (
		argv[0], port_entry_config, vty, (void *)&var);

	PRINT_SUMMARY;
	
	return CMD_SUCCESS;
}

void iface_ethtool_poll_linkstate(struct port_t *this)
{
	int rv;
	int if_need_up_configureation = 0;
	vlib_port_main_t *vp = &vlib_port_main;
	
	if (this->type == ETH_GE) {
		/** use ethtool. */
		rv = netdev_is_up(this->sc_alias_fixed);
		/** up -> down */
		if ((this->ul_flags & NETDEV_ADMIN_UP) && rv == 0) {
			oryx_logn("%s is down", this->sc_alias);
			this->ul_u2d_times ++;
			this->ul_flags |= NETDEV_POLL_UP;
		}
		/** down -> up */
		if (!(this->ul_flags & NETDEV_ADMIN_UP) && rv == 1)
			oryx_logn("%s is up", this->sc_alias);
		switch(rv) {
			case 0:
				this->ul_flags &= ~NETDEV_ADMIN_UP;
				break;
			case 1:
				this->ul_flags |= NETDEV_ADMIN_UP;
				break;
			default:
				oryx_logn("%s error", this->sc_alias);
				break;
		}
	}

	if (this->type == ETH_XE) {
		rv = netdev_is_up(this->sc_alias_fixed);
		/** up -> down */
		if ((this->ul_flags & NETDEV_ADMIN_UP) && rv == 0) {
			oryx_logn("%s is down", this->sc_alias);
		}
		/** down -> up */
		if (!(this->ul_flags & NETDEV_ADMIN_UP) && rv == 1)
			oryx_logn("%s is up", this->sc_alias);
		switch(rv) {
			case 0:
				this->ul_flags &= ~NETDEV_ADMIN_UP;
				break;
			case 1:
				this->ul_flags |= NETDEV_ADMIN_UP;
				break;
			default:
				oryx_logn("%s error", this->sc_alias);
				break;
		}

		if(rv && !strcmp(this->sc_alias_fixed, "enp5s0f1")) {
			vp->enp5s0f1_is_up = rv;
		}
	}
}

static const struct iface_t iface_list[] = {
	{
		"enp5s0f1",
		ETH_XE,
		!IS_PANLE_PORT,
		NULL,
		netdev_up
	},
	
	{
		"enp5s0f2",
		ETH_XE,
		IS_PANLE_PORT,
		NULL,
		netdev_up
	},

	{
		"enp5s0f3",
		ETH_XE,
		IS_PANLE_PORT,
		NULL,
		netdev_up
	},
	
	{
		"lan1",
		ETH_GE,
		IS_PANLE_PORT,
		iface_ethtool_poll_linkstate,
		netdev_up
	},

	{
		"lan2",
		ETH_GE,
		IS_PANLE_PORT,
		iface_ethtool_poll_linkstate,
		netdev_up
	},

	{
		"lan3",
		ETH_GE,
		IS_PANLE_PORT,
		iface_ethtool_poll_linkstate,
		netdev_up
	},
	
	{
		"lan4",
		ETH_GE,
		IS_PANLE_PORT,
		iface_ethtool_poll_linkstate,
		netdev_up
	},

	{
		"lan5",
		ETH_GE,
		IS_PANLE_PORT,
		iface_ethtool_poll_linkstate,
		netdev_up
	},

	{
		"lan6",
		ETH_GE,
		IS_PANLE_PORT,
		iface_ethtool_poll_linkstate,
		netdev_up
	},

	{
		"lan7",
		ETH_GE,
		IS_PANLE_PORT,
		iface_ethtool_poll_linkstate,
		netdev_up
	},

	{
		"lan8",
		ETH_GE,
		IS_PANLE_PORT,
		iface_ethtool_poll_linkstate,
		netdev_up
	}
};


void register_ports(void)
{
	int i;
	struct port_t *entry;
	vlib_port_main_t *vp = &vlib_port_main;
	int n_ports_now = vec_count(vp->entry_vec);

	/** SW<->CPU ports, only one. */
	for (i = 0; i < (int)DIM(iface_list); i ++) {

		if (netdev_exist(iface_list[i].if_name) != 1) {
			continue;
		}
		
		port_entry_new (&entry, &iface_list[i]);
		if (!entry) {
			oryx_panic(-1, 
				"Can not alloc memory for a port");
		}
		
		entry->ul_id = n_ports_now + i;
		port_table_entry_add (entry);
	}

}


static __oryx_always_inline__
void port_activity_prob_tmr_handler(struct oryx_timer_t __oryx_unused__*tmr,
			int __oryx_unused__ argc, char __oryx_unused__**argv)
{
	vlib_port_main_t *vp = &vlib_port_main;	
	int foreach_element;
	struct port_t *this;

	vec_foreach_element(vp->entry_vec, foreach_element, this){
		if (likely(this)) {
			if (this->ethdev_state_poll)
				this->ethdev_state_poll(this);

			if(this->ul_flags & NETDEV_POLL_UP) {
				/** up this interface right now. */
				if(this->ethdev_up) {
					this->ethdev_up(this->sc_alias_fixed);
				} else {
					oryx_loge(-1,
						"ethdev up driver is not registered, this port will down forever.");
				}
			}
		}
	}
}

void port_init(vlib_main_t *vm)
{

	vlib_port_main_t *vp = &vlib_port_main;

	vp->link_detect_tmr_interval = 3;
	vp->htable = oryx_htable_init(DEFAULT_HASH_CHAIN_SIZE, 
			port_hval, port_cmp, port_free, 0);
	vp->entry_vec = vec_init (MAX_PORTS);
	
	if (vp->htable == NULL || vp->entry_vec == NULL) {
		printf ("vlib port main init error!\n");
		exit(0);
	}
	    
	install_element (CONFIG_NODE, &show_interface_cmd);
	install_element (CONFIG_NODE, &show_interfacce_stats_cmd);
	install_element (CONFIG_NODE, &interface_alias_cmd);
	install_element (CONFIG_NODE, &interface_mtu_cmd);
	install_element (CONFIG_NODE, &interface_looback_cmd);
	install_element (CONFIG_NODE, &interface_stats_clear_cmd);

	uint32_t ul_activity_tmr_setting_flags = TMR_OPTIONS_PERIODIC | TMR_OPTIONS_ADVANCED;

	vp->link_detect_tmr = oryx_tmr_create(1, "port activity monitoring tmr", 
							ul_activity_tmr_setting_flags,
							port_activity_prob_tmr_handler,
							0, NULL, vp->link_detect_tmr_interval);

	if(likely(vp->link_detect_tmr))
		oryx_tmr_start(vp->link_detect_tmr);

	register_ports();

	vm->ul_flags |= VLIB_PORT_INITIALIZED;
}


