#ifndef GEO_CDR_MPOOL_H
#define GEO_CDR_MPOOL_H

/** A mempool test. */

struct geo_cdr_entry_t {
	/** queue list pointers, protected by queue mutex. */
    struct geo_cdr_entry_t *lnext;	/* list */
    struct geo_cdr_entry_t *lprev;

	uint64_t	fp;			/* finger print */
	void		*hash_entry;
	struct geo_key_info_t gki;

	uint32_t	data_size;	/* size of data */
	char		data[];
};


static __oryx_always_inline__
int cdr_entry_verify(struct qctx_t *q, void *v)
{
	return (((struct geo_cdr_entry_t *)v)->fp == ((uint64_t)q));
}

static __oryx_always_inline__
int cdr_entry_insert(struct qctx_t *q, void *v)
{
	struct geo_cdr_entry_t *f = (struct geo_cdr_entry_t *)v;

#if defined(BUILD_DEBUG)
	BUG_ON(f->fp == 0);				/* fp is not set */
	BUG_ON(!cdr_entry_verify(q, v));		/* fp is not for this queue. */
#endif

    /* more flows in queue */
    if (q->top != NULL) {
        f->lnext = q->top;
        ((struct geo_cdr_entry_t *)q->top)->lprev = f;
        q->top = f;
    /* only flow */
    } else {
        q->top = f;
        q->bot = f;
    }

	return 0;
}

static __oryx_always_inline__
int cdr_entry_remove(struct qctx_t *q, void *v)
{
	struct geo_cdr_entry_t *f = (struct geo_cdr_entry_t *)v;

#if defined(BUILD_DEBUG)
	BUG_ON(f->fp == 0); 			/* fp is not set */
	BUG_ON(!cdr_entry_verify(q, v)); 	/* fp is not for this queue. */
#endif

    /* more packets in queue */
    if (((struct geo_cdr_entry_t *)q->bot)->lprev != NULL) {
        q->bot = ((struct geo_cdr_entry_t *)q->bot)->lprev;
        ((struct geo_cdr_entry_t *)q->bot)->lnext = NULL;
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
void cdr_entry_init(struct qctx_t *q, void *v)
{
	struct geo_cdr_entry_t *f = (struct geo_cdr_entry_t *)v;

	memset (f, 0, sizeof(struct geo_cdr_entry_t));
	f->lnext		= NULL;
	f->lprev		= NULL;
	f->fp			= (uint64_t)q;
	f->data_size	= 0;
}

static __oryx_always_inline__
void cdr_entry_deinit(struct qctx_t *q, void *v)
{
	struct geo_cdr_entry_t *f = (struct geo_cdr_entry_t *)v;
	f = f;
	q = q;
}


#endif
