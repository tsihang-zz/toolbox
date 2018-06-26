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
	
	r->rp		= 0;
	r->wp		= nb_data;
	r->nb_data	= nb_data;
	r->ul_flags	= flags;
	r->ring_name	= ring_name;

	(*ring) = r;
	return 0;	
}


int oryx_ring_get(struct oryx_ring_t *ring, void **data, uint16_t *data_size)
{
	uint32_t rp = 0;

	/** ring invalid */
	if(unlikely(!ring))
		return -1;
	
	(*data)		= NULL;
	(*data_size)	= 0;

	rp = ring->rp;
	if(rp == ring->wp) {
		return 0; /** no new data in ring. */
	}

	(*data)		= ring->data[rp].v;
	(*data_size)	= ring->data[rp].s;
	ring->rp 	= (ring->rp + 1) % ring->nb_data;
	ring->ul_rp_times ++;
	
	return 0;
}

int oryx_ring_put(struct oryx_ring_t *ring, void *data, uint16_t data_size)
{
	uint32_t wp = 0;

	if(unlikely(!ring))
		return -1;
	
	wp = ring->wp;
	if(((wp + 1) % ring->nb_data) == ring->rp){
		/** ring full */
		return -1;
	}	

	ring->data[wp].v	= data;
	ring->data[wp].s	= data_size;
	ring->wp		= (ring->wp + 1) % ring->nb_data;
	ring->ul_wp_times ++;
	
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

static __oryx_always_inline__
void * ring_rp (void *r)
{
	void *data;
	int ret = 0;
	uint16_t data_size = 0;
	struct oryx_ring_t *ring = (struct oryx_ring_t *)r;

	FOREVER {
		ret = oryx_ring_get(ring, &data, &data_size);
		if(ret != 0)
			continue;
		if(data) {
			//printf ("[%p] rp-> %u\n", data, *(uint32_t *)data);
			free(data);
		}
	}
	
	oryx_task_deregistry_id(pthread_self());
	return NULL;
}

static __oryx_always_inline__
void * ring_wp (void *r)
{
	uint32_t times = 0;
	int sleeps = 0;
	void *data;
	int ret = 0;
	uint16_t data_size = 0;
	struct oryx_ring_t *ring = (struct oryx_ring_t *)r;

	FOREVER {
		data = malloc(sizeof(times));
		if(!data) continue;
		*(uint32_t *)data = times ++;
		data_size = sizeof(times);
		ret = oryx_ring_put(ring, data, data_size);
		if(ret != 0) {
			free(data);
			continue;
		}
		
		usleep(1000);

		/** dump ring desc every 3 sec */
		if(sleeps ++ == 3000) {
			sleeps = 0;
			oryx_ring_dump(ring);
			printf("times %u\n", times);
		}
	}
	
	oryx_task_deregistry_id(pthread_self());
	return NULL;
}


static struct oryx_task_t rp_task =
{
	.module = THIS,
	.sc_alias = "RP Task",
	.fn_handler = ring_rp,
	.ul_lcore = INVALID_CORE,
	.ul_prio = KERNEL_SCHED,
	.argc = 0,
	.argv = NULL,
	.ul_flags = 0,	/** Can not be recyclable. */
};


static struct oryx_task_t wp_task =
{
	.module = THIS,
	.sc_alias = "WP Task",
	.fn_handler = ring_wp,
	.ul_lcore = INVALID_CORE,
	.ul_prio = KERNEL_SCHED,
	.argc = 0,
	.argv = NULL,
	.ul_flags = 0,	/** Can not be recyclable. */
};

void oryx_ring_test(void)
{
	int r = 0;
	struct oryx_ring_t *ring;

	r = oryx_ring_create("test_ring", 1024, 0, &ring);
	if(r != 0) return;

	oryx_ring_dump(ring);

	wp_task.argc = 1;
	wp_task.argv = ring;
	oryx_task_registry(&wp_task);


	rp_task.argc = 1;
	rp_task.argv = ring;
	oryx_task_registry(&rp_task);
}

