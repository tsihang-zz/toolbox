#ifndef LQ_H
#define LQ_H

#define LQ_ENABLE_PASSIVE

#define FQLOCK_INIT(q)\
	do_mutex_init(&(q)->m)
#define FQLOCK_DESTROY(q)\
	do_mutex_destroy(&(q)->m)
#define FQLOCK_LOCK(q)\
	do_mutex_lock(&(q)->m)
#define FQLOCK_TRYLOCK(q)\
	do_mutex_trylock(&(q)->m)
#define FQLOCK_UNLOCK(q)\
	do_mutex_unlock(&(q)->m)

/** trigger method. */
#define LQ_TYPE_PASSIVE	(1 << 2)

/* all message stored in a fixed buffer (ul_cache_size). 
 * if buffer full, new arrived element is dropped.
 * count for this case is record by ul_drop_refcnt. */
#define LQ_TYPE_FIXED_BUFFER	(1 << 3)


/* Define a queue for storing flows */
struct oryx_lq_ctx_t {
    void				*top;
    void				*bot;
	const char			*name;
    uint32_t			len;
	uint64_t			nr_eq_refcnt;
	uint64_t			nr_dq_refcnt;
#ifdef DBG_PERF
    uint32_t	dbg_maxlen;
#endif /* DBG_PERF */
    os_mutex_t	m;

#if defined(LQ_ENABLE_PASSIVE)
	void (*fn_wakeup) (void *);
	void (*fn_hangup) (void *);
	os_mutex_t	cond_lock;
	os_cond_t	cond;
#endif

	uint32_t	ul_flags;
};
//}__attribute__((__packed__));

#define lq_blocked_len(lq)	((lq)->len)
#define lq_type_blocked(lq)	((lq)->ul_flags & LQ_TYPE_PASSIVE)

struct lq_prefix_t {
    void *lnext;	/* list */
    void *lprev;
};

/**
 *  \brief add an instance to a queue
 *
 *  \param q queue
 *  \param f instance
 */
static __oryx_always_inline__
void oryx_lq_enqueue (void *lq, void * f)
{
	struct oryx_lq_ctx_t *q = (struct oryx_lq_ctx_t *)lq;

//#if defined(BUILD_DEBUG)
    BUG_ON(q == NULL || f == NULL);
//#endif

    FQLOCK_LOCK(q);

    if (q->top != NULL) {
        ((struct lq_prefix_t *)f)->lnext = q->top;
        ((struct lq_prefix_t *)q->top)->lprev = f;
        q->top = f;
    } else {
        q->top = f;
        q->bot = f;
    }
	
	q->len ++;
	q->nr_eq_refcnt ++;
	
#ifdef DBG_PERF
	if (q->len > q->dbg_maxlen)
		q->dbg_maxlen = q->len;
#endif /* DBG_PERF */

	FQLOCK_UNLOCK(q);

#if defined(LQ_ENABLE_PASSIVE)
	if(lq_type_blocked(q))
		q->fn_wakeup(q);
#endif

}

/**
 *  \brief remove an element from the list queue
 *
 *  \param q queue
 *
 *  \retval f flow or NULL if empty list.
 */
static __oryx_always_inline__
void * oryx_lq_dequeue (void *lq)
{
	struct oryx_lq_ctx_t *q = (struct oryx_lq_ctx_t *)lq;

//#if defined(BUILD_DEBUG)
	BUG_ON(q == NULL);
//#endif

#if defined(LQ_ENABLE_PASSIVE)
	if(lq_type_blocked(q))
		q->fn_hangup(q);
#endif

    FQLOCK_LOCK(q);

    void *f = q->bot;
    if (f == NULL) {
        FQLOCK_UNLOCK(q);
        return NULL;
    }

	/* more elements in queue */
    if (((struct lq_prefix_t *)q->bot)->lprev != NULL) {
        q->bot = ((struct lq_prefix_t *)q->bot)->lprev;
        ((struct lq_prefix_t *)q->bot)->lnext = NULL;
    } else {
        q->top = NULL;
        q->bot = NULL;
    }

	((struct lq_prefix_t *)f)->lnext = NULL;
    ((struct lq_prefix_t *)f)->lprev = NULL;

#if defined(BUILD_DEBUG)
    BUG_ON(q->len == 0);
#endif
	q->nr_dq_refcnt ++;

    if (q->len > 0)
        q->len--;
	
    FQLOCK_UNLOCK(q);
    return f;
}

static __oryx_always_inline__
int oryx_lq_length (void *lq)
{
	struct oryx_lq_ctx_t *q = (struct oryx_lq_ctx_t *)lq;
	return q->len;
}

#endif
