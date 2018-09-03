#ifndef PKTVAR_PRIVATE_H
#define PKTVAR_PRIVATE_H


#include "event_private.h"

typedef struct PktVar_ {
    uint32_t id;
    struct PktVar_ *next; /* right now just implement this as a list,
                           * in the long run we have thing of something
                           * faster. */
    uint16_t key_len;
    uint16_t value_len;
    uint8_t *key;
    uint8_t *value;
} PktVar;

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


    counter_id counter_invalid_events[DECODE_EVENT_PACKET_MAX];
    /* thread data for flow logging api: only used at forced
     * flow recycle during lookups */
    void *output_flow_thread_data;

} decode_threadvar_ctx_t;


#endif

