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

int oryx_ring_create(const char *ring_name,
		int nb_max_elements, uint32_t flags, struct oryx_ring_t **ring)
{
	BUG_ON(ring_name == NULL);
	BUG_ON(ring == NULL);

	struct oryx_ring_t	*r;
	key_t				key = ring_key(ring_name, strlen(ring_name));

	if (nb_max_elements == 0)
		nb_max_elements = DEAFULT_RING_ELEMENTS;

	{
		r = malloc(sizeof(struct oryx_ring_t));
		if (unlikely(!r))
			return -1;
		memset(r, 0, sizeof(struct oryx_ring_t));

		r->data = malloc(sizeof(struct oryx_ring_data_t) * nb_max_elements);
		if (unlikely(!r->data)) {
			free(r);
			return -1;
		}
		memset(r->data, 0, sizeof(struct oryx_ring_data_t) * nb_max_elements);
	}
	
	r->rp			= 0;
	r->wp			= nb_max_elements;
	r->max_elements = nb_max_elements;
	r->ul_flags		= flags;
	r->ring_name	= ring_name;
	r->key			= key;
	RLOCK_INIT(r);
	
	(*ring) = r;
	return 0;	
}

void oryx_ring_dump(struct oryx_ring_t *ring)
{
	fprintf (stdout, "%16s%32s\n", "ring_name: ",	ring->ring_name);
	fprintf (stdout, "%16s%32d\n", "nb_data: ",		ring->max_elements);
	fprintf (stdout, "%16s%32d\n", "rp_times: ",		ring->ul_rp_times);
	fprintf (stdout, "%16s%32d\n", "wp_times: ",		ring->ul_wp_times);
	fprintf (stdout, "%16s%32d\n", "rp: ",			ring->rp);
	fprintf (stdout, "%16s%32d\n", "wp: ",			ring->wp);
}


