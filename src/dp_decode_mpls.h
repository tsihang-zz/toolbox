#ifndef DP_DECODE_MPLS_H
#define DP_DECODE_MPLS_H


int DecodeMPLS0(ThreadVars *tv, DecodeThreadVars *dtv, Packet *p, uint8_t *pkt,
    uint16_t len, PacketQueue *pq);

#endif
