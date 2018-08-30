#ifndef DP_DECODE_SCTP_H
#define DP_DECODE_SCTP_H

#define SCTP_GET_SRC_PORT(p)                  SCTP_GET_RAW_SRC_PORT(p->sctph)
#define SCTP_GET_DST_PORT(p)                  SCTP_GET_RAW_DST_PORT(p->sctph)

#define CLEAR_SCTP_PACKET(p) { \
    (p)->sctph = NULL; \
} while (0)

static __oryx_always_inline__
int DecodeSCTPPacket(threadvar_ctx_t *tv, packet_t *p, uint8_t *pkt, uint16_t len)
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

static __oryx_always_inline__
int DecodeSCTP0(threadvar_ctx_t *tv, decode_threadvar_ctx_t *dtv, packet_t *p, uint8_t *pkt, uint16_t len, pq_t *pq)
{
	oryx_logd("SCTP");

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

#endif

