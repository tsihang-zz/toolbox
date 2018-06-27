#include "oryx.h"

#include "queue.h"

struct mem_pool_t {
	void			*mem_handle;
	struct qctx_t	*free_q;
	uint32_t		nb_elements;
	uint32_t		nb_bytes_per_element;
	uint32_t		steps;		/* grow steps for mempool after no vlaid memory. */
	struct element_fn_t *efn;
};

void mpool_init(void ** mp, const char *mp_name,
			int nb_steps, int nb_bytes_per_element,
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
	MEM_InitMemory(&handle->mem_handle);
	/*new queue*/
	fq_new(mp_name, &handle->free_q);
	handle->steps					= nb_steps;
	handle->nb_bytes_per_element	= nb_bytes_per_element;
	handle->efn						= efn;
	handle->free_q->insert			= handle->efn->insert;
	handle->free_q->remove			= handle->efn->remove;

	BUG_ON(handle->free_q == NULL);
	for(i = 0 ; i < handle->steps ; i ++) {
		if((element = MEM_GetMemory(handle->mem_handle, nb_bytes_per_element)) == NULL)
			exit(EXIT_FAILURE);
		if (i == 0) printf ("elementr %p\n", element);
		handle->efn->init(element);
		fq_equeue(handle->free_q, element);
		handle->nb_elements ++;
	}

	fq_dump(handle->free_q);
	
	(*mp) = (void *)handle;
}

void mpool_uninit(void * mp)
{
	struct mem_pool_t *handle = (struct mem_pool_t *)mp;
		
	BUG_ON(handle == NULL || handle->mem_handle == NULL);

	/*destroy free queue */
	fq_destroy(handle->free_q);
	
	/*destroy memory */
	MEM_UninitMemory(handle->mem_handle);

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
			oryx_logn("Trying alloc %d", handle->steps);
			for(i = 0 ; i < handle->steps ; i ++) {
				if((element = MEM_GetMemory(handle->mem_handle , handle->nb_bytes_per_element)) == NULL)
					break;
				handle->efn->init(element);
				fq_equeue(handle->free_q, element);
			}
			
			oryx_logn("%d alloced", i);
			handle->nb_elements += i;
		}
		
		/** get memory from free queue. */
		elem = fq_dequeue(handle->free_q);
		if(!elem) {
			oryx_logn("Queue_pull failed\n");
			return NULL;
		}
		
	} while (unlikely(elem == NULL));
	
	return elem;
}

int mpool_free (void * mp , void *elem)
{
	struct mem_pool_t *handle = (struct mem_pool_t *)mp;
#if 0	
	if(handle->efn->verify(handle->free_q, elem)) {
		return;
	}
#endif
	fq_equeue(handle->free_q , elem);
}

