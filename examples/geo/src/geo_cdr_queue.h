#ifndef GEO_CDR_QUEUE_H
#define GEO_CDR_QUEUE_H

/** CDR queue element, allocated by CDR MPOOL. */

struct geo_cdr_qelem_t {
	/** queue list pointers, protected by queue mutex. */
    struct geo_cdr_qelem_t *lnext;	/* list */
    struct geo_cdr_qelem_t *lprev;

	uint64_t	fp;			/* finger print */
	void		*data;		/* data -> cdr_mpool element. */
};


static __oryx_always_inline__
int cdr_qelem_verify(struct qctx_t *q, void *v)
{
	return (((struct geo_cdr_qelem_t *)v)->fp == ((uint64_t)q));
}

static __oryx_always_inline__
int cdr_qelem_insert(struct qctx_t *q, void *v)
{
	struct geo_cdr_qelem_t *f = (struct geo_cdr_qelem_t *)v;

#if defined(BUILD_DEBUG)
	BUG_ON(f->fp == 0);				/* fp is not set */
	BUG_ON(!cdr_qelem_verify(q, v));		/* fp is not for this queue. */
#endif

    /* more flows in queue */
    if (q->top != NULL) {
        f->lnext = q->top;
        ((struct geo_cdr_qelem_t *)q->top)->lprev = f;
        q->top = f;
    /* only flow */
    } else {
        q->top = f;
        q->bot = f;
    }

	return 0;
}

static __oryx_always_inline__
int cdr_qelem_remove(struct qctx_t *q, void *v)
{
	struct geo_cdr_qelem_t *f = (struct geo_cdr_qelem_t *)v;

#if defined(BUILD_DEBUG)
	BUG_ON(f->fp == 0); 			/* fp is not set */
	BUG_ON(!cdr_qelem_verify(q, v)); 	/* fp is not for this queue. */
#endif

    /* more packets in queue */
    if (((struct geo_cdr_qelem_t *)q->bot)->lprev != NULL) {
        q->bot = ((struct geo_cdr_qelem_t *)q->bot)->lprev;
        ((struct geo_cdr_qelem_t *)q->bot)->lnext = NULL;
    /* just the one we remove, so now empty */
    } else {
        q->top = NULL;
        q->bot = NULL;
    }

	f->lnext = NULL;
    f->lprev = NULL;

	return 0;
}

static __oryx_always_inline__
void cdr_qelem_init(struct qctx_t *q, void *v)
{
	struct geo_cdr_qelem_t *f = (struct geo_cdr_qelem_t *)v;

	memset (f, 0, sizeof(struct geo_cdr_qelem_t));
	f->lnext		= NULL;
	f->lprev		= NULL;
	f->fp			= (uint64_t)q;
}

static __oryx_always_inline__
void cdr_qelem_deinit(struct qctx_t *q, void *v)
{
	struct geo_cdr_qelem_t *f = (struct geo_cdr_qelem_t *)v;
	f = f;
	q = q;
}





#endif
