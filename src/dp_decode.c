#include "oryx.h"#include "dp_decode.h"void DecodeUpdateCounters(ThreadVars *tv,                                const DecodeThreadVars *dtv, const Packet *p){    oryx_counter_inc(&tv->perf_private_ctx0, dtv->counter_pkts);    oryx_counter_add(&tv->perf_private_ctx0, dtv->counter_bytes, GET_PKT_LEN(p));    oryx_counter_add(&tv->perf_private_ctx0, dtv->counter_avg_pkt_size, GET_PKT_LEN(p));    oryx_counter_add(&tv->perf_private_ctx0, dtv->counter_max_pkt_size, GET_PKT_LEN(p));}								
/**
 * \brief Return a malloced packet.
 */static
void PacketFree(Packet *p)
{
    p = p;
}

/**
 * \brief Get a malloced packet.
 *
 * \retval p packet, NULL on error
 */
Packet *PacketGetFromAlloc(void)
{
    Packet *p = malloc(DEFAULT_PACKET_SIZE);
    if (unlikely(p == NULL)) {
        return NULL;
    }

    memset(p, 0, DEFAULT_PACKET_SIZE);
    //PACKET_INITIALIZE(p);
    p->flags |= PKT_ALLOC;

    //PACKET_PROFILING_START(p);
    return p;
}

/**
 * \brief Finalize decoding of a packet
 *
 * This function needs to be call at the end of decode
 * functions when decoding has been succesful.
 *
 */

void PacketDecodeFinalize(ThreadVars *tv, DecodeThreadVars *dtv, Packet *p)
{

    if (p->flags & PKT_IS_INVALID) {		 oryx_counter_inc(&tv->perf_private_ctx0, dtv->counter_invalid);
        int i = 0;
        for (i = 0; i < p->events.cnt; i++) {
            if (EVENT_IS_DECODER_PACKET_ERROR(p->events.events[i])) {				 oryx_counter_inc(&tv->perf_private_ctx0, dtv->counter_invalid_events[p->events.events[i]]);
            }
        }
    }
}

