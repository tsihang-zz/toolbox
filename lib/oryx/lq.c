/*!
 * @file lq.c
 * @date 2017/08/29
 *
 * TSIHANG (haechime@gmail.com)
 */

#include "oryx.h"

#if defined(LQ_ENABLE_PASSIVE)
static void
_wakeup 
(
	IN void *lq
)
{
	struct oryx_lq_ctx_t *q = (struct oryx_lq_ctx_t *)lq;
	//fprintf (stdout, "wakeup...\n");
	oryx_sys_mutex_lock(&q->cond_mtx);
	oryx_sys_cond_wake(&q->cond);
	oryx_sys_mutex_unlock(&q->cond_mtx);
}

static void
_hangon 
(
	IN void *lq
)
{
	struct oryx_lq_ctx_t *q = (struct oryx_lq_ctx_t *)lq;
	//fprintf (stdout, "Trying hangup %d...\n", lq_blocked_len(lq));
	if (lq_blocked_len(q) == 0) {
		{
			//fprintf (stdout, "hangup ...\n");
			oryx_sys_mutex_lock(&q->cond_mtx);
			oryx_sys_cond_wait(&q->cond, &q->cond_mtx);
			oryx_sys_mutex_unlock(&q->cond_mtx);
		}
	}
}
#endif

static struct oryx_lq_ctx_t *
list_queue_init 
(
	IN const char *fq_name,
	IN uint32_t fq_cfg,
	IN struct oryx_lq_ctx_t *lq
)
{
    BUG_ON (lq == NULL);
	
    memset(lq, 0, sizeof(struct oryx_lq_ctx_t));
	lq->name		=	fq_name;
	lq->ul_flags	=	fq_cfg;
	lq->bot			=	NULL;
	lq->top			=	NULL;
	lq->len			=	0;

	oryx_sys_mutex_create(&lq->mtx);


#if defined(LQ_ENABLE_PASSIVE)
	fprintf (stdout, "init passive queue... %d\n", lq_type_blocked(lq));
	if(lq_type_blocked(lq)) {
		oryx_sys_mutex_create(&lq->cond_mtx);
		oryx_sys_cond_create(&lq->cond);
		lq->fn_hangon	=	_hangon;
		lq->fn_wakeup	=	_wakeup;
	}
#endif

    return lq;
}

/**
 *  \brief Create a new queue
 *
 *  \param fq_name, the queue alias
 *  \param flags, configurations
 *  \param lq, the q
 */
__oryx_always_extern__
int oryx_lq_new
(
	IN const char *fq_name,
	IN uint32_t fq_cfg,
	OUT void ** lq
)
{
    struct oryx_lq_ctx_t *q = (struct oryx_lq_ctx_t *)malloc(sizeof(struct oryx_lq_ctx_t));
    if (q == NULL)
		oryx_panic(0,
			"malloc: %s", oryx_safe_strerror(errno));
    (*lq) = list_queue_init(fq_name, fq_cfg, q);
	
    return 0;
}

/**
 *  \brief Destroy a queue
 *
 *  \param q the queue to destroy
 */
 __oryx_always_extern__
void oryx_lq_destroy 
(
	IN void * lq
)
{
	struct oryx_lq_ctx_t *q = (struct oryx_lq_ctx_t *)lq;

	/* free all equeued elements. */

	/* destroy mutex */
	oryx_sys_mutex_destroy(&q->mtx);

	/* destroy cond mutex */
	oryx_sys_mutex_destroy(&q->cond_mtx);
}

/**
 *  \brief Display a queue
 *
 *  \param q the queue to display
 */
__oryx_always_extern__
void oryx_lq_dump
(
	IN void *lq
)
{	
	struct oryx_lq_ctx_t *q = (struct oryx_lq_ctx_t *)lq;
	fprintf (stdout, "%16s%32s(%p)\n", "qname: ",	q->name, q);
	fprintf (stdout, "%16s%32d\n", "qlen: ",	q->len);
	fprintf (stdout, "%16s%32d\n", "qcfg: ",	q->ul_flags);
}

