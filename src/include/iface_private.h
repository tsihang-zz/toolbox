#ifndef IFACE_PRIVATE_H
#define IFACE_PRIVATE_H

#include "appl_private.h"
#include "counters_private.h"

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

    /** public counter store: counter syncs update this */
    StatsPublicThreadContext perf_public_ctx;

    /** private counter store: counter updates modify this */
    StatsPrivateThreadContext perf_private_ctx;

    /** stats/counters */
    uint16_t counter_pkts;
    uint16_t counter_bytes;

    uint16_t counter_invalid;

    uint16_t counter_eth;
    uint16_t counter_ipv4;
    uint16_t counter_ipv6;
    uint16_t counter_tcp;
    uint16_t counter_udp;
    uint16_t counter_icmpv4;
    uint16_t counter_icmpv6;

    uint16_t counter_sll;
    uint16_t counter_raw;
    uint16_t counter_null;
    uint16_t counter_sctp;
    uint16_t counter_ppp;
    uint16_t counter_gre;
	uint16_t counter_arp;
    uint16_t counter_vlan;
    uint16_t counter_vlan_qinq;
    uint16_t counter_ieee8021ah;
    uint16_t counter_pppoe;
    uint16_t counter_teredo;
    uint16_t counter_mpls;
    uint16_t counter_ipv4inipv6;
    uint16_t counter_ipv6inipv6;

};

static inline void iface_update_counters(struct port_t *p,
					uint16_t id){
	StatsPrivateThreadContext *pca = &p->perf_private_ctx;

#if defined(DEBUG)
	BUG_ON ((id < 1) || (id > pca->size));
#endif

	pca->head[id].value ++;
	pca->head[id].updates ++;
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

