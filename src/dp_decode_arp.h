#ifndef DP_DECODE_ARP_H
#define DP_DECODE_ARP_H

int DecodeARP0 (ThreadVars *tv, DecodeThreadVars *dtv, Packet *p,
                   uint8_t *pkt, uint16_t len, PacketQueue *pq);


#endif
