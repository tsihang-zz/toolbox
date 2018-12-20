#ifndef DP_H
#define DP_H


/** \brief Per thread variable structure */
typedef struct ThreadVars_ {
    pthread_t t;
	uint32_t lcore;
    char name[16];
    char *printable_name;
    char *thread_group_name;
    ATOMIC_DECLARE(uint32_t, flags);

    /** local id */
    int id;

    /* counters for this thread. */
	struct oryx_counter_ctx_t perf_private_ctx0;

	/** free function for this thread to free a packet. */
	int (*free_fn)(void *);
	
	struct ThreadVars_ *next;
    struct ThreadVars_ *prev;

	uint64_t nr_mbufs_refcnt;
	uint64_t nr_mbufs_feedback;

	/* cur_tsc@lcore. */
	uint64_t	cur_tsc;
}threadvar_ctx_t;


/** \brief Structure to hold thread specific data for all decode modules */
typedef struct DecodeThreadVars_
{
    /** Specific context for udp protocol detection (here atm) */
    //AppLayerThreadCtx *app_tctx;

    int vlan_disabled;

    /** stats/counters */
    counter_id counter_pkts;
    counter_id counter_bytes;
    counter_id counter_avg_pkt_size;
    counter_id counter_max_pkt_size;

    counter_id counter_invalid;

    counter_id counter_eth;
    counter_id counter_ipv4;
    counter_id counter_ipv6;
    counter_id counter_tcp;
    counter_id counter_udp;
    counter_id counter_icmpv4;
    counter_id counter_icmpv6;
	
    counter_id counter_sll;
    counter_id counter_raw;
    counter_id counter_null;
    counter_id counter_sctp;
    counter_id counter_ppp;
    counter_id counter_gre;
	counter_id counter_arp;
    counter_id counter_vlan;
	counter_id counter_dsa;	/** Marvell DSA. */
    counter_id counter_vlan_qinq;
    counter_id counter_ieee8021ah;
    counter_id counter_pppoe;
    counter_id counter_teredo;
    counter_id counter_mpls;
    counter_id counter_ipv4inipv6;
    counter_id counter_ipv6inipv6;
    counter_id counter_erspan;

	counter_id counter_http;
	counter_id counter_http_get;
	counter_id counter_http_post;
	
	counter_id counter_drop;

    /** frag stats - defrag runs in the context of the decoder. */
    counter_id counter_defrag_ipv4_fragments;
    counter_id counter_defrag_ipv4_reassembled;
    counter_id counter_defrag_ipv4_timeouts;
    counter_id counter_defrag_ipv6_fragments;
    counter_id counter_defrag_ipv6_reassembled;
    counter_id counter_defrag_ipv6_timeouts;
    counter_id counter_defrag_max_hit;

    counter_id counter_flow_memcap;

    counter_id counter_flow_tcp;
    counter_id counter_flow_udp;
    counter_id counter_flow_icmp4;
    counter_id counter_flow_icmp6;


    /* thread data for flow logging api: only used at forced
     * flow recycle during lookups */
    void *output_flow_thread_data;

} decode_threadvar_ctx_t;

#define OFF_IPv42PROTO (offsetof(struct ipv4_hdr, next_proto_id))
#define OFF_IPv62PROTO (offsetof(struct ipv6_hdr, proto))

#define MBUF_IPv4_2PROTO(m)	\
	rte_pktmbuf_mtod_offset((m), uint8_t *, ETHERNET_HEADER_LEN + OFF_IPv42PROTO)
#define MBUF_IPv6_2PROTO(m)	\
	rte_pktmbuf_mtod_offset((m), uint8_t *, ETHERNET_HEADER_LEN + OFF_IPv62PROTO)

#define DSA_MBUF_IPv4_2PROTO(m)	\
	rte_pktmbuf_mtod_offset((m), uint8_t *, ETHERNET_DSA_HEADER_LEN + OFF_IPv42PROTO)
#define DSA_MBUF_IPv6_2PROTO(m)	\
	rte_pktmbuf_mtod_offset((m), uint8_t *, ETHERNET_DSA_HEADER_LEN + OFF_IPv62PROTO)

typedef struct vlib_pkt_t {
	/** physical rx and tx port */
	uint32_t	dpdk_port[QUA_RXTX];
	uint32_t	panel_port[QUA_RXTX];

	/** __ntoh32__(this->dsah.dsa) */
	uint32_t	dsa;
	void *iface[QUA_RXTX];	/** rx and tx iface for this packet. */
} __attribute__((aligned(128)))vlib_pkt_t;


ORYX_DECLARE (
	void notify_dp (
		IN vlib_main_t *vm,
		IN int signum
	)
);

ORYX_DECLARE (
	void dp_check_port (
		IN vlib_main_t *vm
	)
);

ORYX_DECLARE (
	void dp_start (
		IN vlib_main_t *vm
	)
);

ORYX_DECLARE (
	void dp_stop (
		IN vlib_main_t *vm
	)
);

ORYX_DECLARE (
	void dp_stats (
		IN vlib_main_t *vm
	)
);



#endif
