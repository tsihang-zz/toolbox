#include "oryx.h"
#include "dp_decode.h"
#include "dp_flow.h"

static int DecodeUDPPacket(ThreadVars *t, Packet *p, uint8_t *pkt, uint16_t len)
{
    if (unlikely(len < UDP_HEADER_LEN)) {
        ENGINE_SET_INVALID_EVENT(p, UDP_HLEN_TOO_SMALL);
        return -1;
    }

    p->udph = (UDPHdr *)pkt;

    if (unlikely(len < UDP_GET_LEN(p))) {
        ENGINE_SET_INVALID_EVENT(p, UDP_PKT_TOO_SMALL);
        return -1;
    }

    if (unlikely(len != UDP_GET_LEN(p))) {
        ENGINE_SET_INVALID_EVENT(p, UDP_HLEN_INVALID);
        return -1;
    }

    SET_UDP_SRC_PORT(p,&p->sp);
    SET_UDP_DST_PORT(p,&p->dp);

    p->payload = pkt + UDP_HEADER_LEN;
    p->payload_len = len - UDP_HEADER_LEN;

    p->proto = IPPROTO_UDP;

    return 0;
}

int DecodeUDP0(ThreadVars *tv, DecodeThreadVars *dtv, Packet *p, uint8_t *pkt, uint16_t len, PacketQueue *pq)
{
	SCLogDebug("UDP");

    StatsIncr(tv, dtv->counter_udp);

    if (unlikely(DecodeUDPPacket(tv, p,pkt,len) < 0)) {
        p->udph = NULL;
        return TM_ECODE_FAILED;
    }

    SCLogDebug("UDP sp: %" PRIu32 " -> dp: %" PRIu32 " - HLEN: %" PRIu32 " LEN: %" PRIu32 "",
        UDP_GET_SRC_PORT(p), UDP_GET_DST_PORT(p), UDP_HEADER_LEN, p->payload_len);

#if defined(HAVE_TEREDO_DECODE)
    if (unlikely(DecodeTeredo(tv, dtv, p, p->payload, p->payload_len, pq) == TM_ECODE_OK)) {
        /* Here we have a Teredo packet and don't need to handle app
         * layer */
        FlowSetupPacket(p);
        return TM_ECODE_OK;
    }
#endif

    FlowSetupPacket(p);

    return TM_ECODE_OK;
}

