#ifndef DP_DECODE_ARP_H
#define DP_DECODE_ARP_H

/**
 * \brief Function to decode GRE packets
 */

static inline int __oryx_hot__
DecodeARP0(ThreadVars *tv, DecodeThreadVars *dtv, Packet *p, uint8_t *pkt, uint16_t len, PacketQueue *pq)
{
    uint16_t header_len = GRE_HDR_LEN;

	oryx_logd("ARP");

    oryx_counter_inc(&tv->perf_private_ctx0, dtv->counter_arp);

	return 0;
}

#endif
