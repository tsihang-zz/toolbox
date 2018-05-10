#include "suricata-common.h"
#include "decode.h"
#include "decode-tcp.h"
#include "decode-events.h"
#include "util-unittest.h"
#include "util-debug.h"
#include "util-optimize.h"
#include "flow.h"
#include "util-profiling.h"
#include "pkt-var.h"
#include "host.h"

#define SET_OPTS(dst, src) \
    (dst).type = (src).type; \
    (dst).len  = (src).len; \
    (dst).data = (src).data

static int DecodeTCPOptions(Packet *p, uint8_t *pkt, uint16_t len)
{
    uint8_t tcp_opt_cnt = 0;
    TCPOpt tcp_opts[TCP_OPTMAX];

    uint16_t plen = len;
    while (plen)
    {
        /* single byte options */
        if (*pkt == TCP_OPT_EOL) {
            break;
        } else if (*pkt == TCP_OPT_NOP) {
            pkt++;
            plen--;

        /* multibyte options */
        } else {
            if (plen < 2) {
                break;
            }

            /* we already know that the total options len is valid,
             * so here the len of the specific option must be bad.
             * Also check for invalid lengths 0 and 1. */
            if (unlikely(*(pkt+1) > plen || *(pkt+1) < 2)) {
                ENGINE_SET_INVALID_EVENT(p, TCP_OPT_INVALID_LEN);
                return -1;
            }

            tcp_opts[tcp_opt_cnt].type = *pkt;
            tcp_opts[tcp_opt_cnt].len  = *(pkt+1);
            if (plen > 2)
                tcp_opts[tcp_opt_cnt].data = (pkt+2);
            else
                tcp_opts[tcp_opt_cnt].data = NULL;

            /* we are parsing the most commonly used opts to prevent
             * us from having to walk the opts list for these all the
             * time. */
            switch (tcp_opts[tcp_opt_cnt].type) {
                case TCP_OPT_WS:
                    if (tcp_opts[tcp_opt_cnt].len != TCP_OPT_WS_LEN) {
                        ENGINE_SET_EVENT(p,TCP_OPT_INVALID_LEN);
                    } else {
                        if (p->tcpvars.ws.type != 0) {
                            ENGINE_SET_EVENT(p,TCP_OPT_DUPLICATE);
                        } else {
                            SET_OPTS(p->tcpvars.ws, tcp_opts[tcp_opt_cnt]);
                        }
                    }
                    break;
                case TCP_OPT_MSS:
                    if (tcp_opts[tcp_opt_cnt].len != TCP_OPT_MSS_LEN) {
                        ENGINE_SET_EVENT(p,TCP_OPT_INVALID_LEN);
                    } else {
                        if (p->tcpvars.mss.type != 0) {
                            ENGINE_SET_EVENT(p,TCP_OPT_DUPLICATE);
                        } else {
                            SET_OPTS(p->tcpvars.mss, tcp_opts[tcp_opt_cnt]);
                        }
                    }
                    break;
                case TCP_OPT_SACKOK:
                    if (tcp_opts[tcp_opt_cnt].len != TCP_OPT_SACKOK_LEN) {
                        ENGINE_SET_EVENT(p,TCP_OPT_INVALID_LEN);
                    } else {
                        if (p->tcpvars.sackok.type != 0) {
                            ENGINE_SET_EVENT(p,TCP_OPT_DUPLICATE);
                        } else {
                            SET_OPTS(p->tcpvars.sackok, tcp_opts[tcp_opt_cnt]);
                        }
                    }
                    break;
                case TCP_OPT_TS:
                    if (tcp_opts[tcp_opt_cnt].len != TCP_OPT_TS_LEN) {
                        ENGINE_SET_EVENT(p,TCP_OPT_INVALID_LEN);
                    } else {
                        if (p->tcpvars.ts_set) {
                            ENGINE_SET_EVENT(p,TCP_OPT_DUPLICATE);
                        } else {
                            uint32_t values[2];
                            memcpy(&values, tcp_opts[tcp_opt_cnt].data, sizeof(values));
                            p->tcpvars.ts_val = ntohl(values[0]);
                            p->tcpvars.ts_ecr = ntohl(values[1]);
                            p->tcpvars.ts_set = TRUE;
                        }
                    }
                    break;
                case TCP_OPT_SACK:
                    SCLogDebug("SACK option, len %u", tcp_opts[tcp_opt_cnt].len);
                    if (tcp_opts[tcp_opt_cnt].len < TCP_OPT_SACK_MIN_LEN ||
                            tcp_opts[tcp_opt_cnt].len > TCP_OPT_SACK_MAX_LEN ||
                            !((tcp_opts[tcp_opt_cnt].len - 2) % 8 == 0))
                    {
                        ENGINE_SET_EVENT(p,TCP_OPT_INVALID_LEN);
                    } else {
                        if (p->tcpvars.sack.type != 0) {
                            ENGINE_SET_EVENT(p,TCP_OPT_DUPLICATE);
                        } else {
                            SET_OPTS(p->tcpvars.sack, tcp_opts[tcp_opt_cnt]);
                        }
                    }
                    break;
            }

            pkt += tcp_opts[tcp_opt_cnt].len;
            plen -= (tcp_opts[tcp_opt_cnt].len);
            tcp_opt_cnt++;
        }
    }
    return 0;
}

static int DecodeTCPPacket(ThreadVars *tv, Packet *p, uint8_t *pkt, uint16_t len)
{
    if (unlikely(len < TCP_HEADER_LEN)) {
        ENGINE_SET_INVALID_EVENT(p, TCP_PKT_TOO_SMALL);
        return -1;
    }

    p->tcph = (TCPHdr *)pkt;

    uint8_t hlen = TCP_GET_HLEN(p);
    if (unlikely(len < hlen)) {
        ENGINE_SET_INVALID_EVENT(p, TCP_HLEN_TOO_SMALL);
        return -1;
    }

    uint8_t tcp_opt_len = hlen - TCP_HEADER_LEN;
    if (unlikely(tcp_opt_len > TCP_OPTLENMAX)) {
        ENGINE_SET_INVALID_EVENT(p, TCP_INVALID_OPTLEN);
        return -1;
    }

    if (likely(tcp_opt_len > 0)) {
        DecodeTCPOptions(p, pkt + TCP_HEADER_LEN, tcp_opt_len);
    }

    SET_TCP_SRC_PORT(p,&p->sp);
    SET_TCP_DST_PORT(p,&p->dp);

    p->proto = IPPROTO_TCP;

    p->payload = pkt + hlen;
    p->payload_len = len - hlen;

    return 0;
}

int DecodeTCP0(ThreadVars *tv, DecodeThreadVars *dtv, Packet *p, uint8_t *pkt, uint16_t len, PacketQueue *pq)
{
#if defined(HAVE_STATS_COUNTERS)
    StatsIncr(tv, dtv->counter_tcp);
#endif
    if (unlikely(DecodeTCPPacket(tv, p,pkt,len) < 0)) {
        SCLogDebug("invalid TCP packet");
        p->tcph = NULL;
        return TM_ECODE_FAILED;
    }

#ifdef DEBUG
    SCLogDebug("TCP sp: %" PRIu32 " -> dp: %" PRIu32 " - HLEN: %" PRIu32 " LEN: %" PRIu32 " %s%s%s%s%s",
        GET_TCP_SRC_PORT(p), GET_TCP_DST_PORT(p), TCP_GET_HLEN(p), len,
        TCP_HAS_SACKOK(p) ? "SACKOK " : "", TCP_HAS_SACK(p) ? "SACK " : "",
        TCP_HAS_WSCALE(p) ? "WS " : "", TCP_HAS_TS(p) ? "TS " : "",
        TCP_HAS_MSS(p) ? "MSS " : "");
#endif

    FlowSetupPacket(p);

    return TM_ECODE_OK;
}

