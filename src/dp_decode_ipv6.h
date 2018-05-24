#ifndef DP_DECODE_IPv6_H
#define DP_DECODE_IPv6_H

#include "iph.h"

#ifndef IPPROTO_HOPOPTS
#define IPPROTO_HOPOPTS 0
#endif

#ifndef IPPROTO_IPV6
#define IPPROTO_IPV6 41
#endif

#ifndef IPPROTO_ROUTING
#define IPPROTO_ROUTING 43
#endif

#ifndef IPPROTO_FRAGMENT
#define IPPROTO_FRAGMENT 44
#endif

#ifndef IPPROTO_NONE
#define IPPROTO_NONE 59
#endif

#ifndef IPPROTO_MH
#define IPPROTO_MH 135
#endif

/* Host Identity Protocol (rfc 5201) */
#ifndef IPPROTO_HIP
#define IPPROTO_HIP 139
#endif

#ifndef IPPROTO_SHIM6
#define IPPROTO_SHIM6 140
#endif

#define IPV6_HEADER_LEN            40
#define	IPV6_MAXPACKET	           65535 /* maximum packet size */
#define IPV6_MAX_OPT               40

#define IPV6_SET_L4PROTO(p,proto)       (p)->ip6vars.l4proto = proto


/* ONLY call these functions after making sure that:
 * 1. p->ip6h is set
 * 2. p->ip6h is valid (len is correct)
 */
#define IPV6_GET_VER(p) \
    IPV6_GET_RAW_VER((p)->ip6h)
#define IPV6_GET_CLASS(p) \
    IPV6_GET_RAW_CLASS((p)->ip6h)
#define IPV6_GET_FLOW(p) \
    IPV6_GET_RAW_FLOW((p)->ip6h)
#define IPV6_GET_NH(p) \
    (IPV6_GET_RAW_NH((p)->ip6h))
#define IPV6_GET_PLEN(p) \
    IPV6_GET_RAW_PLEN((p)->ip6h)
#define IPV6_GET_HLIM(p) \
    (IPV6_GET_RAW_HLIM((p)->ip6h))
/* XXX */
#define IPV6_GET_L4PROTO(p) \
    ((p)->ip6vars.l4proto)

/** \brief get the highest proto/next header field we know */
//#define IPV6_GET_UPPER_PROTO(p)         (p)->ip6eh.ip6_exthdrs_cnt ?
//    (p)->ip6eh.ip6_exthdrs[(p)->ip6eh.ip6_exthdrs_cnt - 1].next : IPV6_GET_NH((p))

#define CLEAR_IPV6_PACKET(p) do { \
    (p)->ip6h = NULL; \
} while (0)

#define IPV6_EXTHDR_SET_FH(p)       (p)->ip6eh.fh_set = TRUE
#define IPV6_EXTHDR_ISSET_FH(p)     (p)->ip6eh.fh_set
#define IPV6_EXTHDR_SET_RH(p)       (p)->ip6eh.rh_set = TRUE
#define IPV6_EXTHDR_ISSET_RH(p)     (p)->ip6eh.rh_set


int DecodeIPv60(ThreadVars *tv, DecodeThreadVars *dtv, Packet *p, uint8_t *pkt, uint16_t len, PacketQueue *pq);

#endif

