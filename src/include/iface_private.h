#ifndef IFACE_PRIVATE_H
#define IFACE_PRIVATE_H

#include "appl_private.h"

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
	RX_COUNTER,
	TX_COUNTER,
	RX_TX
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

	void *table;


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
	
}vlib_port_main_t;

vlib_port_main_t vlib_port_main;

#define port_alias(p) ((p)->sc_alias)


#endif

