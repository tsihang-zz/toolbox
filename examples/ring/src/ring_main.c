#include "oryx.h"

#define DATA_SIZE	(sizeof(uint32_t))
#define shm_elem 1024
char *shmemory[shm_elem];

static __oryx_always_inline__
void * ring_rp (void *r)
{
	void		*data;
	uint16_t	data_size = 0;
	struct oryx_ring_t *ring = (struct oryx_ring_t *)r;

	FOREVER {
		if (oryx_ring_get(ring, &data, &data_size) != 0)
			continue;
		if (data) {
			if (!(ring->ul_flags & RING_SHARED))
				free(data);
		}
	}
	
	oryx_task_deregistry_id(pthread_self());
	return NULL;
}

static __oryx_always_inline__
void * ring_wp (void *r)
{
	int			i = 0;
	uint32_t	times = 0;
	int			sleeps = 0;
	void		*data;
	struct oryx_ring_t *ring = (struct oryx_ring_t *)r;

	FOREVER {
		if (!(ring->ul_flags & RING_SHARED)) {
			if(NULL == (data = malloc(DATA_SIZE)))
				continue;
		}
		else {
			data = shmemory[(i ++) % shm_elem];
		}

		*(uint32_t *)data = times ++;

		if (oryx_ring_put(ring, data, DATA_SIZE) != 0) {
			if (!(ring->ul_flags & RING_SHARED))
				free(data);
			continue;
		}
		
		usleep(1000);

		/** dump ring desc every 3 sec */
		if (sleeps ++ == 3000) {
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
	.module		= THIS,
	.sc_alias	= "RP Task",
	.fn_handler	= ring_rp,
	.ul_lcore	= INVALID_CORE,
	.ul_prio	= KERNEL_SCHED,
	.argc		= 0,
	.argv		= NULL,
	.ul_flags	= 0,	/** Can not be recyclable. */
};


static struct oryx_task_t wp_task =
{
	.module		= THIS,
	.sc_alias	= "WP Task",
	.fn_handler	= ring_wp,
	.ul_lcore	= INVALID_CORE,
	.ul_prio	= KERNEL_SCHED,
	.argc		= 0,
	.argv		= NULL,
	.ul_flags	= 0,	/** Can not be recyclable. */
};

static void share_mem_init(void)
{
	int r;
	const uint16_t	key	= 12345;

	for (r = 0; r < shm_elem; r ++) {
		void *m = MEM_GetShareMem(ring_key_data(key, r), DATA_SIZE);
		if (likely(m)) {
			printf("%x: %p\n", ring_key_data(key, r), m);
			shmemory[r] = m;
			
		}else {
			exit(0);
			shmemory[r] = NULL;
		}
	}
}

int main (
	int		__oryx_unused_param__	argc,
	char	__oryx_unused_param__	** argv
)
{
	int				r = 0;
#if 1
	uint32_t	flags = 0;
#else
	uint32_t	flags = RING_SHARED;
#endif
	struct oryx_ring_t *ring;

	oryx_initialize();
	
	r = oryx_ring_create("test_ring", 1024, flags, &ring);
	if (r != 0) return 0;

	if (flags & RING_SHARED)
		share_mem_init();

	oryx_ring_dump(ring);

	wp_task.argc = 1;
	wp_task.argv = ring;
	oryx_task_registry(&wp_task);

	rp_task.argc = 1;
	rp_task.argv = ring;
	oryx_task_registry(&rp_task);

	oryx_task_launch();

	FOREVER;

	return 0;
}

