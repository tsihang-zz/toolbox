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

struct element_fn_t {
	int (*insert)(struct qctx_t *q, void *v);
	int (*remove)(struct qctx_t *q, void *v);
	int (*verify)(struct qctx_t *q, void *v);
	void (*init)(struct qctx_t *q, void *v);
	void (*deinit)(struct qctx_t *q, void *v);
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
	BUG_ON(q->insert == NULL);
#endif

    FQLOCK_LOCK(q);

	if (q->insert(q, f) == 0) {
		q->len ++;
#ifdef DBG_PERF
		if (q->len > q->dbg_maxlen)
			q->dbg_maxlen = q->len;
#endif /* DBG_PERF */
	}

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
	BUG_ON(q->remove == NULL);
#endif

    FQLOCK_LOCK(q);

    void *f = q->bot;
    if (f == NULL) {
        FQLOCK_UNLOCK(q);
        return NULL;
    }

	if (q->remove(q, f) != 0)
		f = NULL;
	else {
		#if defined(BUILD_DEBUG)
		    BUG_ON(q->len == 0);
		#endif
		    if (q->len > 0)
		        q->len--;
	}

    FQLOCK_UNLOCK(q);
    return f;
}


#endif