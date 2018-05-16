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
    uint16_t counter_pkts;
    uint16_t counter_bytes;
    uint16_t counter_avg_pkt_size;
    uint16_t counter_max_pkt_size;

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
    uint16_t counter_erspan;

    /** frag stats - defrag runs in the context of the decoder. */
    uint16_t counter_defrag_ipv4_fragments;
    uint16_t counter_defrag_ipv4_reassembled;
    uint16_t counter_defrag_ipv4_timeouts;
    uint16_t counter_defrag_ipv6_fragments;
    uint16_t counter_defrag_ipv6_reassembled;
    uint16_t counter_defrag_ipv6_timeouts;
    uint16_t counter_defrag_max_hit;

    uint16_t counter_flow_memcap;

    uint16_t counter_flow_tcp;
    uint16_t counter_flow_udp;
    uint16_t counter_flow_icmp4;
    uint16_t counter_flow_icmp6;

    uint16_t counter_invalid_events[DECODE_EVENT_PACKET_MAX];
    /* thread data for flow logging api: only used at forced
     * flow recycle during lookups */
    void *output_flow_thread_data;

#ifdef __SC_CUDA_SUPPORT__
    CudaThreadVars cuda_vars;
#endif
} DecodeThreadVars;


#endif

