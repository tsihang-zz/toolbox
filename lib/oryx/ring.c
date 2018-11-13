#include "oryx.h"

#define DEAFULT_RING_ELEMENTS	4096

static __oryx_always_inline__
uint16_t ring_key (const char *v, uint32_t s) 
{
     uint8_t *d = (uint8_t *)v;
     uint32_t i;
     uint16_t hv = 0;

     for (i = 0; i < s; i++)
	 	hv += d[i];

	 return hv;
}

int oryx_ring_create(const char *name,
		int nr_elements, uint32_t flags, struct oryx_ring_t **ring)
{
	BUG_ON(name == NULL);
	BUG_ON(ring == NULL);

	struct oryx_ring_t	*r;
	key_t				key = ring_key(name, strlen(name));

	if (nr_elements == 0)
		nr_elements = DEAFULT_RING_ELEMENTS;

	{
		r = malloc(sizeof(struct oryx_ring_t));
		if (unlikely(!r))
			return -1;
		memset(r, 0, sizeof(struct oryx_ring_t));

		r->data = malloc(sizeof(struct oryx_ring_data_t) * nr_elements);
		if (unlikely(!r->data)) {
			free(r);
			return -1;
		}
		memset(r->data, 0, sizeof(struct oryx_ring_data_t) * nr_elements);
	}
	
	r->rp			= 0;
	r->wp			= nr_elements;
	r->max_elements = nr_elements;
	r->ul_flags		= flags;
	r->name	= name;
	r->key			= key;
	RLOCK_INIT(r);
	
	(*ring) = r;
	return 0;	
}

void oryx_ring_dump(struct oryx_ring_t *ring)
{
	fprintf (stdout, "%16s%32s\n", "name: ",		ring->name);
	fprintf (stdout, "%16s%32d\n", "nb_data: ",		ring->max_elements);
	fprintf (stdout, "%16s%32lu\n", "rp_times: ",	ring->nr_times_r);
	fprintf (stdout, "%16s%32lu\n", "wp_times: ",	ring->nr_times_w);
	fprintf (stdout, "%16s%32lu\n", "rp: ",			ring->rp);
	fprintf (stdout, "%16s%32lu\n", "wp: ",			ring->wp);
}


