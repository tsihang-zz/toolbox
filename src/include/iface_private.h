#ifndef IFACE_PRIVATE_H
#define IFACE_PRIVATE_H

#include "common_private.h"
#include "appl_private.h"

#define MAX_PORTS (ET1500_N_XE_PORTS + ET1500_N_GE_PORTS)

#define NB_APPL_N_BUCKETS	(1 << 10)

#define foreach_iface_speed			\
  _(1G, "1,000 Gbps.")				\
  _(10G, "10,000 Gbps.")			\
  _(40G, "40,000 Gbps.")			\
  _(100G, "100,000 Gbps.")			\

enum speed_t {
#define _(f,s) ETH_SPEED_##f,
	foreach_iface_speed
#undef _
	ETH_SPEED,
};

#define foreach_iface_config_prefix_cmd				\
  _(SET_ALIAS, "Set interface alias.")				\
  _(SET_MTU, "10,000 Gbps.")						\
  _(SET_LINKUP, "40,000 Gbps.")						\
  _(CLEAR_STATS, "Clear interface statistics.")		\
  _(SET_LOOPBACK, "Interface loopback.")			\
  _(SET_VID, "100,000 Gbps.")


enum interface_conf_cmd {
#define _(f,s) INTERFACE_##f,
	foreach_iface_config_prefix_cmd
#undef _
	INTERFACE,
};

enum {
	COUNTER_RX,
	COUNTER_TX,
	RX_TX
};

enum {
	ETH_GE,
	ETH_XE
};

struct port_t {	
	/** A panel interface ID. Can be translated by TO_INFACE_ID(Frame.vlan) */
	u32 ul_id;
	int type;
	/** for validate checking. */
	u32 ul_vid;
	
	/** Port name. for example, Gn_b etc... can be overwritten by CLI. */
	char  sc_alias[32];

	/** fixed alias used to do linkstate poll, and can not be overwrite by CLI. */
	const char *sc_alias_fixed;
	
	/** ethernet address for this port. */
	char eth_addr[6];

#define NETDEV_ADMIN_UP           (1 << 0)	/** 0-down, 1-up */
#define NETDEV_PROMISC            (1 << 1)
#define	NETDEV_DUPLEX_FULL		  (1 << 2)	/** 0-half, 1-full */
#define NETDEV_PMD                (1 << 3)
#define	NETDEV_LOOPBACK	  		  (1 << 4)
#define NETDEV_POLL_UP			  (1 << 5)	/** poll this port up. */

	u32 ul_flags;

	/** up->down counter. */
	u32 ul_u2d_times;

	/** */
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

	void *table;

	void (*ethdev_state_poll)(struct port_t *this);
	int (*ethdev_up)(const char *if_name);

	struct CounterCtx perf_private_ctx;
	
    /** stats/counters */
    counter_id counter_pkts[RX_TX];
    counter_id counter_bytes[RX_TX];
	
    counter_id counter_eth[RX_TX];
    counter_id counter_ipv4[RX_TX];
    counter_id counter_ipv6[RX_TX];
    counter_id counter_tcp[RX_TX];
    counter_id counter_udp[RX_TX];
    counter_id counter_icmpv4[RX_TX];
    counter_id counter_icmpv6[RX_TX];
    counter_id counter_sctp[RX_TX];
	counter_id counter_arp[RX_TX];
    counter_id counter_vlan[RX_TX];
    counter_id counter_pppoe[RX_TX];
    counter_id counter_mpls[RX_TX];

};

static inline void iface_update_counters(struct port_t *p,
					counter_id id){
	oryx_counter_inc(&p->perf_private_ctx, id);
}
					
typedef struct {
	/* dpdk_ports + sw_ports */
	int ul_n_ports;
	u32 ul_flags;
	struct oryx_timer_t *link_detect_tmr;
	u32 link_detect_tmr_interval;
	u32 poll_interval;
	oryx_vector entry_vec;
	struct oryx_htable_t *htable;

	/** enp5s0f1 must be up first before lan1-lan8 up. */
	int enp5s0f1_is_up;
	
}vlib_port_main_t;

vlib_port_main_t vlib_port_main;

#define port_alias(p) ((p)->sc_alias)


#endif

