#include "oryx.h"

#define CACHE_LINE_ROUNDUP(size,cache_line_size) \
	(cache_line_size * ((size + cache_line_size - 1) / cache_line_size))

struct mem_handle_t {
	oryx_vector v;
};

struct mem_pool_t {
	void			*mem_handle;			/* mem handle, unused now. */
	struct qctx_t	*free_q;				/* freed queue for fast alloc. */
	uint32_t		nb_elements;			/* elements count. */
	uint32_t		nb_bytes_per_element;	/* cache lined. */
	uint32_t		steps;					/* grow steps for mempool after no vlaid memory. */
	uint32_t		cache_line_size;		/* cache line size for every element. */
	struct element_fn_t *efn;
};

static void InitMemory(void **mem_handle) 
{
	struct mem_handle_t *mh;

	if((mh = malloc(sizeof(struct mem_handle_t)))  == NULL)
		exit (EXIT_FAILURE);

	mh->v = vec_init(1);
	BUG_ON(mh->v == NULL);

	(*mem_handle) = mh;
}

static void UnInitMemory(void *mem_handle) 
{
	int each;
	struct mem_handle_t *mh;
	void *v;
	
	BUG_ON(mem_handle == NULL);
	
	mh = (struct mem_handle_t *)mem_handle;
	vec_foreach_element(mh->v, each, v) {
		if (likely(v)) {
			free(v);
		}
	}

	vec_free(mh->v);
}

static void SetMemory(void *mem_handle, void *mem)
{
	struct mem_handle_t *mh = (struct mem_handle_t *)mem_handle;
	vec_set(mh->v, mem);
}

static void *GetMemory(void *mem_handle, uint32_t size)
{
	void *v = NULL;

	v = malloc(size);
	if (v) {
		memset (v, 0, size);
		/** hold this memory to mem_handle. */
		SetMemory(mem_handle, v);
	}

	return v;
}

void mpool_init(void ** mp, const char *mp_name,
			int nb_steps, int nb_bytes_per_element, int cache_line_size,
			struct element_fn_t *efn)

{
	int i;
	void *element;
	struct mem_pool_t *handle;

	BUG_ON(mp_name == NULL);
	
	if((handle = (struct mem_pool_t *)malloc(sizeof(struct mem_pool_t))) == NULL)
		exit(EXIT_FAILURE);

	memset(handle, 0, sizeof(struct mem_pool_t));
	
	/*init*/
	InitMemory(&handle->mem_handle);
	
	/*new queue*/
	fq_new(mp_name, &handle->free_q);

	handle->steps					= nb_steps;
	handle->nb_bytes_per_element	= CACHE_LINE_ROUNDUP(nb_bytes_per_element, cache_line_size);
	handle->efn						= efn;
	handle->cache_line_size			= cache_line_size;
	handle->free_q->insert			= handle->efn->insert;
	handle->free_q->remove			= handle->efn->remove;
	handle->free_q->verify			= handle->efn->verify;
	handle->free_q->init			= handle->efn->init;
	handle->free_q->deinit			= handle->efn->deinit;

	BUG_ON(handle->free_q == NULL);
	
	for(i = 0 ; i < (int)handle->steps ; i ++) {
		if((element = GetMemory(handle->mem_handle, handle->nb_bytes_per_element)) == NULL)
			exit(EXIT_FAILURE);
		handle->free_q->init(handle->free_q, element);
		fq_equeue(handle->free_q, element);
		handle->nb_elements ++;
	}

	(*mp) = (void *)handle;
}

void mpool_uninit(void * mp)
{
	struct mem_pool_t *handle = (struct mem_pool_t *)mp;
		
	BUG_ON(handle == NULL || handle->mem_handle == NULL);

	/*destroy free queue */
	fq_destroy(handle->free_q);
	
	/*destroy memory */
	UnInitMemory(handle->mem_handle);

	free(handle->mem_handle);
	free(handle);
}

void * mpool_alloc(void * mp)
{
	int i = 0;
	void *element;
	void *elem = NULL;
	struct mem_pool_t *handle = (struct mem_pool_t *)mp;

	do {
		/* no enough element in FREE queue. */
		if(handle->free_q->len == 0) {
			for(i = 0 ; i < (int)handle->steps ; i ++) {
				if((element = GetMemory(handle->mem_handle , handle->nb_bytes_per_element)) == NULL)
					break;
				handle->free_q->init(handle->free_q, element);
				fq_equeue(handle->free_q, element);
			}
			handle->nb_elements += i;
		}
		
		/** get memory from free queue. */
		elem = fq_dequeue(handle->free_q);
		if(!elem) {
			return NULL;
		}
		
	} while (unlikely(elem == NULL));
	
	return elem;
}

void mpool_free (void * mp , void *elem)
{
	struct mem_pool_t *handle = (struct mem_pool_t *)mp;
	fq_equeue(handle->free_q , elem);
}

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

void mpool_sample(void)
{
	void *flow_pool, *flow_pool1;
	struct element_fn_t efn = {
		.insert = flow_insert,
		.remove = flow_remove,
		.verify = flow_verify,
		.init = flow_init,
		.deinit = flow_deinit,
	};


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
}

