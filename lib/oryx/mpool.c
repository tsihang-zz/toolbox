#include "oryx.h"

#define CACHE_LINE_ROUNDUP(size,cache_line_size) \
	(cache_line_size * ((size + cache_line_size - 1) / cache_line_size))

struct oryx_mp_handle_t {
	oryx_vector v;
};

struct oryx_mpool_t {
	void			*mem;					/* mem handle. */
	void			*free_q;				/* freed queue for fast alloc. */
	uint32_t		nb_elements;			/* elements count. */
	uint32_t		nb_bytes_per_element;	/* cache lined. */
	uint32_t		steps;					/* grow steps for mempool after no vlaid memory. */
	uint32_t		cache_line_size;		/* cache line size for every element. */
};

static void os_free(void *mem) 
{
	int each;
	struct oryx_mp_handle_t *mh;
	void *v;
	
	BUG_ON(mem == NULL);
	
	mh = (struct oryx_mp_handle_t *)mem;
	BUG_ON(mh->v == NULL);

	vec_foreach_element(mh->v, each, v) {
		if (likely(v)) {
			free(v);
		}
	}

	vec_free(mh->v);
}
static void *os_alloc(void *p, uint32_t size)
{
	BUG_ON(p == NULL);
	struct oryx_mpool_t *omp = (struct oryx_mpool_t *)p;
	struct oryx_mp_handle_t *h = (struct oryx_mp_handle_t *)(omp->mem);
	void *v = NULL;

	BUG_ON(h == NULL);
	
	v = malloc(size);
	if (unlikely (!v))
		oryx_panic(-1,
			"malloc: %s", oryx_safe_strerror(errno));
	
	memset (v, 0, size);
	
	vec_set(h->v, h);

	return v;
}

void mpool_init(void ** p, const char *mp_name,
			int nb_steps, int nb_bytes_per_element, int cache_line_size)

{
	int i;
	void *element;
	struct oryx_mpool_t *omp;
	struct oryx_mp_handle_t *omph;

	BUG_ON(mp_name == NULL);
	
	if((omp = (struct oryx_mpool_t *)malloc(sizeof(struct oryx_mpool_t))) == NULL)
		oryx_panic(-1,
			"malloc: %s", oryx_safe_strerror(errno));
	
	memset(omp, 0, sizeof(struct oryx_mpool_t));
	
	/* init memory handler */
	if((omph = malloc(sizeof(struct oryx_mp_handle_t)))  == NULL)
		oryx_panic(-1,
			"malloc: %s", oryx_safe_strerror(errno));

	omph->v = vec_init(1);
	BUG_ON(omph->v == NULL);
	omp->mem = omph;
	
	/* new queue*/
	oryx_lq_new(mp_name, 0, &omp->free_q);
	BUG_ON(omp->free_q == NULL);

	omp->steps					= nb_steps;
	omp->nb_bytes_per_element	= CACHE_LINE_ROUNDUP(nb_bytes_per_element, cache_line_size);
	omp->cache_line_size		= cache_line_size;

	for(i = 0 ; i < (int)omp->steps ; i ++) {
		element = os_alloc(omp, omp->nb_bytes_per_element);
		oryx_lq_enqueue(omp->free_q, element);
		omp->nb_elements ++;
	}

	(*p) = (void *)omp;

	fprintf (stdout, "Init mempoll %p\n", omp);
}

void mpool_uninit(void * p)
{
	fprintf (stdout, "Uninit mempool %p .... ", p);

	struct oryx_mpool_t *omp = (struct oryx_mpool_t *)p;
		
	BUG_ON(omp == NULL || omp->mem == NULL);

	/*destroy free queue */
	oryx_lq_destroy(omp->free_q);
	
	/*destroy memory */
	os_free(omp->mem);

	free(omp->mem);
	free(omp);

	fprintf (stdout, "done\n");
}

void * mpool_alloc(void * p)
{
	int i = 0;
	void *element;
	void *elem = NULL;
	struct oryx_mpool_t *omp = (struct oryx_mpool_t *)p;

	do {
		/* no enough element in FREE queue. */
		if(oryx_lq_length(omp->free_q) == 0) {
			for(i = 0; i < (int)omp->steps; i ++) {
				if((element = os_alloc(omp->mem , omp->nb_bytes_per_element)) == NULL)
					break;
				oryx_lq_enqueue(omp->free_q, element);
			}
			omp->nb_elements += i;
		}
		
		/** get memory from free queue. */
		elem = oryx_lq_dequeue(omp->free_q);
		if(!elem) {
			return NULL;
		}
		
	} while (unlikely(elem == NULL));
	
	return elem;
}

void mpool_free (void * p , void *elem)
{
	struct oryx_mpool_t *omp = (struct oryx_mpool_t *)p;
	oryx_lq_enqueue(omp->free_q , elem);
}

