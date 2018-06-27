#include "oryx.h"
#include "queue.h"

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

struct qctx_t *FlowQueueInit (const char *fq_name,	struct qctx_t *q)
{
    if (q != NULL) {
        memset(q, 0, sizeof(struct qctx_t));
		q->name = fq_name;
        FQLOCK_INIT(q);
    }
    return q;
}

int fq_new(const char *fq_name, struct qctx_t ** fq)
{
    struct qctx_t *q = (struct qctx_t *)malloc(sizeof(struct qctx_t));
    if (q == NULL) {
        oryx_loge(0,
			"Fatal error encountered in FlowQueueNew. Exiting...");
        exit(EXIT_SUCCESS);
    }
    (*fq) = FlowQueueInit(fq_name, q);
	
    return 0;
}



/**
 *  \brief Destroy a flow queue
 *
 *  \param q the flow queue to destroy
 */
void fq_destroy (struct qctx_t * fq)
{
	FQLOCK_DESTROY(fq);
}

/**
 *  \brief add an instance to a queue
 *
 *  \param q queue
 *  \param f instance
 */
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

void fq_dump(struct qctx_t *q){
	printf("%16s%32s\n", "qname: ", q->name);
	printf("%16s%32d\n", "qlen: ", q->len);
}

struct flow_t {
	/** queue list pointers, protected by queue mutex. */
    struct flow_t *lnext; /* list */
    struct flow_t *lprev;

	uint64_t 	owner;
	uint32_t	data_size;
	char		data[];
};

int flow_insert(struct qctx_t *q, void *v)
{
	struct flow_t *f = (struct flow_t *)v;

	printf("q %p insert %p\n", q, v);
	//f->owner = (uint64_t)q;
	
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

int flow_remove(struct qctx_t *q, void *v)
{
	struct flow_t *f = (struct flow_t *)v;

	printf("remove %p\n", v);
	if (f->owner != (uint64_t)q) {
		oryx_logn("flow %p %x is not in queue %p", f, f->owner, q);
	}
	
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

int flow_verify(struct qctx_t *q, void *v)
{
	struct flow_t *f = (struct flow_t *)v;

	printf("verify %p\n", v);
	if(f->owner == ((uint64_t)q & 0xffffffff)) {
		oryx_logn("double free!");
		return -1;
	}
}

int flow_pool_alloc(struct qctx_t *q, int nb_free_blocks)
{
	int i;
	int data_size = 0;
	size_t block_size;
	struct flow_t *f = NULL;
	
	for (i = 0; i < nb_free_blocks; i ++) {
		block_size = sizeof (struct flow_t) + data_size;
		f = malloc(block_size);
		if(f == NULL)
			break;
		
		memset (f, 0, block_size);
		f->owner = i;
		f->data_size = data_size;

		fq_equeue(q, f);
	}

	return i;
}

void flow_init(void *flow)
{
	struct flow_t *f = (struct flow_t *)flow;

	memset (f, 0, sizeof(struct flow_t));
	f->lnext		= NULL;
	f->lprev		= NULL;
	f->owner		= 0;
	f->data_size	= 0;
}

void flow_deinit(void *flow)
{
	struct flow_t *f = (struct flow_t *)flow;
	f = f;
}


void fq_sample(void)
{
#if 0
	struct qctx_t *free_q, *work_q;

	fq_new("flow free queue", &free_q);
	fq_new("flow work queue", &work_q);

	BUG_ON(free_q == NULL);
	BUG_ON(work_q == NULL);
	
	free_q->insert = flow_insert;
	free_q->remove = flow_remove;

	work_q->insert = flow_insert;
	work_q->remove = flow_remove;

	/** init free queue */
	flow_pool_alloc(free_q, 10000);

	fq_dump(free_q);
	fq_dump(work_q);

	struct flow_t *f = NULL;
	while((f = fq_dequeue(free_q)) != NULL) {
		fq_equeue(work_q, f);
	}

	fq_dump(free_q);
	fq_dump(work_q);
	
	fq_destroy(free_q);
	fq_destroy(work_q);

#endif

	void *flow_pool;
	struct element_fn_t efn = {
		.insert = flow_insert,
		.remove = flow_remove,
		.verify = flow_verify,
		.init = flow_init,
		.deinit = flow_deinit,
	};


	int i = 3;

	mpool_init(&flow_pool, "mempool for flow",
					i, sizeof(struct flow_t), &efn);

	void *elem = mpool_alloc(flow_pool);
	if(elem) {
		mpool_free(flow_pool, elem);
	}
}

