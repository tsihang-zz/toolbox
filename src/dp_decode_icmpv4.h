#ifndef DP_DECODE_ICMPv4_H
#define DP_DECODE_ICMPv4_H

/** marco for icmpv4 type access */
#define ICMPV4_GET_TYPE(p)      (p)->icmpv4h->type
/** marco for icmpv4 code access */
#define ICMPV4_GET_CODE(p)      (p)->icmpv4h->code

#define CLEAR_ICMPV4_PACKET(p) do { \
    (p)->level4_comp_csum = -1;     \
    PACKET_CLEAR_L4VARS((p));       \
    (p)->icmpv4h = NULL;            \
} while(0)

#define ICMPV4_HEADER_PKT_OFFSET 8

/** macro for icmpv4 "type" access */
#define ICMPV4_GET_TYPE(p)      (p)->icmpv4h->type
/** macro for icmpv4 "code" access */
#define ICMPV4_GET_CODE(p)      (p)->icmpv4h->code
/** macro for icmpv4 "csum" access */
#define ICMPV4_GET_RAW_CSUM(p)  ntohs((p)->icmpv4h->checksum)
#define ICMPV4_GET_CSUM(p)      (p)->icmpv4h->checksum

/* If message is informational */

/** macro for icmpv4 "id" access */
#define ICMPV4_GET_ID(p)        ((p)->icmpv4vars.id)
/** macro for icmpv4 "seq" access */
#define ICMPV4_GET_SEQ(p)       ((p)->icmpv4vars.seq)

/* If message is Error */

/** macro for icmpv4 embedded "protocol" access */
#define ICMPV4_GET_EMB_PROTO(p)    (p)->icmpv4vars.emb_ip4_proto
/** macro for icmpv4 embedded "ipv4h" header access */
#define ICMPV4_GET_EMB_IPV4(p)     (p)->icmpv4vars.emb_ipv4h
/** macro for icmpv4 embedded "tcph" header access */
#define ICMPV4_GET_EMB_TCP(p)      (p)->icmpv4vars.emb_tcph
/** macro for icmpv4 embedded "udph" header access */
#define ICMPV4_GET_EMB_UDP(p)      (p)->icmpv4vars.emb_udph
/** macro for icmpv4 embedded "icmpv4h" header access */
#define ICMPV4_GET_EMB_ICMPV4H(p)  (p)->icmpv4vars.emb_icmpv4h

/** macro for checking if a ICMP DEST UNREACH packet is valid for use
 *  in other parts of the engine, such as the flow engine. 
 *
 *  \warning use only _after_ the decoder has processed the packet
 */
#define ICMPV4_DEST_UNREACH_IS_VALID(p) ( \
    (!((p)->flags & PKT_IS_INVALID)) && \
    ((p)->icmpv4h != NULL) && \
    (ICMPV4_GET_TYPE((p)) == ICMP_DEST_UNREACH) && \
    (ICMPV4_GET_EMB_IPV4((p)) != NULL) && \
    ((ICMPV4_GET_EMB_TCP((p)) != NULL) || \
     (ICMPV4_GET_EMB_UDP((p)) != NULL)))

/**
 *  marco for checking if a ICMP packet is an error message or an
 *  query message.
 *
 *  \todo This check is used in the flow engine and needs to be as
 *        cheap as possible. Consider setting a bitflag at the decoder
 *        stage so we can to a bit check instead of the more expensive
 *        check below.
 */
#define ICMPV4_IS_ERROR_MSG(p) (ICMPV4_GET_TYPE((p)) == ICMP_DEST_UNREACH || \
        ICMPV4_GET_TYPE((p)) == ICMP_SOURCE_QUENCH || \
        ICMPV4_GET_TYPE((p)) == ICMP_REDIRECT || \
        ICMPV4_GET_TYPE((p)) == ICMP_TIME_EXCEEDED || \
        ICMPV4_GET_TYPE((p)) == ICMP_PARAMETERPROB)


/**
 * \brief Calculates the checksum for the ICMP packet
 *
 * \param pkt  Pointer to the start of the ICMP packet
 * \param hlen Total length of the ICMP packet(header + payload)
 *
 * \retval csum Checksum for the ICMP packet
 */
static inline uint16_t ICMPV4CalculateChecksum0(uint16_t *pkt, uint16_t tlen)
{
    uint16_t pad = 0;
    uint32_t csum = pkt[0];

    tlen -= 4;
    pkt += 2;

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

int DecodeICMPV40(ThreadVars *tv, DecodeThreadVars *dtv, Packet *p, uint8_t *pkt, uint16_t len, PacketQueue *pq);


#endif

