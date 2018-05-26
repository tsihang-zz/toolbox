#ifndef DP_DECODE_UDP_H
#define DP_DECODE_UDP_H

#define UDP_GET_LEN(p)                       UDP_GET_RAW_LEN(p->udph)
#define UDP_GET_SRC_PORT(p)                  UDP_GET_RAW_SRC_PORT(p->udph)
#define UDP_GET_DST_PORT(p)                  UDP_GET_RAW_DST_PORT(p->udph)
#define UDP_GET_SUM(p)                       UDP_GET_RAW_SUM(p->udph)

#define CLEAR_UDP_PACKET(p) do {    \
    (p)->level4_comp_csum = -1;     \
    (p)->udph = NULL;               \
} while (0)


/**
 * \brief Calculate or valid the checksum for the UDP packet
 *
 * \param shdr Pointer to source address field from the IP packet.  Used as a
 *             part of the psuedoheader for computing the checksum
 * \param pkt  Pointer to the start of the UDP packet
 * \param hlen Total length of the UDP packet(header + payload)
 * \param init For validation this is the UDP checksum, for calculation this
 *    value should be set to 0.
 *
 * \retval csum For validation 0 will be returned for success, for calculation
 *    this will be the checksum.
 */
static __oryx_always_inline__
uint16_t UDPV4Checksum0(uint16_t *shdr, uint16_t *pkt,
                                     uint16_t tlen, uint16_t init)
{
    uint16_t pad = 0;
    uint32_t csum = init;

    csum += shdr[0] + shdr[1] + shdr[2] + shdr[3] + htons(17) + htons(tlen);

    csum += pkt[0] + pkt[1] + pkt[2];

    tlen -= 8;
    pkt += 4;

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

    uint16_t csum_u16 = (uint16_t)~csum;
    if (init == 0 && csum_u16 == 0)
        return 0xFFFF;
    else
        return csum_u16;
}

/**
 * \brief Calculate or valid the checksum for the UDP packet
 *
 * \param shdr Pointer to source address field from the IPV6 packet.  Used as a
 *             part of the psuedoheader for computing the checksum
 * \param pkt  Pointer to the start of the UDP packet
 * \param tlen Total length of the UDP packet(header + payload)
 * \param init For validation this is the UDP checksum, for calculation this
 *    value should be set to 0.
 *
 * \retval csum For validation 0 will be returned for success, for calculation
 *    this will be the checksum.
 */
static __oryx_always_inline__
uint16_t UDPV6Checksum0(uint16_t *shdr, uint16_t *pkt,
                                     uint16_t tlen, uint16_t init)
{
    uint16_t pad = 0;
    uint32_t csum = init;

    csum += shdr[0] + shdr[1] + shdr[2] + shdr[3] + shdr[4] + shdr[5] + shdr[6] +
        shdr[7] + shdr[8] + shdr[9] + shdr[10] + shdr[11] + shdr[12] +
        shdr[13] + shdr[14] + shdr[15] + htons(17) + htons(tlen);

    csum += pkt[0] + pkt[1] + pkt[2];

    tlen -= 8;
    pkt += 4;

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

    uint16_t csum_u16 = (uint16_t)~csum;
    if (init == 0 && csum_u16 == 0)
        return 0xFFFF;
    else
        return csum_u16;
}

static __oryx_always_inline__
int DecodeUDPPacket(ThreadVars *t, Packet *p, uint8_t *pkt, uint16_t len)
{
	if (unlikely(len < UDP_HEADER_LEN)) {
	 ENGINE_SET_INVALID_EVENT(p, UDP_HLEN_TOO_SMALL);
	 return -1;
 	}

	 p->udph = (UDPHdr *)pkt;

	 if (unlikely(len < UDP_GET_LEN(p))) {
		 ENGINE_SET_INVALID_EVENT(p, UDP_PKT_TOO_SMALL);
		 return -1;
	 }

	 if (unlikely(len != UDP_GET_LEN(p))) {
		 ENGINE_SET_INVALID_EVENT(p, UDP_HLEN_INVALID);
		 return -1;
	 }

	 SET_UDP_SRC_PORT(p,&p->sp);
	 SET_UDP_DST_PORT(p,&p->dp);

	 p->payload = pkt + UDP_HEADER_LEN;
	 p->payload_len = len - UDP_HEADER_LEN;

	 p->proto = IPPROTO_UDP;

	 return 0;
}
 
 static __oryx_always_inline__
 int DecodeUDP0(ThreadVars *tv, DecodeThreadVars *dtv, Packet *p, uint8_t *pkt, uint16_t len, PacketQueue *pq)
 {
	 oryx_logd("UDP");
 
	 oryx_counter_inc(&tv->perf_private_ctx0, dtv->counter_udp);
 
	 if (unlikely(DecodeUDPPacket(tv, p,pkt,len) < 0)) {
		 p->udph = NULL;
		 return TM_ECODE_FAILED;
	 }
 
	 oryx_logd("UDP sp: %" PRIu32 " -> dp: %" PRIu32 " - HLEN: %" PRIu32 " LEN: %" PRIu32 "",
		 UDP_GET_SRC_PORT(p), UDP_GET_DST_PORT(p), UDP_HEADER_LEN, p->payload_len);
 
#if defined(HAVE_TEREDO_DECODE)
	 if (unlikely(DecodeTeredo(tv, dtv, p, p->payload, p->payload_len, pq) == TM_ECODE_OK)) {
		 /* Here we have a Teredo packet and don't need to handle app
		  * layer */
		 FlowSetupPacket(p);
		 return TM_ECODE_OK;
	 }
#endif
 
	 FlowSetupPacket(p);
 
	 return TM_ECODE_OK;
 }
 
#endif

