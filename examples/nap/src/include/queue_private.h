#ifndef QUEUE_PRIVATE_H
#define QUEUE_PRIVATE_H

/* Packet Queue. */
typedef struct pq_t_ {
    void *top;
    void *bot;
    uint32_t len;
#ifdef DBG_PERF
    uint32_t dbg_maxlen;
#endif /* DBG_PERF */
    os_mutex_t mutex_q;
    os_cond_t cond_q;
} pq_t;


#endif


