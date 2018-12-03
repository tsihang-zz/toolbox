#include "oryx.h"

#define CACHE_LINE_ROUNDUP(size,cache_line_size) \
	(cache_line_size * ((size + cache_line_size - 1) / cache_line_size))

struct oryx_mpool_handle_t {
	oryx_vector v;
};

struct oryx_mpool_t {
	//const char*		name;
	void			*mhandler;					/* mem handle. */
	void			*free_q;				/* freed queue for fast alloc. */
	uint32_t		nr_elements;			/* elements count. */
	uint32_t		nr_bytes_per_element;	/* cache lined. */
	uint32_t		nr_steps;				/* grow steps for mempool after no vlaid memory. */
	uint32_t		cache_line_size;		/* cache line size for every element. */
	ATOMIC_DECLARE(uint64_t, nr_alloc_failure);
};

static __oryx_always_inline__
void os_mh_elem_free
(
	IN void *mhandler
) 
{
	int each;
	void *v;	
	struct oryx_mpool_handle_t *h;
	
	BUG_ON(mhandler == NULL);
	h = (struct oryx_mpool_handle_t *)mhandler;

	if (!h->v) {
		fprintf(stdout, "empty element vector\n");
		return;
	}
	
	vec_foreach_element(h->v, each, v) {
		if (likely(v)) {
			free(v);
		}
	}
	
	vec_free(h->v);
}

static __oryx_always_inline__
void *os_mh_elem_alloc
(
	IN void *mhandler,
	IN uint32_t size
)
{
	struct oryx_mpool_handle_t *h;
	void *v = NULL;

	BUG_ON(mhandler == NULL);
	h   = (struct oryx_mpool_handle_t *)(mhandler);
	
	v = malloc(size);
	if (unlikely (!v)) {
		oryx_panic(-1,
			"malloc: %s", oryx_safe_strerror(errno));
	}
	
	memset (v, 0, size);
	vec_set(h->v, v);

	return v;
}

__oryx_always_extern__
void oryx_mpool_new
(
	OUT void ** p,
	IN const char *name,
	IN int nr_steps,
	IN int nr_bytes_per_element,
	IN int cache_line_size
)
{
	int i;
	struct oryx_mpool_t *omp;
	struct oryx_mpool_handle_t *omph;

	BUG_ON(name == NULL);
	
	if((omp = (struct oryx_mpool_t *)malloc(sizeof(struct oryx_mpool_t))) == NULL)
		oryx_panic(-1,
			"malloc: %s", oryx_safe_strerror(errno));
	
	memset(omp, 0, sizeof(struct oryx_mpool_t));
	
	/* init memory handler */
	if((omph = malloc(sizeof(struct oryx_mpool_handle_t)))  == NULL)
		oryx_panic(-1,
			"malloc: %s", oryx_safe_strerror(errno));

	omph->v			= vec_init(1);
	BUG_ON(omph->v == NULL);
	
	/* new free memory queue*/
	oryx_lq_new(name, 0, &omp->free_q);
	BUG_ON(omp->free_q == NULL);

	//omp->name					= name;
	omp->mhandler				= omph;
	omp->nr_steps				= nr_steps;
	omp->nr_bytes_per_element	= CACHE_LINE_ROUNDUP(nr_bytes_per_element, cache_line_size);
	omp->cache_line_size		= cache_line_size;

	for(i = 0 ; i < (int)omp->nr_steps ; i ++) {
		oryx_lq_enqueue(omp->free_q, 
			os_mh_elem_alloc(omp->mhandler, omp->nr_bytes_per_element));
		omp->nr_elements ++;
	}

	(*p) = (void *)omp;

	fprintf (stdout, "Init mempool %s(%p)\n", name, omp);
}

__oryx_always_extern__
void oryx_mpool_destroy 
(
	IN void * p
)
{
	fprintf (stdout, "Uninit mempool %p .... ", p);

	struct oryx_mpool_t *omp = (struct oryx_mpool_t *)p;
		
	BUG_ON(omp == NULL || omp->mhandler == NULL);

	/*destroy free queue */
	oryx_lq_destroy(omp->free_q);
	
	/*destroy memory */
	os_mh_elem_free(omp->mhandler);

	free(omp->mhandler);
	free(omp);

	fprintf (stdout, "done\n");
}

__oryx_always_extern__
void *oryx_mpool_alloc 
(
	IN void * p
)
{
	int i = 0;
	void *element;
	void *elem = NULL;
	struct oryx_mpool_t *omp = (struct oryx_mpool_t *)p;

	do {
		/* no enough element in FREE queue. */
		if (oryx_lq_length(omp->free_q) == 0) {
			for (i = 0; i < (int)omp->nr_steps; i ++) {
				if ((element = os_mh_elem_alloc(omp->mhandler, omp->nr_bytes_per_element)) == NULL)
					break;
				oryx_lq_enqueue(omp->free_q, element);
			}
			omp->nr_elements += i;
		}
		
		/** get memory from free queue. */
		elem = oryx_lq_dequeue(omp->free_q);
		if(!elem) {
			atomic_inc(omp->nr_alloc_failure);
			return NULL;
		}
		
	} while (unlikely(elem == NULL));
	
	return elem;
}

__oryx_always_extern__
void oryx_mpool_dump 
(
	IN void * p
)
{
	struct oryx_mpool_t *omp = (struct oryx_mpool_t *)p;
	BUG_ON(omp == NULL || omp->mhandler == NULL);

	fprintf(stdout, "mpool.mhandle %p\n", omp->mhandler);
}

__oryx_always_extern__
void oryx_mpool_free 
(
	IN void * p ,
	IN void *elem
)
{
	struct oryx_mpool_t *omp = (struct oryx_mpool_t *)p;
	oryx_lq_enqueue(omp->free_q , elem);
}

