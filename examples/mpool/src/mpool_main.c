#include "oryx.h"

/** A mempool test. */

struct flow_t {
#define FLOW_CACHE_LINE_SIZE	64
	/** queue list pointers, protected by queue mutex. */
    struct flow_t *lnext;	/* list */
    struct flow_t *lprev;

	uint64_t	fp;			/* finger print */
	uint32_t	data_size;	/* size of data */
	char		data[];
};


static __oryx_always_inline__
int flow_verify(struct qctx_t *q, void *v)
{
	return (((struct flow_t *)v)->fp == ((uint64_t)q));
}

static __oryx_always_inline__
int flow_insert(struct qctx_t *q, void *v)
{
	struct flow_t *f = (struct flow_t *)v;

#if defined(BUILD_DEBUG)
	BUG_ON(f->fp == 0);				/* fp is not set */
	BUG_ON(!flow_verify(q, v));		/* fp is not for this queue. */
#endif

    /* more flows in queue */
    if (q->top != NULL) {
        f->lnext = q->top;
        ((struct flow_t *)q->top)->lprev = f;
        q->top = f;
    /* only flow */
    } else {
        q->top = f;
        q->bot = f;
    }

	return 0;
}

static __oryx_always_inline__
int flow_remove(struct qctx_t *q, void *v)
{
	struct flow_t *f = (struct flow_t *)v;

#if defined(BUILD_DEBUG)
	BUG_ON(f->fp == 0); 			/* fp is not set */
	BUG_ON(!flow_verify(q, v)); 	/* fp is not for this queue. */
#endif

    /* more packets in queue */
    if (((struct flow_t *)q->bot)->lprev != NULL) {
        q->bot = ((struct flow_t *)q->bot)->lprev;
        ((struct flow_t *)q->bot)->lnext = NULL;
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
void flow_init(struct qctx_t *q, void *v)
{
	struct flow_t *f = (struct flow_t *)v;

	memset (f, 0, sizeof(struct flow_t));
	f->lnext		= NULL;
	f->lprev		= NULL;
	f->fp			= (uint64_t)q;
	f->data_size	= 0;
}

static __oryx_always_inline__
void flow_deinit(struct qctx_t *q, void *v)
{
	struct flow_t *f = (struct flow_t *)v;
	f = f;
	q = q;
}

int main(int argc, char ** argv)
{
	void *flow_pool, *flow_pool1;
	struct element_fn_t efn = {
		.insert = flow_insert,
		.remove = flow_remove,
		.verify = flow_verify,
		.init = flow_init,
		.deinit = flow_deinit,
	};

	oryx_initialize();
	
	int i = 3;

	mpool_init(&flow_pool, "mempool for flow",
					i, sizeof(struct flow_t), FLOW_CACHE_LINE_SIZE, &efn);
	
	mpool_init(&flow_pool1, "mempool for flow",
					i, sizeof(struct flow_t), FLOW_CACHE_LINE_SIZE, &efn);

	struct timeval start, end;
	
	/** test normal alloc and free with same pool. */
	i = 3;
	printf ("test normal .... ");
	do {
		void *elem;
		gettimeofday(&start, NULL);
		elem = mpool_alloc(flow_pool);
		gettimeofday(&end, NULL);	
		if(elem) {
			oryx_logn("from pool -> cost %llu", tm_elapsed_us(&start, &end));
			mpool_free(flow_pool, elem);
		}

		gettimeofday(&start, NULL);
		elem = malloc(64);
		gettimeofday(&end, NULL);	
		if(elem) {
			oryx_logn("stdlib -> cost %llu", tm_elapsed_us(&start, &end));
			free(elem);
		}

		sleep (1);
	}while (i --);
	printf ("done !\n");

	mpool_uninit(flow_pool);
	mpool_uninit(flow_pool1);


	while(1) {
		mpool_init(&flow_pool, "mempool for flow",
						1000, sizeof(struct flow_t), FLOW_CACHE_LINE_SIZE, &efn);
		
		mpool_init(&flow_pool1, "mempool for flow",
						1000, sizeof(struct flow_t), FLOW_CACHE_LINE_SIZE, &efn);

		mpool_uninit(flow_pool);
		mpool_uninit(flow_pool1);
		usleep(10000);
	}

	return 0;
}
