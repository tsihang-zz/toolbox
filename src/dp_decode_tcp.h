#ifndef DP_DECODE_TCP_H
#define DP_DECODE_TCP_H

/** macro for getting the first timestamp from the packet in host order */
#define TCP_GET_TSVAL(p)                    ((p)->tcpvars.ts_val)

/** macro for getting the second timestamp from the packet in host order. */
#define TCP_GET_TSECR(p)                    ((p)->tcpvars.ts_ecr)

#define TCP_HAS_WSCALE(p)                   ((p)->tcpvars.ws.type == TCP_OPT_WS)
#define TCP_HAS_SACK(p)                     ((p)->tcpvars.sack.type == TCP_OPT_SACK)
#define TCP_HAS_SACKOK(p)                   ((p)->tcpvars.sackok.type == TCP_OPT_SACKOK)
#define TCP_HAS_TS(p)                       ((p)->tcpvars.ts_set == TRUE)
#define TCP_HAS_MSS(p)                      ((p)->tcpvars.mss.type == TCP_OPT_MSS)

/** macro for getting the wscale from the packet. */
#define TCP_GET_WSCALE(p)                    (TCP_HAS_WSCALE((p)) ? \
                                                (((*(uint8_t *)(p)->tcpvars.ws.data) <= TCP_WSCALE_MAX) ? \
                                                  (*(uint8_t *)((p)->tcpvars.ws.data)) : 0) : 0)

#define TCP_GET_SACKOK(p)                    (TCP_HAS_SACKOK((p)) ? 1 : 0)
#define TCP_GET_SACK_PTR(p)                  TCP_HAS_SACK((p)) ? (p)->tcpvars.sack.data : NULL
#define TCP_GET_SACK_CNT(p)                  (TCP_HAS_SACK((p)) ? (((p)->tcpvars.sack.len - 2) / 8) : 0)

#define TCP_GET_OFFSET(p)                    TCP_GET_RAW_OFFSET((p)->tcph)
#define TCP_GET_X2(p)                        TCP_GET_RAW_X2((p)->tcph)
#define TCP_GET_HLEN(p)                      (TCP_GET_OFFSET((p)) << 2)
#define TCP_GET_SRC_PORT(p)                  TCP_GET_RAW_SRC_PORT((p)->tcph)
#define TCP_GET_DST_PORT(p)                  TCP_GET_RAW_DST_PORT((p)->tcph)
#define TCP_GET_SEQ(p)                       TCP_GET_RAW_SEQ((p)->tcph)
#define TCP_GET_ACK(p)                       TCP_GET_RAW_ACK((p)->tcph)
#define TCP_GET_WINDOW(p)                    TCP_GET_RAW_WINDOW((p)->tcph)
#define TCP_GET_URG_POINTER(p)               TCP_GET_RAW_URG_POINTER((p)->tcph)
#define TCP_GET_SUM(p)                       TCP_GET_RAW_SUM((p)->tcph)
#define TCP_GET_FLAGS(p)                     (p)->tcph->th_flags

#define TCP_ISSET_FLAG_FIN(p)                ((p)->tcph->th_flags & TH_FIN)
#define TCP_ISSET_FLAG_SYN(p)                ((p)->tcph->th_flags & TH_SYN)
#define TCP_ISSET_FLAG_RST(p)                ((p)->tcph->th_flags & TH_RST)
#define TCP_ISSET_FLAG_PUSH(p)               ((p)->tcph->th_flags & TH_PUSH)
#define TCP_ISSET_FLAG_ACK(p)                ((p)->tcph->th_flags & TH_ACK)
#define TCP_ISSET_FLAG_URG(p)                ((p)->tcph->th_flags & TH_URG)
#define TCP_ISSET_FLAG_RES2(p)               ((p)->tcph->th_flags & TH_RES2)
#define TCP_ISSET_FLAG_RES1(p)               ((p)->tcph->th_flags & TH_RES1)


typedef struct TCPOptSackRecord_ {
    uint32_t le;        /**< left edge, network order */
    uint32_t re;        /**< right edge, network order */
} TCPOptSackRecord;


#define CLEAR_TCP_PACKET(p) {   \
    (p)->level4_comp_csum = -1; \
    PACKET_CLEAR_L4VARS((p));   \
    (p)->tcph = NULL;           \
}

/**
 * \brief Calculate or validate the checksum for the TCP packet
 *
 * \param shdr Pointer to source address field from the IP packet.  Used as a
 *             part of the pseudoheader for computing the checksum
 * \param pkt  Pointer to the start of the TCP packet
 * \param tlen Total length of the TCP packet(header + payload)
 * \param init The current checksum if validating, 0 if generating.
 *
 * \retval csum For validation 0 will be returned for success, for calculation
 *    this will be the checksum.
 */
static inline uint16_t TCPChecksum0(uint16_t *shdr, uint16_t *pkt,
                                   uint16_t tlen, uint16_t init)
{
    uint16_t pad = 0;
    uint32_t csum = init;

    csum += shdr[0] + shdr[1] + shdr[2] + shdr[3] + htons(6) + htons(tlen);

    csum += pkt[0] + pkt[1] + pkt[2] + pkt[3] + pkt[4] + pkt[5] + pkt[6] +
        pkt[7] + pkt[9];

    tlen -= 20;
    pkt += 10;

    while (tlen >= 32) {
        csum += pkt[0] + pkt[1] + pkt[2] + pkt[3] + pkt[4] + pkt[5] + pkt[6] +
            pkt[7] +
            pkt[8] +
            pkt[9] + pkt[10] + pkt[11] + pkt[12] + pkt[13] +
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
        pkt += 1;
        tlen -= 2;
    }

    if (tlen == 1) {
        *(uint8_t *)(&pad) = (*(uint8_t *)pkt);
        csum += pad;
    }

    csum = (csum >> 16) + (csum & 0x0000FFFF);
    csum += (csum >> 16);

    return (uint16_t)~csum;
}

/**
 * \brief Calculate or validate the checksum for the TCP packet
 *
 * \param shdr Pointer to source address field from the IPV6 packet.  Used as a
 *             part of the psuedoheader for computing the checksum
 * \param pkt  Pointer to the start of the TCP packet
 * \param tlen Total length of the TCP packet(header + payload)
 * \param init The current checksum if validating, 0 if generating.
 *
 * \retval csum For validation 0 will be returned for success, for calculation
 *    this will be the checksum.
 */
static inline uint16_t TCPV6Checksum0(uint16_t *shdr, uint16_t *pkt,
                                     uint16_t tlen, uint16_t init)
{
    uint16_t pad = 0;
    uint32_t csum = init;

    csum += shdr[0] + shdr[1] + shdr[2] + shdr[3] + shdr[4] + shdr[5] +
        shdr[6] +  shdr[7] + shdr[8] + shdr[9] + shdr[10] + shdr[11] +
        shdr[12] + shdr[13] + shdr[14] + shdr[15] + htons(6) + htons(tlen);

    csum += pkt[0] + pkt[1] + pkt[2] + pkt[3] + pkt[4] + pkt[5] + pkt[6] +
        pkt[7] + pkt[9];

    tlen -= 20;
    pkt += 10;

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
        pkt += 1;
        tlen -= 2;
    }

    if (tlen == 1) {
        *(uint8_t *)(&pad) = (*(uint8_t *)pkt);
        csum += pad;
    }

    csum = (csum >> 16) + (csum & 0x0000FFFF);
    csum += (csum >> 16);

    return (uint16_t)~csum;
}


int DecodeTCP0(ThreadVars *tv, DecodeThreadVars *dtv, Packet *p, uint8_t *pkt, uint16_t len, PacketQueue *pq);


#endif

