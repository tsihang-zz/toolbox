#include "suricata-common.h"
#include "decode.h"
#include "decode-ppp.h"
#include "decode-events.h"

#include "flow.h"

#include "util-unittest.h"
#include "util-debug.h"

int DecodePPP0(ThreadVars *tv, DecodeThreadVars *dtv, Packet *p, uint8_t *pkt, uint16_t len, PacketQueue *pq)
{
#if defined(HAVE_STATS_COUNTERS)
    StatsIncr(tv, dtv->counter_ppp);
#endif
    if (unlikely(len < PPP_HEADER_LEN)) {
        ENGINE_SET_INVALID_EVENT(p, PPP_PKT_TOO_SMALL);
        return TM_ECODE_FAILED;
    }

    p->ppph = (PPPHdr *)pkt;
    if (unlikely(p->ppph == NULL))
        return TM_ECODE_FAILED;

    SCLogDebug("p %p pkt %p PPP protocol %04x Len: %" PRId32 "",
        p, pkt, ntohs(p->ppph->protocol), len);

    switch (ntohs(p->ppph->protocol))
    {
        case PPP_VJ_UCOMP:
            if (unlikely(len < (PPP_HEADER_LEN + IPV4_HEADER_LEN))) {
                ENGINE_SET_INVALID_EVENT(p,PPPVJU_PKT_TOO_SMALL);
                p->ppph = NULL;
                return TM_ECODE_FAILED;
            }

            if (likely(IPV4_GET_RAW_VER((IPV4Hdr *)(pkt + PPP_HEADER_LEN)) == 4)) {
                return DecodeIPV4(tv, dtv, p, pkt + PPP_HEADER_LEN, len - PPP_HEADER_LEN, pq);
            } else
                return TM_ECODE_FAILED;
            break;

        case PPP_IP:
            if (unlikely(len < (PPP_HEADER_LEN + IPV4_HEADER_LEN))) {
                ENGINE_SET_INVALID_EVENT(p,PPPIPV4_PKT_TOO_SMALL);
                p->ppph = NULL;
                return TM_ECODE_FAILED;
            }

            return DecodeIPV4(tv, dtv, p, pkt + PPP_HEADER_LEN, len - PPP_HEADER_LEN, pq);

            /* PPP IPv6 was not tested */
        case PPP_IPV6:
            if (unlikely(len < (PPP_HEADER_LEN + IPV6_HEADER_LEN))) {
                ENGINE_SET_INVALID_EVENT(p,PPPIPV6_PKT_TOO_SMALL);
                p->ppph = NULL;
                return TM_ECODE_FAILED;
            }

            return DecodeIPV6(tv, dtv, p, pkt + PPP_HEADER_LEN, len - PPP_HEADER_LEN, pq);

        case PPP_VJ_COMP:
        case PPP_IPX:
        case PPP_OSI:
        case PPP_NS:
        case PPP_DECNET:
        case PPP_APPLE:
        case PPP_BRPDU:
        case PPP_STII:
        case PPP_VINES:
        case PPP_HELLO:
        case PPP_LUXCOM:
        case PPP_SNS:
        case PPP_MPLS_UCAST:
        case PPP_MPLS_MCAST:
        case PPP_IPCP:
        case PPP_OSICP:
        case PPP_NSCP:
        case PPP_DECNETCP:
        case PPP_APPLECP:
        case PPP_IPXCP:
        case PPP_STIICP:
        case PPP_VINESCP:
        case PPP_IPV6CP:
        case PPP_MPLSCP:
        case PPP_LCP:
        case PPP_PAP:
        case PPP_LQM:
        case PPP_CHAP:
            ENGINE_SET_EVENT(p,PPP_UNSUP_PROTO);
            return TM_ECODE_OK;

        default:
            SCLogDebug("unknown PPP protocol: %" PRIx32 "",ntohs(p->ppph->protocol));
            ENGINE_SET_INVALID_EVENT(p, PPP_WRONG_TYPE);
            return TM_ECODE_OK;
    }

}

