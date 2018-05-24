#ifndef DP_FLOW_H
#define DP_FLOW_H

#if defined (HAVE_FLOW_MGR)

#include "flow.h"

/* global flow config */
typedef struct FlowCnf_
{
    uint32_t hash_rand;
    uint32_t hash_size;
    uint64_t memcap;
    uint32_t max_flows;
    uint32_t prealloc;

    uint32_t timeout_new;
    uint32_t timeout_est;

    uint32_t emerg_timeout_new;
    uint32_t emerg_timeout_est;
    uint32_t emergency_recovery;

} FlowConfig;


/**
 *  \brief See if a ICMP packet belongs to a flow by comparing the embedded
 *         packet in the ICMP error packet to the flow.
 *
 *  \param f flow
 *  \param p ICMP packet
 *
 *  \retval 1 match
 *  \retval 0 no match
 */
static inline int FlowCompareICMPv4(Flow *f, const Packet *p)
{
    if (ICMPV4_DEST_UNREACH_IS_VALID(p)) {
        /* first check the direction of the flow, in other words, the client ->
         * server direction as it's most likely the ICMP error will be a
         * response to the clients traffic */
        if ((f->src.addr_data32[0] == IPV4_GET_RAW_IPSRC_U32( ICMPV4_GET_EMB_IPV4(p) )) &&
                (f->dst.addr_data32[0] == IPV4_GET_RAW_IPDST_U32( ICMPV4_GET_EMB_IPV4(p) )) &&
                f->sp == p->icmpv4vars.emb_sport &&
                f->dp == p->icmpv4vars.emb_dport &&
                f->proto == ICMPV4_GET_EMB_PROTO(p) &&
                f->recursion_level == p->recursion_level &&
                f->vlan_id[0] == p->vlan_id[0] &&
                f->vlan_id[1] == p->vlan_id[1])
        {
            return 1;

        /* check the less likely case where the ICMP error was a response to
         * a packet from the server. */
        } else if ((f->dst.addr_data32[0] == IPV4_GET_RAW_IPSRC_U32( ICMPV4_GET_EMB_IPV4(p) )) &&
                (f->src.addr_data32[0] == IPV4_GET_RAW_IPDST_U32( ICMPV4_GET_EMB_IPV4(p) )) &&
                f->dp == p->icmpv4vars.emb_sport &&
                f->sp == p->icmpv4vars.emb_dport &&
                f->proto == ICMPV4_GET_EMB_PROTO(p) &&
                f->recursion_level == p->recursion_level &&
                f->vlan_id[0] == p->vlan_id[0] &&
                f->vlan_id[1] == p->vlan_id[1])
        {
            return 1;
        }

        /* no match, fall through */
    } else {
        /* just treat ICMP as a normal proto for now */
        return CMP_FLOW(f, p);
    }

    return 0;
}

/** \brief check if a memory alloc would fit in the memcap
 *
 *  \param size memory allocation size to check
 *
 *  \retval 1 it fits
 *  \retval 0 no fit
 */
#define FLOW_CHECK_MEMCAP(size) \
    ((((uint64_t)SC_ATOMIC_GET(flow_memuse) + (uint64_t)(size)) <= flow_config.memcap))

#define COPY_TIMESTAMP(src,dst) ((dst)->tv_sec = (src)->tv_sec, (dst)->tv_usec = (src)->tv_usec)

#define FLOW_COPY_IPV4_ADDR_TO_PACKET(fa, pa) do {      \
        (pa)->family = AF_INET;                         \
        (pa)->addr_data32[0] = (fa)->addr_data32[0];    \
    } while (0)

#define FLOW_COPY_IPV6_ADDR_TO_PACKET(fa, pa) do {      \
        (pa)->family = AF_INET6;                        \
        (pa)->addr_data32[0] = (fa)->addr_data32[0];    \
        (pa)->addr_data32[1] = (fa)->addr_data32[1];    \
        (pa)->addr_data32[2] = (fa)->addr_data32[2];    \
        (pa)->addr_data32[3] = (fa)->addr_data32[3];    \
    } while (0)

/* Set the IPv4 addressesinto the Addrs of the Packet.
 * Make sure p->ip4h is initialized and validated.
 *
 * We set the rest of the struct to 0 so we can
 * prevent using memset. */
#define FLOW_SET_IPV4_SRC_ADDR_FROM_PACKET(p, a) do {             \
        (a)->addr_data32[0] = (uint32_t)(p)->ip4h->s_ip_src.s_addr; \
        (a)->addr_data32[1] = 0;                                  \
        (a)->addr_data32[2] = 0;                                  \
        (a)->addr_data32[3] = 0;                                  \
    } while (0)

#define FLOW_SET_IPV4_DST_ADDR_FROM_PACKET(p, a) do {             \
        (a)->addr_data32[0] = (uint32_t)(p)->ip4h->s_ip_dst.s_addr; \
        (a)->addr_data32[1] = 0;                                  \
        (a)->addr_data32[2] = 0;                                  \
        (a)->addr_data32[3] = 0;                                  \
    } while (0)

/* clear the address structure by setting all fields to 0 */
#define FLOW_CLEAR_ADDR(a) do {  \
        (a)->addr_data32[0] = 0; \
        (a)->addr_data32[1] = 0; \
        (a)->addr_data32[2] = 0; \
        (a)->addr_data32[3] = 0; \
    } while (0)

/* Set the IPv6 addressesinto the Addrs of the Packet.
 * Make sure p->ip6h is initialized and validated. */
#define FLOW_SET_IPV6_SRC_ADDR_FROM_PACKET(p, a) do {   \
        (a)->addr_data32[0] = (p)->ip6h->s_ip6_src[0];  \
        (a)->addr_data32[1] = (p)->ip6h->s_ip6_src[1];  \
        (a)->addr_data32[2] = (p)->ip6h->s_ip6_src[2];  \
        (a)->addr_data32[3] = (p)->ip6h->s_ip6_src[3];  \
    } while (0)

#define FLOW_SET_IPV6_DST_ADDR_FROM_PACKET(p, a) do {   \
        (a)->addr_data32[0] = (p)->ip6h->s_ip6_dst[0];  \
        (a)->addr_data32[1] = (p)->ip6h->s_ip6_dst[1];  \
        (a)->addr_data32[2] = (p)->ip6h->s_ip6_dst[2];  \
        (a)->addr_data32[3] = (p)->ip6h->s_ip6_dst[3];  \
    } while (0)

#endif	/** end of if defined(HAVE_FLOW_MGR */

void FlowSetupPacket(Packet *p);


#endif	/** end of ifndef DP_FLOW_H */

