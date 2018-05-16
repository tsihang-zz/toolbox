#include "oryx.h"
#include "dp_decode.h"

/**
 * \brief Function to decode GRE packets
 */

int DecodeARP0(ThreadVars *tv, DecodeThreadVars *dtv, Packet *p, uint8_t *pkt, uint16_t len, PacketQueue *pq)
{
    uint16_t header_len = GRE_HDR_LEN;

	oryx_logd("ARP");

    StatsIncr(tv, dtv->counter_arp);

}

