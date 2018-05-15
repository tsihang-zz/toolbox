#ifndef DP_PPPoE_H
#define DP_PPPoE_H

int DecodePPPOEDiscovery0(ThreadVars *tv, DecodeThreadVars *dtv, Packet *p, uint8_t *pkt, uint16_t len, PacketQueue *pq);
int DecodePPPOESession0(ThreadVars *tv, DecodeThreadVars *dtv, Packet *p, uint8_t *pkt, uint16_t len, PacketQueue *pq);

#endif

