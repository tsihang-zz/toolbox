#ifndef DP_DECODE_SCTP_H
#define DP_DECODE_SCTP_H

#define SCTP_GET_SRC_PORT(p)                  SCTP_GET_RAW_SRC_PORT(p->sctph)
#define SCTP_GET_DST_PORT(p)                  SCTP_GET_RAW_DST_PORT(p->sctph)

#define CLEAR_SCTP_PACKET(p) { \
    (p)->sctph = NULL; \
} while (0)


int DecodeSCTP0(ThreadVars *tv, DecodeThreadVars *dtv, Packet *p, uint8_t *pkt, uint16_t len, PacketQueue *pq);

#endif

