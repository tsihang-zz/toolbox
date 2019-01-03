#ifndef FLOW_QUEUE_H
#define FLOW_QUEUE_H

#if defined(HAVE_FLOW_MGR)

#include "flow.h"

/** Spinlocks or Mutex for the flow queues. */
//#define FQLOCK_SPIN
#define FQLOCK_MUTEX

#ifdef FQLOCK_SPIN
    #ifdef FQLOCK_MUTEX
        #error Cannot enable both FQLOCK_SPIN and FQLOCK_MUTEX
    #endif
#endif

/* Define a queue for storing flows */
typedef struct FlowQueue_
{
    Flow *top;
    Flow *bot;
    uint32_t len;
#ifdef DBG_PERF
    uint32_t dbg_maxlen;
#endif /* DBG_PERF */
#ifdef FQLOCK_MUTEX
    sys_mutex_t m;
#elif defined FQLOCK_SPIN
    os_spinlock_t s;
#else
    #error Enable FQLOCK_SPIN or FQLOCK_MUTEX
#endif
} FlowQueue;

#ifdef FQLOCK_SPIN
    #define FQLOCK_INIT(q) do_spin_init(&(q)->s, 0)
    #define FQLOCK_DESTROY(q) do_spin_destroy(&(q)->s)
    #define FQLOCK_LOCK(q) do_spin_lock(&(q)->s)
    #define FQLOCK_TRYLOCK(q) do_spin_trylock(&(q)->s)
    #define FQLOCK_UNLOCK(q) do_spin_unlock(&(q)->s)
#elif defined FQLOCK_MUTEX
    #define FQLOCK_INIT(q) pthread_mutex_init(&(q)->m, NULL)
    #define FQLOCK_DESTROY(q) do_mutex_destroy(&(q)->m)
    #define FQLOCK_LOCK(q) oryx_sys_mutex_lock(&(q)->m)
    #define FQLOCK_TRYLOCK(q) do_mutex_trylock(&(q)->m)
    #define FQLOCK_UNLOCK(q) oryx_sys_mutex_unlock(&(q)->m)
#else
    #error Enable FQLOCK_SPIN or FQLOCK_MUTEX
#endif

/** spare/unused/prealloced flows live here */
FlowQueue flow_spare_q;

/** queue to pass flows to cleanup/log thread(s) */
FlowQueue flow_recycle_q;

/* prototypes */
FlowQueue *FlowQueueNew(void);
FlowQueue *FlowQueueInit(FlowQueue *);
void FlowQueueDestroy (FlowQueue *);

void FlowEnqueue (FlowQueue *, Flow *);
Flow *FlowDequeue (FlowQueue *);

void FlowMoveToSpare(Flow *);

#endif /* end of if defined(HAVE_FLOW_MGR) */

#endif
