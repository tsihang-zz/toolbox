#ifndef QUEUE_PRIVATE_H
#define QUEUE_PRIVATE_H

typedef struct PacketQueue_ {
    void *top;
    void *bot;
    uint32_t len;
#ifdef DBG_PERF
    uint32_t dbg_maxlen;
#endif /* DBG_PERF */
    os_lock_t mutex_q;
    os_cond_t cond_q;
} PacketQueue;


#endif


