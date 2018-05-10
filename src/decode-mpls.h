#ifndef TR_DECODE_MPLS_H
#define TR_DECODE_MPLS_H

#if !defined(HAVE_SURICATA)
#define ETHERNET_TYPE_MPLS_UNICAST   0x8847
#define ETHERNET_TYPE_MPLS_MULTICAST 0x8848

#ifndef IPPROTO_MPLS
#define IPPROTO_MPLS 137
#endif

#endif

int DecodeMPLS0(ThreadVars *tv, DecodeThreadVars *dtv, Packet *p, uint8_t *pkt,
    uint16_t len, PacketQueue *pq);

#endif