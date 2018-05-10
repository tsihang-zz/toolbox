#ifndef TR_DECODE_ETHERNET_H
#define TR_DECODE_ETHERNET_H


int DecodeEthernet0 (ThreadVars *tv, DecodeThreadVars *dtv, Packet *p,
                   uint8_t *pkt, uint16_t len, PacketQueue *pq);



#endif
