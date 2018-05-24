#include "oryx.h"
#include "dp_decode.h"
#include "dp_flow.h"

static int DecodeSCTPPacket(ThreadVars *tv, Packet *p, uint8_t *pkt, uint16_t len)
{
    if (unlikely(len < SCTP_HEADER_LEN)) {
        ENGINE_SET_INVALID_EVENT(p, SCTP_PKT_TOO_SMALL);
        return -1;
    }

    p->sctph = (SCTPHdr *)pkt;

    SET_SCTP_SRC_PORT(p,&p->sp);
    SET_SCTP_DST_PORT(p,&p->dp);

    p->payload = pkt + sizeof(SCTPHdr);
    p->payload_len = len - sizeof(SCTPHdr);

    p->proto = IPPROTO_SCTP;

    return 0;
}

int DecodeSCTP0(ThreadVars *tv, DecodeThreadVars *dtv, Packet *p, uint8_t *pkt, uint16_t len, PacketQueue *pq)
{
    oryx_counter_inc(&tv->perf_private_ctx0, dtv->counter_sctp);

    if (unlikely(DecodeSCTPPacket(tv, p,pkt,len) < 0)) {
        p->sctph = NULL;
        return TM_ECODE_FAILED;
    }

#if defined(BUILD_DEBUG)
    oryx_logd("SCTP sp: %" PRIu32 " -> dp: %" PRIu32,
        SCTP_GET_SRC_PORT(p), SCTP_GET_DST_PORT(p));
#endif

    FlowSetupPacket(p);

    return TM_ECODE_OK;
}

