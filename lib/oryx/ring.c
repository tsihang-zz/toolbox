/*!
 * @file ring.c
 * @date 2017/08/29
 *
 * TSIHANG (haechime@gmail.com)
 */

#include "oryx.h"

#define DEAFULT_RING_ELEMENTS	4096

static __oryx_always_inline__
uint16_t ring_key
(
	IN const char *v,
	IN uint32_t s
) 
{
     uint8_t *d = (uint8_t *)v;
     uint32_t i;
     uint16_t hv = 0;

     for (i = 0; i < s; i++)
	 	hv += d[i];

	 return hv;
}

int oryx_ring_create
(
	IN const char *name,
	IN int nr_elements,
	IN size_t __oryx_unused__ nr_bytes_per_element,
	IN uint32_t flags,
	OUT struct oryx_ring_t **ring
)
{
	BUG_ON(name == NULL);
	BUG_ON(ring == NULL);

	struct oryx_ring_t	*r;
	key_t				key = ring_key(name, strlen(name));

	if (nr_elements == 0)
		nr_elements = DEAFULT_RING_ELEMENTS;

	{
		r = malloc(sizeof(struct oryx_ring_t));
		if (unlikely(!r)) {
			fprintf(stdout, "malloc: %s\n", oryx_safe_strerror(errno));
			return -1;
		}
		memset(r, 0, sizeof(struct oryx_ring_t));

		r->data = malloc(sizeof(struct oryx_ring_data_t) * nr_elements);
		if (unlikely(!r->data)) {
			fprintf(stdout, "malloc: %s\n", oryx_safe_strerror(errno));
			free(r);
			return -1;
		}
		memset(r->data, 0, sizeof(struct oryx_ring_data_t) * nr_elements);
	#if 0	
		int i;
		for(i = 0; i < nr_elements; i ++) {
			struct oryx_ring_data_t *rd = &r->data[i];
			rd->v = malloc(nr_bytes_per_element);
			if(unlikely(!rd->v)) {
				fprintf(stdout, "malloc: %s\n", oryx_safe_strerror(errno));
				exit(0);
			}
			memset(rd->v, 0, nr_bytes_per_element);
			rd->s0 = nr_bytes_per_element;
		}
	#endif
	}
	
	//r->rp			= 0;
	//r->wp			= nr_elements;
	atomic_set(r->rp, 0);
	atomic_set(r->wp, nr_elements);
	
	r->max_elements = nr_elements;
	r->ul_flags		= flags;
	r->name	= name;
	r->key			= key;
	oryx_sys_mutex_create(&r->mtx);
	
	(*ring) = r;
	return 0;	
}

void oryx_ring_dump
(
	IN struct oryx_ring_t *ring
)
{
	fprintf (stdout, "ring %s\n", ring->name);
	fprintf (stdout, "%16s%32d\n", "nb_data: ", 	ring->max_elements);
	fprintf (stdout, "%16s%32lu\n", "rp_times: ",	ring->nr_times_rd);
	fprintf (stdout, "%16s%32lu\n", "wp_times: ",	ring->nr_times_wr);
	fprintf (stdout, "%16s%32lu\n", "full_times: ", ring->nr_times_f);		
	fprintf (stdout, "%16s%32lu\n", "rp: ", 		atomic_read(ring->rp));
	fprintf (stdout, "%16s%32lu\n", "wp: ", 		atomic_read(ring->wp));

}


