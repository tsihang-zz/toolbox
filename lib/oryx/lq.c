#include "oryx.h"

static void
_wakeup (void *lq)
{
	struct oryx_lq_ctx_t *q = (struct oryx_lq_ctx_t *)lq;
	//fprintf (stdout, "wakeup...\n");
	do_mutex_lock (&q->cond_lock);
	do_cond_signal(&q->cond);
	do_mutex_unlock (&q->cond_lock);
}

static void
_hangup (void *lq)
{
	struct oryx_lq_ctx_t *q = (struct oryx_lq_ctx_t *)lq;
	//fprintf (stdout, "Trying hangup %d...\n", lq_blocked_len(lq));
	if (lq_blocked_len(q) == 0) {
		{
			//fprintf (stdout, "hangup ...\n");
			do_mutex_lock (&q->cond_lock);
			do_cond_wait(&q->cond, &q->cond_lock);		
			do_mutex_unlock (&q->cond_lock);
		}
	}
}

static struct oryx_lq_ctx_t *
list_queue_init (const char *fq_name,	uint32_t fq_cfg, struct oryx_lq_ctx_t *lq)
{
    BUG_ON (lq == NULL);
	
    memset(lq, 0, sizeof(struct oryx_lq_ctx_t));
	lq->name		=	fq_name;
	lq->ul_flags	=	fq_cfg;
	lq->bot			=	NULL;
	lq->top			=	NULL;
	lq->len			=	0;
    FQLOCK_INIT(lq);

#if defined(LQ_ENABLE_PASSIVE)
	fprintf (stdout, "init passive queue... %d\n", lq_type_blocked(lq));
	if(lq_type_blocked(lq)) {
		do_mutex_init(&lq->cond_lock);
		do_cond_init(&lq->cond);
		lq->fn_hangup	=	_hangup;
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
int oryx_lq_new (const char *fq_name, uint32_t fq_cfg, void ** lq)
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
void oryx_lq_destroy (void * lq)
{
	struct oryx_lq_ctx_t *q = (struct oryx_lq_ctx_t *)lq;
	FQLOCK_DESTROY(q);
}

/**
 *  \brief Display a queue
 *
 *  \param q the queue to display
 */
__oryx_always_extern__
void oryx_lq_dump(void *lq)
{	
	struct oryx_lq_ctx_t *q = (struct oryx_lq_ctx_t *)lq;
	fprintf (stdout, "%16s%32s(%p)\n", "qname: ",	q->name, q);
	fprintf (stdout, "%16s%32d\n", "qlen: ",	q->len);
	fprintf (stdout, "%16s%32d\n", "qcfg: ",	q->ul_flags);
}

