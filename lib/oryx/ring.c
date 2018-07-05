#include "oryx.h"
#include "ring.h"

#define DEAFULT_RING_ELEMENTS	4096
int oryx_ring_create(const char *ring_name,
		int nb_data, uint32_t flags, struct oryx_ring_t **ring)
{
	struct oryx_ring_t *r;

	BUG_ON(ring_name == NULL);
	BUG_ON(ring == NULL);
	
	r = malloc(sizeof(struct oryx_ring_t));
	if(unlikely(!r))
		return -1;
	memset(r, 0, sizeof(struct oryx_ring_t));

	if(nb_data == 0)
		nb_data = DEAFULT_RING_ELEMENTS;
	
	r->data = malloc(sizeof(struct oryx_ring_data_t) * nb_data);
	if(unlikely(!r->data)) {
		free(r);
		return -1;
	}
	memset(r->data, 0, sizeof(struct oryx_ring_data_t) * nb_data);
	
	r->rp			= 0;
	r->wp			= nb_data;
	r->nb_data		= nb_data;
	r->ul_flags		= flags;
	r->ring_name	= ring_name;
	RLOCK_INIT(r);
	
	(*ring) = r;
	return 0;	
}

void oryx_ring_dump(struct oryx_ring_t *ring)
{
	printf ("%16s%32s\n", "ring_name: ", ring->ring_name);
	printf ("%16s%32d\n", "nb_data: ", ring->nb_data);
	printf ("%16s%32d\n", "rp_times: ", ring->ul_rp_times);
	printf ("%16s%32d\n", "wp_times: ", ring->ul_wp_times);
	printf ("%16s%32d\n", "rp: ", ring->rp);
	printf ("%16s%32d\n", "wp: ", ring->wp);
}


