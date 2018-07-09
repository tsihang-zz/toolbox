#ifndef QUEUE_H
#define QUEUE_H

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

/* Define a queue for storing flows */
struct qctx_t {
    void		*top;
    void		*bot;
	const char	*name;
    uint32_t	len;

	int (*insert)(struct qctx_t *q, void *f);
	int (*remove)(struct qctx_t *q, void *f);
	int (*verify)(struct qctx_t *q, void *v);
	void (*init)(struct qctx_t *q, void *v);
	void (*deinit)(struct qctx_t *q, void *v);

#ifdef DBG_PERF
    uint32_t dbg_maxlen;
#endif /* DBG_PERF */
    os_mutex_t m;
};

struct q_prefix_t {
    void *lnext;	/* list */
    void *lprev;
};

extern int fq_new(const char *fq_name, struct qctx_t ** fq);
extern void fq_destroy (struct qctx_t * fq);
extern void fq_dump(struct qctx_t *fq);

/**
 *  \brief add an instance to a queue
 *
 *  \param q queue
 *  \param f instance
 */
static __oryx_always_inline__
void fq_equeue (struct qctx_t * q, void * f)
{
#if defined(BUILD_DEBUG)
    BUG_ON(q == NULL || f == NULL);
#endif

    FQLOCK_LOCK(q);

    if (q->top != NULL) {
        ((struct q_prefix_t *)f)->lnext = q->top;
        ((struct q_prefix_t *)q->top)->lprev = f;
        q->top = f;
    } else {
        q->top = f;
        q->bot = f;
    }
	
	q->len ++;
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
static __oryx_always_inline__
void * fq_dequeue (struct qctx_t *q)
{
#if defined(BUILD_DEBUG)
	BUG_ON(q == NULL);
#endif

    FQLOCK_LOCK(q);

    void *f = q->bot;
    if (f == NULL) {
        FQLOCK_UNLOCK(q);
        return NULL;
    }

    if (((struct q_prefix_t *)q->bot)->lprev != NULL) {
        q->bot = ((struct q_prefix_t *)q->bot)->lprev;
        ((struct q_prefix_t *)q->bot)->lnext = NULL;
    } else {
        q->top = NULL;
        q->bot = NULL;
    }

	((struct q_prefix_t *)f)->lnext = NULL;
    ((struct q_prefix_t *)f)->lprev = NULL;

#if defined(BUILD_DEBUG)
    BUG_ON(q->len == 0);
#endif
    if (q->len > 0)
        q->len--;	

    FQLOCK_UNLOCK(q);
    return f;
}


#endif
