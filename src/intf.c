#include "oryx.h"

#if defined(HAVE_QUAGGA)
#include "vty.h"
#include "command.h"
#include "prefix.h"
#endif

#include "et1500.h"

#define PORT_LOCK
#ifdef PORT_LOCK
static INIT_MUTEX(port_lock);
#else
#undef do_lock(lock)
#undef do_unlock(lock)
#define do_lock(lock)
#define do_unlock(lock)
#endif

#define VTY_ERROR_PORT(prefix, alias)\
	vty_out (vty, "%s(Error)%s %s port \"%s\"%s", \
		draw_color(COLOR_RED), draw_color(COLOR_FIN), prefix, alias, VTY_NEWLINE)

#define VTY_SUCCESS_PORT(prefix, v)\
	vty_out (vty, "%s(Success)%s %s port \"%s\"(%u)%s", \
		draw_color(COLOR_GREEN), draw_color(COLOR_FIN), prefix, v->sc_alias, v->ul_id, VTY_NEWLINE)

const struct rte_eth_conf port_conf_default = {
	.rxmode = {
		.split_hdr_size = 0,
		.header_split   = 0, /**< Header Split disabled */
		.hw_ip_checksum = 0, /**< IP checksum offload disabled */
		.hw_vlan_filter = 0, /**< VLAN filtering disabled */
		.jumbo_frame    = 0, /**< Jumbo Frame Support disabled */
		.hw_strip_crc   = 1, /**< CRC stripped by hardware */
	},
	.txmode = {
		.mq_mode = ETH_MQ_TX_NONE,
	},
};


static __oryx_always_inline__
void mk_alias (struct port_t *entry)
{

	switch (entry->uc_speed) {
	case ETH_SPEED_1G:
			sprintf (entry->sc_alias, "%s%u", (char *)"G0/", entry->ul_id);
			break;
	case ETH_SPEED_10G:
			sprintf (entry->sc_alias, "%s%u", (char *)"XG0/", entry->ul_id);
			break;
	case ETH_SPEED_40G:
			sprintf (entry->sc_alias, "%s%u", (char *)"4XG0/", entry->ul_id);
			break;
	case ETH_SPEED_100G:
			sprintf (entry->sc_alias, "%s%u", (char *)"10XG0/", entry->ul_id);
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

void port_entry_new (struct port_t **port, u32 id, int type)
{
	/** create an port */
	(*port) = kmalloc (sizeof (struct port_t), MPF_CLR, __oryx_unused_val__);

	ASSERT ((*port));

	(*port)->type = type;
	(*port)->ul_id = id;
	(*port)->us_mtu = 1500;
	(*port)->belong_maps = vec_init(1024);
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

	printout (vty, "%s", VTY_NEWLINE);
	/** find this port named 'alias'. */
	printout (vty, "%16s\"%s\"(%u)%s", "Port ", port->sc_alias, port->ul_id, VTY_NEWLINE);

	printout (vty, "		%16s%02X:%02X:%02X:%02X:%02X:%02X%s", 
		"Mac: ", 
		port->eth_addr.addr_bytes[0],
		port->eth_addr.addr_bytes[1],
		port->eth_addr.addr_bytes[2],
		port->eth_addr.addr_bytes[3],
		port->eth_addr.addr_bytes[4],
		port->eth_addr.addr_bytes[5],
		VTY_NEWLINE);
	printout (vty, "		%16s%s%s", 
		"Duplex: ", 
		(port->ul_flags & NB_INTF_FLAGS_FULL_DUPLEX) ? "Full" : "Half",
		VTY_NEWLINE);
	printout (vty, "		%16s%s%s", 
		"States: ", 
		(port->ul_flags & NB_INTF_FLAGS_LINKUP) ? "Up" : "Down", 
		VTY_NEWLINE);
	printout (vty, "		%16s%s%s", 
		"Type: ", 
		(port->ul_flags & NB_INTF_FLAGS_NETWORK) ? "Network" : "Tool", 
		VTY_NEWLINE);
	printout (vty, "		%16s%s%s", 
		"ForceUP: ", 
		(port->ul_flags & NB_INTF_FLAGS_FORCEDUP) ? "Yes" : "No", 
		VTY_NEWLINE);

	printout (vty, "		%16s%d%s", 
		"MTU: ", 
		port->us_mtu, 
		VTY_NEWLINE);

	printout (vty, "		%16s", "Maps: ");

	oryx_vector v = port->belong_maps;
	int actives = vec_active(v);
	if (!actives) printout (vty, "N/A%s", VTY_NEWLINE);
	else {
		int i;
		
		struct map_t *p;
		vector_foreach_element (v, i, p) {
			if (p) {
				printout (vty, "%s", p->sc_alias);
				-- actives;
				if (actives) printout (vty, ", ");
				else actives = actives;
			}
			else {p = p;}
		}
		printout (vty, "%s", VTY_NEWLINE);
	}
	printout (vty, "%16s%s", "_______________________________________________", VTY_NEWLINE);

}

	
/** if a null port specified, map_entry_output display all */
static __oryx_always_inline__
void port_entry_stat_output (struct port_t *port, struct vty *vty)
{

	char format[256] = {0};
	
	if (unlikely (!port)) 
		return;
	{

		/** find this port named 'alias'. */
		printout (vty, "%15s(%u)", port->sc_alias, port->ul_id);

		sprintf (format, "%llu/%llu", PORT_INBYTE(port), PORT_OUTBYTE(port));
		printout (vty, "%20s", format);

		sprintf (format, "%llu/%llu", PORT_INPKT(port), PORT_OUTPKT(port));
		printout (vty, "%20s", format);


		vty_newline(vty);
	} 
}

static __oryx_always_inline__
void do_port_activity_check(struct port_t *p) {

	switch (p->type) {
		
		case dpdk_port:
			break;

		case sw_port:
			break;

		default:
			break;
	}
	return;
}

static __oryx_always_inline__
void port_activity_prob_tmr_handler(struct oryx_timer_t __oryx_unused__*tmr,
			int __oryx_unused__ argc, char __oryx_unused__**argv)
{
	vlib_port_main_t *vp = &vlib_port_main;	
	int foreach_element;
	struct port_t *p;

	vector_foreach_element(vp->entry_vec, foreach_element, p){
		if (likely(p)) {
			do_port_activity_check(p);
		}
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
			ul_flags |= NB_INTF_FLAGS_LOOPBACK;
			if (!strncmp ((char *)var->v, "d", 1))
				ul_flags &= ~NB_INTF_FLAGS_LOOPBACK;
			port->ul_flags = ul_flags;
			break;
			
		case INTERFACE_CLEAR_STATS:
			PORT_INBYTE(port) = PORT_INPKT(port) = 0;
			PORT_OUTBYTE(port) = PORT_OUTPKT(port) = 0;
			break;
			
		default:
			break;
	}
}

static dpdk_device_config_t *id2_devconf(u32 ul_id)
{
	dpdk_main_t *dm = &dpdk_main;
	return dm->conf->dev_confs + (ul_id % dm->conf->device_config_index_by_pci_addr);
}

void port_entry_setup(struct port_t *entry)
{
	dpdk_main_t *dm = &dpdk_main;
	dpdk_device_config_t *dev_conf;
	int ret;
	
	switch (entry->type) {
		case dpdk_port:

			dev_conf = id2_devconf(entry->ul_id);
			if (!dev_conf) {
				printf ("No device config for this port %d\n", entry->ul_id);
				exit (0);
			}
			
			printf("Initializing port %u... ", (unsigned) entry->ul_id);
			fflush(stdout);
			/** */
			ret = rte_eth_dev_configure (entry->ul_id, 		/** port id */
								dev_conf->num_rx_queues,	/** nb_rx_q */
								dev_conf->num_tx_queues,	/** nb_tx_q */
								&port_conf_default);		/** devconf */
			if (ret < 0) {
				rte_exit(EXIT_FAILURE, "Cannot configure device: err=%d, port=%u\n",
					  ret, (unsigned) entry->ul_id);
			}
			/** MAC */
			rte_eth_macaddr_get(entry->ul_id, &entry->eth_addr);

#if 1
			printf ("socket ID %d\n", rte_eth_dev_socket_id(entry->ul_id));
			/* init one RX queue */
			fflush(stdout);
			ret = rte_eth_rx_queue_setup (entry->ul_id,		/** port id */
							0,								/** rx_queue_id */
							dev_conf->num_rx_desc,			/** nb_rx_desc */
							rte_eth_dev_socket_id(entry->ul_id),/** socket id */
							NULL,							/** rx_conf */
							dm->pktmbuf_pools); /** mempool */
			if (ret < 0) {
				rte_exit(EXIT_FAILURE, "rte_eth_rx_queue_setup:err=%d, port=%u\n",
					  ret, (unsigned) entry->ul_id);
			}

			/* init one TX queue on each port */
			fflush(stdout);
			ret = rte_eth_tx_queue_setup(entry->ul_id, 		/** port id */
							0, 								/** tx_queue_id */
							dev_conf->num_tx_desc,			/** nb_tx_desc */
							rte_eth_dev_socket_id(entry->ul_id),/** socket id */
							NULL);							/** tx_conf */
			if (ret < 0) {
				rte_exit(EXIT_FAILURE, "rte_eth_tx_queue_setup:err=%d, port=%u\n",
					ret, (unsigned) entry->ul_id);
			}

#endif

			/* Start device */
			ret = rte_eth_dev_start(entry->ul_id);
			if (ret < 0) {
				rte_exit(EXIT_FAILURE, "rte_eth_dev_start:err=%d, port=%u\n",
					  ret, (unsigned) entry->ul_id);
			}
			rte_eth_promiscuous_enable(entry->ul_id);

			printf("done: \n");
	
			/** LinkS */		
			struct rte_eth_link link;	/** defined in rte_ethdev.h */
			rte_eth_link_get_nowait(entry->ul_id, &link);
			if (link.link_status) {
				printf("Port %d Link Up - speed %u "
					"Mbps - %s\n", (uint8_t)entry->ul_id,
					(unsigned)link.link_speed,
					(link.link_duplex == ETH_LINK_FULL_DUPLEX) ?
					("full-duplex") : ("half-duplex\n"));
			} else {
				printf("Port %d Link Down\n",
					(uint8_t)entry->ul_id);
			}
			printf("Port %u, MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n",
				(unsigned) entry->ul_id,
				entry->eth_addr.addr_bytes[0],
				entry->eth_addr.addr_bytes[1],
				entry->eth_addr.addr_bytes[2],
				entry->eth_addr.addr_bytes[3],
				entry->eth_addr.addr_bytes[4],
				entry->eth_addr.addr_bytes[5]);

			break;

		case sw_port:
			break;

		default:
			return;
	}

	/** make port alias. */
	mk_alias (entry);
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
    
__oryx_always_extern__
int port_table_entry_add (struct port_t *port)
{
	vlib_port_main_t *vp = &vlib_port_main;

	ASSERT (port);

	do_lock (&port_lock);
	int r = oryx_htable_add(vp->htable, port->sc_alias, strlen((const char *)port->sc_alias));
	if (r == 0) {
		vec_set_index (vp->entry_vec, port->ul_id, port);
		//port->table = TABLE(panel_port);
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

#if defined(HAVE_QUAGGA)

atomic_t n_intf_elements = ATOMIC_INIT(0);

#define PRINT_SUMMARY	\
	printout (vty, "matched %d element(s), total %d element(s)%s", \
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
	printout (vty, "Trying to display %s%d%s elements ...%s", 
		draw_color(COLOR_RED), vec_active(vp->entry_vec), draw_color(COLOR_FIN), 
		VTY_NEWLINE);
	
	printout (vty, "%16s%s", "_______________________________________________", VTY_NEWLINE);

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
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR)
{
	vlib_port_main_t *vp = &vlib_port_main;
	printout (vty, "Trying to display %d elements ...%s", 
			vec_active(vp->entry_vec), VTY_NEWLINE);
	printout (vty, "%15s%20s%20s%s", "Port", "Bytes(I/O)", "Packets(I/O)", VTY_NEWLINE);
	
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

#endif

void port_init (vlib_main_t *vm)
{

	vlib_port_main_t *vp = &vlib_port_main;

	vp->link_detect_tmr_interval = 3;
	vp->htable = oryx_htable_init(DEFAULT_HASH_CHAIN_SIZE, 
			port_hval, port_cmp, port_free, 0);
	vp->entry_vec = vector_init (PANEL_N_PORTS);
	
	if (vp->htable == NULL || vp->entry_vec == NULL) {
		printf ("vlib port main init error!\n");
		exit(0);
	}
	    
#if defined(HAVE_QUAGGA)
	install_element (CONFIG_NODE, &show_interface_cmd);
	install_element (CONFIG_NODE, &show_interfacce_stats_cmd);
	install_element (CONFIG_NODE, &interface_alias_cmd);
	install_element (CONFIG_NODE, &interface_mtu_cmd);
	install_element (CONFIG_NODE, &interface_looback_cmd);
	install_element (CONFIG_NODE, &interface_stats_clear_cmd);
#endif

	uint32_t ul_activity_tmr_setting_flags = TMR_OPTIONS_PERIODIC | TMR_OPTIONS_ADVANCED;

	vp->link_detect_tmr = oryx_tmr_create(1, "port activity monitoring tmr", 
							ul_activity_tmr_setting_flags,
							port_activity_prob_tmr_handler, 
							0, NULL, vp->link_detect_tmr_interval);

	if(likely(vp->link_detect_tmr))
		oryx_tmr_start(vp->link_detect_tmr);


}


