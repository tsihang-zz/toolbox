#include "oryx.h"
#include "dp_flow_queue.h"

#if defined(HAVE_FLOW_MGR)

/** spare/unused/prealloced flows live here */
FlowQueue flow_spare_q;

/** queue to pass flows to cleanup/log thread(s) */
FlowQueue flow_recycle_q;

FlowQueue *FlowQueueNew()
{
    FlowQueue *q = (FlowQueue *)malloc(sizeof(FlowQueue));
    if (q == NULL) {
        oryx_loge(0,
			"Fatal error encountered in FlowQueueNew. Exiting...");
        exit(EXIT_SUCCESS);
    }
    q = FlowQueueInit(q);
    return q;
}

FlowQueue *FlowQueueInit (FlowQueue *q)
{
    if (q != NULL) {
        memset(q, 0, sizeof(FlowQueue));
        FQLOCK_INIT(q);
    }
    return q;
}

/**
 *  \brief Destroy a flow queue
 *
 *  \param q the flow queue to destroy
 */
void FlowQueueDestroy (FlowQueue *q)
{
    FQLOCK_DESTROY(q);
}

/**
 *  \brief add a flow to a queue
 *
 *  \param q queue
 *  \param f flow
 */
void FlowEnqueue (FlowQueue *q, Flow *f)
{
#if defined(BUILD_DEBUG)
    BUG_ON(q == NULL || f == NULL);
#endif

    FQLOCK_LOCK(q);

    /* more flows in queue */
    if (q->top != NULL) {
        f->lnext = q->top;
        q->top->lprev = f;
        q->top = f;
    /* only flow */
    } else {
        q->top = f;
        q->bot = f;
    }
    q->len++;
#ifdef DBG_PERF
    if (q->len > q->dbg_maxlen)
        q->dbg_maxlen = q->len;
#endif /* DBG_PERF */
    FQLOCK_UNLOCK(q);
}

/**
 *  \brief remove a flow from the queue
 *
 *  \param q queue
 *
 *  \retval f flow or NULL if empty list.
 */
Flow *FlowDequeue (FlowQueue *q)
{
    FQLOCK_LOCK(q);

    Flow *f = q->bot;
    if (f == NULL) {
        FQLOCK_UNLOCK(q);
        return NULL;
    }

    /* more packets in queue */
    if (q->bot->lprev != NULL) {
        q->bot = q->bot->lprev;
        q->bot->lnext = NULL;
    /* just the one we remove, so now empty */
    } else {
        q->top = NULL;
        q->bot = NULL;
    }

#if defined(BUILD_DEBUG)
    BUG_ON(q->len == 0);
#endif
    if (q->len > 0)
        q->len--;

    f->lnext = NULL;
    f->lprev = NULL;

    FQLOCK_UNLOCK(q);
    return f;
}

/**
 *  \brief Transfer a flow from a queue to the spare queue
 *
 *  \param f the flow to be transfered
 *  \param q the source queue, where the flow will be removed. This queue is locked.
 *
 *  \note spare queue needs locking
 */
void FlowMoveToSpare(Flow *f)
{
    /* now put it in spare */
    FQLOCK_LOCK(&flow_spare_q);

    /* add to new queue (append) */
    f->lprev = flow_spare_q.bot;
    if (f->lprev != NULL)
        f->lprev->lnext = f;
    f->lnext = NULL;
    flow_spare_q.bot = f;
    if (flow_spare_q.top == NULL)
        flow_spare_q.top = f;

    flow_spare_q.len++;
#ifdef DBG_PERF
    if (flow_spare_q.len > flow_spare_q.dbg_maxlen)
        flow_spare_q.dbg_maxlen = flow_spare_q.len;
#endif /* DBG_PERF */

    FQLOCK_UNLOCK(&flow_spare_q);
}

#endif
