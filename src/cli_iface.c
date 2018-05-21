#include "oryx.h"
#include "vty.h"
#include "command.h"
#include "prefix.h"

#include "common_private.h"
#include "map_private.h"
#include "cli_iface.h"

vlib_port_main_t vlib_port_main = {
	.lock = INIT_MUTEX_VAL,
};

#define VTY_ERROR_PORT(prefix, alias)\
	vty_out (vty, "%s(Error)%s %s port \"%s\"%s", \
		draw_color(COLOR_RED), draw_color(COLOR_FIN), prefix, alias, VTY_NEWLINE)

#define VTY_SUCCESS_PORT(prefix, v)\
	vty_out (vty, "%s(Success)%s %s port \"%s\"(%u)%s", \
		draw_color(COLOR_GREEN), draw_color(COLOR_FIN), prefix, v->sc_alias, v->ul_id, VTY_NEWLINE)

atomic_t n_intf_elements = ATOMIC_INIT(0);

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

/** if a null port specified, map_entry_output display all */
static __oryx_always_inline__
void port_entry_output (struct iface_t *port, struct vty *vty)
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
void port_entry_stat_output (struct iface_t *port, struct vty *vty)
{
	char format[256] = {0};
	u64 pkts[QUA_COUNTERS];
	u64 bytes[QUA_COUNTERS];
	int id = 0;
	
	if (unlikely (!port)) 
		return;

	id = QUA_COUNTER_RX;
	pkts[id]  = oryx_counter_get(&port->perf_private_ctx, port->counter_pkts[id]);
	bytes[id] = oryx_counter_get(&port->perf_private_ctx, port->counter_bytes[id]);
	
	id = QUA_COUNTER_TX;
	pkts[id]  = oryx_counter_get(&port->perf_private_ctx, port->counter_pkts[id]);
	bytes[id] = oryx_counter_get(&port->perf_private_ctx, port->counter_bytes[id]);

	{
		/** find this port named 'alias'. */
		vty_out (vty, "%15s(%u)", port->sc_alias, port->ul_id);

		sprintf (format, "%llu/%llu", bytes[QUA_COUNTER_RX], bytes[QUA_COUNTER_TX]);
		vty_out (vty, "%20s", format);

		sprintf (format, "%llu/%llu", pkts[QUA_COUNTER_RX], pkts[QUA_COUNTER_TX]);
		vty_out (vty, "%20s", format);

		vty_newline(vty);
	} 
}

static __oryx_always_inline__
void port_entry_config (struct iface_t *port, void __oryx_unused__ *vty_, void *arg)
{

	struct vty *vty = vty_;
	struct prefix_t *var;
	u32 ul_flags = port->ul_flags;
	vlib_port_main_t *vp = &vlib_port_main;

	var = (struct prefix_t *)arg;

	switch (var->cmd)
	{
		case INTERFACE_SET_ALIAS:
			if (strlen ((char *)var->v) > sizeof (port->sc_alias)) {
				VTY_ERROR_PORT("invalid alias", (char *)port->sc_alias);
				break;
			}
			struct iface_t *exist;/** check same alias. */
			iface_lookup_alias(vp, (const char *)var->v, &exist);
			if (exist) {
				VTY_ERROR_PORT("alias been named by a port", (char *)exist->sc_alias);
				break;
			}	
			iface_rename(vp, port, (const char *)var->v);
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

void port_table_entry_lookup (struct prefix_t *lp, 
	struct iface_t **p)
{
	vlib_port_main_t *vp = &vlib_port_main;

	ASSERT (lp);
	ASSERT (p);
	
	switch (lp->cmd) {
		case LOOKUP_ID:
			iface_lookup_id(vp, (*(u32*)lp->v), p);
			break;
		case LOOKUP_ALIAS:
			iface_lookup_alias(vp, (const char*)lp->v, p);
			break;
		default:
			break;
	}
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

int iface_poll_linkstate(struct iface_t *this)
{
	int rv;
	int if_need_up_configuration = 0;
	
	if (this->type == ETH_GE) {
		/** use ethtool. */
		rv = netdev_is_up(this->sc_alias_fixed);
		/** up -> down */
		if ((this->ul_flags & NETDEV_ADMIN_UP) && rv == 0) {
			oryx_logn("%s is down", this->sc_alias);
			this->ul_up_down_times ++;
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
	}

	return 0;
}

int iface_poll_up(struct iface_t *this)
{
	return netdev_up(this->sc_alias_fixed);
}

static const struct iface_t iface_list[] = {
	{
		"ens33",
		ETH_XE,
		!IS_PANLE_PORT,
		NULL,
		iface_poll_up
	},

	{
		"enp5s0f1",
		ETH_XE,
		!IS_PANLE_PORT,
		NULL,
		iface_poll_up
	},
	
	{
		"enp5s0f2",
		ETH_XE,
		IS_PANLE_PORT,
		NULL,
		iface_poll_up
	},

	{
		"enp5s0f3",
		ETH_XE,
		IS_PANLE_PORT,
		NULL,
		iface_poll_up
	},
	
	{
		"lan1",
		ETH_GE,
		IS_PANLE_PORT,
		iface_poll_linkstate,
		iface_poll_up
	},

	{
		"lan2",
		ETH_GE,
		IS_PANLE_PORT,
		iface_poll_linkstate,
		iface_poll_up
	},

	{
		"lan3",
		ETH_GE,
		IS_PANLE_PORT,
		iface_poll_linkstate,
		iface_poll_up
	},
	
	{
		"lan4",
		ETH_GE,
		IS_PANLE_PORT,
		iface_poll_linkstate,
		iface_poll_up
	},

	{
		"lan5",
		ETH_GE,
		IS_PANLE_PORT,
		iface_poll_linkstate,
		iface_poll_up
	},

	{
		"lan6",
		ETH_GE,
		IS_PANLE_PORT,
		iface_poll_linkstate,
		iface_poll_up
	},

	{
		"lan7",
		ETH_GE,
		IS_PANLE_PORT,
		iface_poll_linkstate,
		iface_poll_up
	},

	{
		"lan8",
		ETH_GE,
		IS_PANLE_PORT,
		iface_poll_linkstate,
		iface_poll_up
	}
};


void register_ports(void)
{
	int i;
	struct iface_t *entry;
	struct iface_t *iface;
	vlib_port_main_t *vp = &vlib_port_main;
	int n_ports_now = vec_count(vp->entry_vec);

	/** SW<->CPU ports, only one. */
	for (i = 0; i < (int)DIM(iface_list); i ++) {

		iface = &iface_list[i];
		if (!netdev_exist(iface->sc_alias_fixed)) {
			continue;
		}

		iface_alloc(&entry);
		if (!entry) {
			oryx_panic(-1, 
				"Can not alloc memory for a port");
		} else {
			memcpy(&entry->sc_alias[0], iface->sc_alias_fixed, strlen(iface->sc_alias_fixed));
			entry->sc_alias_fixed = iface->sc_alias_fixed;
			entry->type = iface->type;
			entry->if_poll_state = iface->if_poll_state;
			entry->if_poll_up = iface->if_poll_up;
			entry->ul_id = n_ports_now + i;
			if (!iface_add(vp, entry))
				oryx_logn("registering interface %s ... success", entry->sc_alias);
			else
				oryx_loge(-1, "registering interface %s ... error", entry->sc_alias);
		}
	}
}


static __oryx_always_inline__
void port_activity_prob_tmr_handler(struct oryx_timer_t __oryx_unused__*tmr,
			int __oryx_unused__ argc, char __oryx_unused__**argv)
{
	vlib_port_main_t *vp = &vlib_port_main;	
	int foreach_element;
	struct iface_t *this;

	vec_foreach_element(vp->entry_vec, foreach_element, this){
		if (likely(this)) {
			
			/** check linkstate. */
			if (this->if_poll_state)
				this->if_poll_state(this);

			/** poll a port up if need. */
			if(this->ul_flags & NETDEV_POLL_UP) {
				/** up this interface right now. */
				if(this->if_poll_up) {
					this->if_poll_up(this);
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


