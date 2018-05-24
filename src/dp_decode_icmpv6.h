#ifndef DP_DECODE_ICMPv6_H
#define DP_DECODE_ICMPv6_H

/** macro for icmpv6 "type" access */
#define ICMPV6_GET_TYPE(p)      (p)->icmpv6h->type
/** macro for icmpv6 "code" access */
#define ICMPV6_GET_CODE(p)      (p)->icmpv6h->code
/** macro for icmpv6 "csum" access */
#define ICMPV6_GET_RAW_CSUM(p)      ntohs((p)->icmpv6h->csum)
#define ICMPV6_GET_CSUM(p)      (p)->icmpv6h->csum

/** If message is informational */
/** macro for icmpv6 "id" access */
#define ICMPV6_GET_ID(p)        (p)->icmpv6vars.id
/** macro for icmpv6 "seq" access */
#define ICMPV6_GET_SEQ(p)       (p)->icmpv6vars.seq

/** If message is Error */
/** macro for icmpv6 "unused" access */
#define ICMPV6_GET_UNUSED(p)       (p)->icmpv6h->icmpv6b.icmpv6e.unused
/** macro for icmpv6 "error_ptr" access */
#define ICMPV6_GET_ERROR_PTR(p)    (p)->icmpv6h->icmpv6b.icmpv6e.error_ptr
/** macro for icmpv6 "mtu" access */
#define ICMPV6_GET_MTU(p)          (p)->icmpv6h->icmpv6b.icmpv6e.mtu

/** macro for icmpv6 embedded "protocol" access */
#define ICMPV6_GET_EMB_PROTO(p)    (p)->icmpv6vars.emb_ip6_proto_next
/** macro for icmpv6 embedded "ipv6h" header access */
#define ICMPV6_GET_EMB_IPV6(p)     (p)->icmpv6vars.emb_ipv6h
/** macro for icmpv6 embedded "tcph" header access */
#define ICMPV6_GET_EMB_TCP(p)      (p)->icmpv6vars.emb_tcph
/** macro for icmpv6 embedded "udph" header access */
#define ICMPV6_GET_EMB_UDP(p)      (p)->icmpv6vars.emb_udph
/** macro for icmpv6 embedded "icmpv6h" header access */
#define ICMPV6_GET_EMB_icmpv6h(p)  (p)->icmpv6vars.emb_icmpv6h

#define CLEAR_ICMPV6_PACKET(p) do { \
    (p)->level4_comp_csum = -1;     \
    PACKET_CLEAR_L4VARS((p));       \
    (p)->icmpv6h = NULL;            \
} while(0)

/**
 * \brief Calculates the checksum for the ICMPV6 packet
 *
 * \param shdr Pointer to source address field from the IPV6 packet.  Used as a
 *             part of the psuedoheader for computing the checksum
 * \param pkt  Pointer to the start of the ICMPV6 packet
 * \param tlen Total length of the ICMPV6 packet(header + payload)
 *
 * \retval csum Checksum for the ICMPV6 packet
 */
static inline uint16_t ICMPV6CalculateChecksum0(uint16_t *shdr, uint16_t *pkt,
                                        uint16_t tlen)
{
    uint16_t pad = 0;
    uint32_t csum = shdr[0];

    csum += shdr[1] + shdr[2] + shdr[3] + shdr[4] + shdr[5] + shdr[6] +
        shdr[7] + shdr[8] + shdr[9] + shdr[10] + shdr[11] + shdr[12] +
        shdr[13] + shdr[14] + shdr[15] + htons(58 + tlen);

    csum += pkt[0];

    tlen -= 4;
    pkt += 2;

    while (tlen >= 64) {
        csum += pkt[0] + pkt[1] + pkt[2] + pkt[3] + pkt[4] + pkt[5] + pkt[6] +
            pkt[7] + pkt[8] + pkt[9] + pkt[10] + pkt[11] + pkt[12] + pkt[13] +
            pkt[14] + pkt[15] + pkt[16] + pkt[17] + pkt[18] + pkt[19] +
            pkt[20] + pkt[21] + pkt[22] + pkt[23] + pkt[24] + pkt[25] +
            pkt[26] + pkt[27] + pkt[28] + pkt[29] + pkt[30] + pkt[31];
        tlen -= 64;
        pkt += 32;
    }

    while (tlen >= 32) {
        csum += pkt[0] + pkt[1] + pkt[2] + pkt[3] + pkt[4] + pkt[5] + pkt[6] +
            pkt[7] + pkt[8] + pkt[9] + pkt[10] + pkt[11] + pkt[12] + pkt[13] +
            pkt[14] + pkt[15];
        tlen -= 32;
        pkt += 16;
    }

    while(tlen >= 8) {
        csum += pkt[0] + pkt[1] + pkt[2] + pkt[3];
        tlen -= 8;
        pkt += 4;
    }

    while(tlen >= 4) {
        csum += pkt[0] + pkt[1];
        tlen -= 4;
        pkt += 2;
    }

    while (tlen > 1) {
        csum += pkt[0];
        tlen -= 2;
        pkt += 1;
    }

    if (tlen == 1) {
        *(uint8_t *)(&pad) = (*(uint8_t *)pkt);
        csum += pad;
    }

    csum = (csum >> 16) + (csum & 0x0000FFFF);
    csum += (csum >> 16);

    return (uint16_t) ~csum;
}
										

int DecodeICMPV60(ThreadVars *tv, DecodeThreadVars *dtv, Packet *p,
                  uint8_t *pkt, uint16_t len, PacketQueue *pq);

#endif
