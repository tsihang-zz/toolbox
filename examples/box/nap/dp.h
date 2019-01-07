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
    oryx_counter_id_t counter_pkts;
    oryx_counter_id_t counter_bytes;
    oryx_counter_id_t counter_avg_pkt_size;
    oryx_counter_id_t counter_max_pkt_size;

    oryx_counter_id_t counter_invalid;

    oryx_counter_id_t counter_eth;
    oryx_counter_id_t counter_ipv4;
    oryx_counter_id_t counter_ipv6;
    oryx_counter_id_t counter_tcp;
    oryx_counter_id_t counter_udp;
    oryx_counter_id_t counter_icmpv4;
    oryx_counter_id_t counter_icmpv6;
	
    oryx_counter_id_t counter_sll;
    oryx_counter_id_t counter_raw;
    oryx_counter_id_t counter_null;
    oryx_counter_id_t counter_sctp;
    oryx_counter_id_t counter_ppp;
    oryx_counter_id_t counter_gre;
	oryx_counter_id_t counter_arp;
    oryx_counter_id_t counter_vlan;
	oryx_counter_id_t counter_dsa;	/** Marvell DSA. */
    oryx_counter_id_t counter_vlan_qinq;
    oryx_counter_id_t counter_ieee8021ah;
    oryx_counter_id_t counter_pppoe;
    oryx_counter_id_t counter_teredo;
    oryx_counter_id_t counter_mpls;
    oryx_counter_id_t counter_ipv4inipv6;
    oryx_counter_id_t counter_ipv6inipv6;
    oryx_counter_id_t counter_erspan;

	oryx_counter_id_t counter_http;
	oryx_counter_id_t counter_http_get;
	oryx_counter_id_t counter_http_post;
	
	oryx_counter_id_t counter_drop;

    /** frag stats - defrag runs in the context of the decoder. */
    oryx_counter_id_t counter_defrag_ipv4_fragments;
    oryx_counter_id_t counter_defrag_ipv4_reassembled;
    oryx_counter_id_t counter_defrag_ipv4_timeouts;
    oryx_counter_id_t counter_defrag_ipv6_fragments;
    oryx_counter_id_t counter_defrag_ipv6_reassembled;
    oryx_counter_id_t counter_defrag_ipv6_timeouts;
    oryx_counter_id_t counter_defrag_max_hit;

    oryx_counter_id_t counter_flow_memcap;

    oryx_counter_id_t counter_flow_tcp;
    oryx_counter_id_t counter_flow_udp;
    oryx_counter_id_t counter_flow_icmp4;
    oryx_counter_id_t counter_flow_icmp6;


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
