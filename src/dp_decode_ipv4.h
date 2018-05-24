#ifndef DP_DECODE_IPv4_H
#define DP_DECODE_IPv4_H

/* ONLY call these functions after making sure that:
 * 1. p->ip4h is set
 * 2. p->ip4h is valid (len is correct)
 */
#define IPV4_GET_VER(p) \
    IPV4_GET_RAW_VER((p)->ip4h)
#define IPV4_GET_HLEN(p) \
    (IPV4_GET_RAW_HLEN((p)->ip4h) << 2)
#define IPV4_GET_IPTOS(p) \
    IPV4_GET_RAW_IPTOS((p)->ip4h)
#define IPV4_GET_IPLEN(p) \
    (ntohs(IPV4_GET_RAW_IPLEN((p)->ip4h)))
#define IPV4_GET_IPID(p) \
    (ntohs(IPV4_GET_RAW_IPID((p)->ip4h)))
/* _IPV4_GET_IPOFFSET: get the content of the offset header field in host order */
#define _IPV4_GET_IPOFFSET(p) \
    (ntohs(IPV4_GET_RAW_IPOFFSET((p)->ip4h)))
/* IPV4_GET_IPOFFSET: get the final offset */
#define IPV4_GET_IPOFFSET(p) \
    (_IPV4_GET_IPOFFSET(p) & 0x1fff)
/* IPV4_GET_RF: get the RF flag. Use _IPV4_GET_IPOFFSET to save a ntohs call. */
#define IPV4_GET_RF(p) \
    (uint8_t)((_IPV4_GET_IPOFFSET((p)) & 0x8000) >> 15)
/* IPV4_GET_DF: get the DF flag. Use _IPV4_GET_IPOFFSET to save a ntohs call. */
#define IPV4_GET_DF(p) \
    (uint8_t)((_IPV4_GET_IPOFFSET((p)) & 0x4000) >> 14)
/* IPV4_GET_MF: get the MF flag. Use _IPV4_GET_IPOFFSET to save a ntohs call. */
#define IPV4_GET_MF(p) \
    (uint8_t)((_IPV4_GET_IPOFFSET((p)) & 0x2000) >> 13)
#define IPV4_GET_IPTTL(p) \
     IPV4_GET_RAW_IPTTL(p->ip4h)
#define IPV4_GET_IPPROTO(p) \
    IPV4_GET_RAW_IPPROTO((p)->ip4h)

#define CLEAR_IPV4_PACKET(p) do { \
    (p)->ip4h = NULL; \
    (p)->level3_comp_csum = -1; \
} while (0)

enum IPV4OptionFlags {
    IPV4_OPT_FLAG_EOL = 0,
    IPV4_OPT_FLAG_NOP,
    IPV4_OPT_FLAG_RR,
    IPV4_OPT_FLAG_TS,
    IPV4_OPT_FLAG_QS,
    IPV4_OPT_FLAG_LSRR,
    IPV4_OPT_FLAG_SSRR,
    IPV4_OPT_FLAG_SID,
    IPV4_OPT_FLAG_SEC,
    IPV4_OPT_FLAG_CIPSO,
    IPV4_OPT_FLAG_RTRALT,
};

/**
 * \brief Calculateor validate the checksum for the IP packet
 *
 * \param pkt  Pointer to the start of the IP packet
 * \param hlen Length of the IP header
 * \param init The current checksum if validating, 0 if generating.
 *
 * \retval csum For validation 0 will be returned for success, for calculation
 *    this will be the checksum.
 */
static inline uint16_t IPV4Checksum(uint16_t *pkt, uint16_t hlen, uint16_t init)
{
    uint32_t csum = init;

    csum += pkt[0] + pkt[1] + pkt[2] + pkt[3] + pkt[4] + pkt[6] + pkt[7] +
        pkt[8] + pkt[9];

    hlen -= 20;
    pkt += 10;

    if (hlen == 0) {
        ;
    } else if (hlen == 4) {
        csum += pkt[0] + pkt[1];
    } else if (hlen == 8) {
        csum += pkt[0] + pkt[1] + pkt[2] + pkt[3];
    } else if (hlen == 12) {
        csum += pkt[0] + pkt[1] + pkt[2] + pkt[3] + pkt[4] + pkt[5];
    } else if (hlen == 16) {
        csum += pkt[0] + pkt[1] + pkt[2] + pkt[3] + pkt[4] + pkt[5] + pkt[6] +
            pkt[7];
    } else if (hlen == 20) {
        csum += pkt[0] + pkt[1] + pkt[2] + pkt[3] + pkt[4] + pkt[5] + pkt[6] +
            pkt[7] + pkt[8] + pkt[9];
    } else if (hlen == 24) {
        csum += pkt[0] + pkt[1] + pkt[2] + pkt[3] + pkt[4] + pkt[5] + pkt[6] +
            pkt[7] + pkt[8] + pkt[9] + pkt[10] + pkt[11];
    } else if (hlen == 28) {
        csum += pkt[0] + pkt[1] + pkt[2] + pkt[3] + pkt[4] + pkt[5] + pkt[6] +
            pkt[7] + pkt[8] + pkt[9] + pkt[10] + pkt[11] + pkt[12] + pkt[13];
    } else if (hlen == 32) {
        csum += pkt[0] + pkt[1] + pkt[2] + pkt[3] + pkt[4] + pkt[5] + pkt[6] +
            pkt[7] + pkt[8] + pkt[9] + pkt[10] + pkt[11] + pkt[12] + pkt[13] +
            pkt[14] + pkt[15];
    } else if (hlen == 36) {
        csum += pkt[0] + pkt[1] + pkt[2] + pkt[3] + pkt[4] + pkt[5] + pkt[6] +
            pkt[7] + pkt[8] + pkt[9] + pkt[10] + pkt[11] + pkt[12] + pkt[13] +
            pkt[14] + pkt[15] + pkt[16] + pkt[17];
    } else if (hlen == 40) {
        csum += pkt[0] + pkt[1] + pkt[2] + pkt[3] + pkt[4] + pkt[5] + pkt[6] +
            pkt[7] + pkt[8] + pkt[9] + pkt[10] + pkt[11] + pkt[12] + pkt[13] +
            pkt[14] + pkt[15] + pkt[16] + pkt[17] + pkt[18] + pkt[19];
    }

    csum = (csum >> 16) + (csum & 0x0000FFFF);
    csum += (csum >> 16);

    return (uint16_t) ~csum;
}

int DecodeIPv40(ThreadVars *tv, DecodeThreadVars *dtv, Packet *p, uint8_t *pkt, uint16_t len, PacketQueue *pq);

#endif

