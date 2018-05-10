#include "suricata-common.h"
#include "decode.h"
#include "decode-sctp.h"
#include "decode-events.h"
#include "util-unittest.h"
#include "util-debug.h"
#include "util-optimize.h"
#include "flow.h"

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
#if defined(HAVE_STATS_COUNTERS)
    StatsIncr(tv, dtv->counter_sctp);
#endif
    if (unlikely(DecodeSCTPPacket(tv, p,pkt,len) < 0)) {
        p->sctph = NULL;
        return TM_ECODE_FAILED;
    }

#ifdef DEBUG
    SCLogDebug("SCTP sp: %" PRIu32 " -> dp: %" PRIu32,
        SCTP_GET_SRC_PORT(p), SCTP_GET_DST_PORT(p));
#endif

    FlowSetupPacket(p);

    return TM_ECODE_OK;
}

