#ifndef __INFACE_H__
#define __INFACE_H__

#define PORT_PREFIX	"port"
#define PANEL_N_PORTS		(ET1500_N_XE_PORTS + ET1500_N_GE_PORTS)
#define NB_APPL_N_BUCKETS	(1 << 10)
#define PORT_INVALID_ID		(PANEL_N_PORTS)

#define foreach_intf_speed				\
  _(1G, "1,000 Gbps.")						\
  _(10G, "10,000 Gbps.")						\
  _(40G, "40,000 Gbps.")						\
  _(100G, "100,000 Gbps.")					\
  
enum speed_t {
#define _(f,s) ETH_SPEED_##f,
	foreach_intf_speed
#undef _
	ETH_SPEED,
};

#define foreach_intf_config_prefix_cmd			\
  _(SET_ALIAS, "Set interface alias.")				\
  _(SET_MTU, "10,000 Gbps.")					\
  _(SET_LINKUP, "40,000 Gbps.")					\
  _(CLEAR_STATS, "Clear interface statistics.")		\
  _(SET_LOOPBACK, "Interface loopback.")		\
  _(SET_VID, "100,000 Gbps.")			
  
enum interface_conf_cmd {
#define _(f,s) INTERFACE_##f,
	foreach_intf_config_prefix_cmd
#undef _
	INTERFACE,
};

struct port_t {	
	/** A panel interface ID. Can be translated by TO_INFACE_ID(Frame.vlan) */
	u32 ul_id;
	enum {dpdk_port, sw_port} type;
	/** for validate checking. */
	u32 ul_vid;
	/** Port name. for example, Gn_b etc... */
	char  sc_alias[32];

/** Work Port or Channel Port */
#define	NB_INTF_FLAGS_NETWORK			(1 << 0 /** 0-Network port(s), 1-Tool port(s) */)
#define	NB_INTF_FLAGS_FORCEDUP			(1 << 1 /** 0-disabled, 1-enabled. */)
#define	NB_INTF_FLAGS_FULL_DUPLEX		(1 << 2 /** 0-half, 1-full */)
#define	NB_INTF_FLAGS_LINKUP			(1 << 3 /** 0-down, 1-up */)
#define	NB_INTF_FLAGS_LOOPBACK			(1 << 4)

#if defined(HAVE_DPDK)
#define DPDK_DEVICE_FLAG_ADMIN_UP           (1 << 0)
#define DPDK_DEVICE_FLAG_PROMISC            (1 << 1)
#define DPDK_DEVICE_FLAG_PMD                (1 << 2)
#define DPDK_DEVICE_FLAG_PMD_INIT_FAIL      (1 << 3)
#define DPDK_DEVICE_FLAG_MAYBE_MULTISEG     (1 << 4)
#define DPDK_DEVICE_FLAG_HAVE_SUBIF         (1 << 5)
#define DPDK_DEVICE_FLAG_HQOS               (1 << 6)
#define DPDK_DEVICE_FLAG_BOND_SLAVE         (1 << 7)
#define DPDK_DEVICE_FLAG_BOND_SLAVE_UP      (1 << 8)
	u16 nb_tx_desc;
	/* dpdk rte_mbuf rx and tx vectors, VLIB_FRAME_SIZE */
	struct rte_mbuf ***tx_vectors;	/* one per worker thread */
	struct rte_mbuf ***rx_vectors;
	/* PMD related */
	u16 tx_q_used;
	u16 rx_q_used;
	u16 nb_rx_desc;
	u16 *cpu_socket_id_by_queue;
	f64 time_last_link_update;
	/* mac address */
#endif
	/** ethernet address for this port. */
	struct ether_addr eth_addr;
	u32 ul_flags;

	u8 uc_speed;

	u16 us_mtu;

	/** 
	  * Deployed applications which holded by hlist. 
	  * An vpp_deployed_appl_t entry will be find by 
	  * hashing "appl_id%max_buckets". 
	  * And then find that entry by matching the same $appl_id
	  */
	struct hlist_head hhead[NB_APPL_N_BUCKETS];

	/** Map for this interface belong to. */
	oryx_vector belong_maps;

	struct stats_trunk_t in;
	struct stats_trunk_t out;
	struct stats_trunk_t drop;

	void *table;
};

typedef struct {
	/* dpdk_ports + sw_ports */
	int ul_n_ports;
	u32 ul_flags;
	struct oryx_timer_t *link_detect_tmr;
	u32 link_detect_tmr_interval;
	u32 poll_interval;
	oryx_vector entry_vec;
	struct oryx_htable_t *htable;
	
}vlib_port_main_t;

vlib_port_main_t vlib_port_main;

#define PORT_INPKT(port)	((port)->in.packets)
#define PORT_INBYTE(port)	((port)->in.bytes)

#define PORT_OUTPKT(port) ((port)->out.packets)
#define PORT_OUTBYTE(port) ((port)->out.bytes)

#define PORT_INPKT_INC(port,v) (PORT_INPKT(port) += v)
#define PORT_INBYTES_INC(port,v) (PORT_INBYTE(port) += (v))

#define PORT_OUTPKT_INC(port,v) (PORT_OUTPKT(port) += (v))
#define PORT_OUTBYTES_INC(port,v) (PORT_OUTBYTE(port) += (v))

#define PORT_DROPPKT_INC(port,v) 
#define PORT_DROPBYTES_INC(port,v) 

#define port_alias(p) ((p)->sc_alias)


void port_init (vlib_main_t *vm);

void port_entry_new (struct port_t **port, u32 id, int type);
void port_entry_setup(struct port_t *entry);

extern
int port_table_entry_add (struct port_t *port);

extern
void port_table_entry_lookup (struct prefix_t *lp, struct port_t **p);

#if defined(HAVE_QUAGGA)

extern atomic_t n_intf_elements;

#define split_foreach_port_func1(argv_x, func){\
	const char *split = ",";/** split tokens */\
	char *token = NULL;\
	char *save = NULL;\
	char alias_list[128] = {0};\
	int foreach_element;\
	oryx_vector vec = vlib_port_main.entry_vec;\
	struct port_t *v = NULL;\
	atomic_set(&n_intf_elements, 0);\
	if (!strcmp (alias_list, "*")) {\
		/** lookup alias with Post-Fuzzy match */\
		vector_foreach_element(vec, foreach_element, v){\
			if (v){\
				func (v);\
				atomic_inc(&n_intf_elements);\
			}\
		}\
	} else {\
		memcpy (alias_list, (char *)argv_x, strlen ((char *)argv_x));\
		token = strtok_r (alias_list, split, &save);\
		while (token) {\
			if (token) {\
				if (isalldigit (token)) {\
					/** Lookup by ID. */\
					u32 id = atoi(token);\
					struct prefix_t lp = {\
						.cmd = LOOKUP_ID,\
						.v = (void *)&id,\
						.s = strlen (token),\
					};\
					port_table_entry_lookup (&lp, &v);\
					if (!v) {\
						goto lookup_by_alias_exactly;\
					}\
				} else {\
					/** Lookup by ALIAS fuzzy. */\
					if (strchr (token, '*')){\
						goto lookup_by_alias_posted_fuzzy;\
					}\
					else {\
	lookup_by_alias_exactly:\
						token = token;\
						/** Lookup by ALIAS exactly. */\
						struct prefix_t lp = {\
							.cmd = LOOKUP_ALIAS,\
							.s = strlen (token),\
							.v = token,\
						};\
						port_table_entry_lookup (&lp, &v);\
						if (!v) {\
							goto lookup_next;\
						}\
					}\
				}\
				if (v) {\
					func (v);\
					atomic_inc(&n_intf_elements);\
					goto lookup_next;\
				}\
	lookup_by_alias_posted_fuzzy:\
				/** lookup alias with Post-Fuzzy match */\
				vector_foreach_element(vec, foreach_element, v){\
					if (v && !strncmp (v->sc_alias, token, (strlen(token) - 1/** ignore '*'  */))){\
						func (v);\
						atomic_inc(&n_intf_elements);\
					}\
				}\
				goto lookup_next;\
			}\
	lookup_next:\
			token = strtok_r (NULL, split, &save);\
		}\
	}\
}

#define split_foreach_port_func1_param1(argv_x, func, param0){\
	const char *split = ",";/** split tokens */\
	char *token = NULL;\
	char *save = NULL;\
	char alias_list[128] = {0};\
	int foreach_element;\
	oryx_vector vec = vlib_port_main.entry_vec;\
	struct port_t *v = NULL;\
	atomic_set(&n_intf_elements, 0);\
	if (!strcmp (alias_list, "*")) {\
		/** lookup alias with Post-Fuzzy match */\
		vector_foreach_element(vec, foreach_element, v){\
			if (v){\
				func (v, param0);\
				atomic_inc(&n_intf_elements);\
			}\
		}\
	} else {\
		memcpy (alias_list, (char *)argv_x, strlen ((char *)argv_x));\
		token = strtok_r (alias_list, split, &save);\
		while (token) {\
			if (token) {\
				if (isalldigit (token)) {\
					/** Lookup by ID. */\
					u32 id = atoi(token);\
					struct prefix_t lp = {\
						.cmd = LOOKUP_ID,\
						.s = strlen (token),\
						.v = (void *)&id,\
					};\
					port_table_entry_lookup (&lp, &v);\
					if (!v) {\
						goto lookup_by_alias_exactly;\
					}\
				} else {\
					/** Lookup by ALIAS fuzzy. */\
					if (strchr (token, '*')){\
						goto lookup_by_alias_posted_fuzzy;\
					}\
					else {\
	lookup_by_alias_exactly:\
						token = token;\
						/** Lookup by ALIAS exactly. */\
						struct prefix_t lp = {\
							.cmd = LOOKUP_ALIAS,\
							.s = strlen (token),\
							.v = token,\
						};\
						port_table_entry_lookup (&lp, &v);\
						if (!v) {\
							goto lookup_next;\
						}\
					}\
				}\
				if (v) {\
					func (v, param0);\
					atomic_inc(&n_intf_elements);\
					goto lookup_next;\
				}\
	lookup_by_alias_posted_fuzzy:\
				/** lookup alias with Post-Fuzzy match */\
				vector_foreach_element(vec, foreach_element, v){\
					if (v && !strncmp (v->sc_alias, token, (strlen(token) - 1/** ignore '*'  */))){\
						func (v, param0);\
						atomic_inc(&n_intf_elements);\
					}\
				}\
				goto lookup_next;\
			}\
	lookup_next:\
			token = strtok_r (NULL, split, &save);\
		}\
	}\
}


#define split_foreach_port_func1_param2(argv_x, func, param0, param_1) {\
	const char *split = ",";/** split tokens */\
	char *token = NULL;\
	char *save = NULL;\
	char alias_list[128] = {0};\
	int foreach_element;\
	oryx_vector vec = vlib_port_main.entry_vec;\
	struct port_t *v = NULL;\
	atomic_set(&n_intf_elements, 0);\
	if (!strcmp (alias_list, "*")) {\
		/** lookup alias with Post-Fuzzy match */\
		vector_foreach_element(vec, foreach_element, v){\
			if (v){\
				func (v, param0, param_1);\
				atomic_inc(&n_intf_elements);\
			}\
		}\
	} else {\
		memcpy (alias_list, (char *)argv_x, strlen ((char *)argv_x));\
		token = strtok_r (alias_list, split, &save);\
		while (token) {\
			if (token) {\
				if (isalldigit (token)) {\
					/** Lookup by ID. */\
					u32 id = atoi(token);\
					struct prefix_t lp = {\
						.cmd = LOOKUP_ID,\
						.s = strlen (token),\
						.v = (void *)&id,\
					};\
					port_table_entry_lookup (&lp, &v);\
					if (!v) {\
						goto lookup_by_alias_exactly;\
					}\
				} else {\
					/** Lookup by ALIAS fuzzy. */\
					if (strchr (token, '*')){\
						goto lookup_by_alias_posted_fuzzy;\
					}\
					else {\
	lookup_by_alias_exactly:\
						token = token;\
						/** Lookup by ALIAS exactly. */\
						struct prefix_t lp = {\
							.cmd = LOOKUP_ALIAS,\
							.s = strlen (token),\
							.v = token,\
						};\
						port_table_entry_lookup (&lp, &v);\
						if (!v) {\
							goto lookup_next;\
						}\
					}\
				}\
				if (v) {\
					func (v, param0, param_1);\
					atomic_inc(&n_intf_elements);\
					goto lookup_next;\
				}\
	lookup_by_alias_posted_fuzzy:\
				/** lookup alias with Post-Fuzzy match */\
				vector_foreach_element(vec, foreach_element, v){\
					if (v && !strncmp (v->sc_alias, token, (strlen(token) - 1/** ignore '*'  */))){\
						func (v, param0, param_1);\
						atomic_inc(&n_intf_elements);\
					}\
				}\
				goto lookup_next;\
			}\
	lookup_next:\
			token = strtok_r (NULL, split, &save);\
		}\
	}\
}

#define foreach_port_func1_param0(argv_x, func)\
	int foreach_element;\
	oryx_vector vec = vlib_port_main.entry_vec;\
	atomic_set(&n_intf_elements, 0);\
	struct port_t *v;\
	vector_foreach_element(vec, foreach_element, v){\
		if (v) {\
			func (v);\
			atomic_inc(&n_intf_elements);\
		}\
	}

#define foreach_port_func1_param1(argv_x, func, param0)\
	int foreach_element;\
	oryx_vector vec = vlib_port_main.entry_vec;\
	atomic_set(&n_intf_elements, 0);\
	struct port_t *v;\
	vector_foreach_element(vec, foreach_element, v){\
		if (v) {\
			func (v, param0);\
			atomic_inc(&n_intf_elements);\
		}\
	}

#define foreach_port_func0_param0_func1_param1(argv_x, func0, param0, func1, param1)\
	int foreach_element;\
	oryx_vector vec = vlib_port_main.entry_vec;\
	atomic_set(&n_intf_elements, 0);\
	struct port_t *v;\
	vector_foreach_element(vec, foreach_element, v){\
		if (v) {\
			func (v, param0);\
			atomic_inc(&n_intf_elements);\
		}\
	}
		
#endif


#endif
