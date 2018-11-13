#include "oryx.h"

#define RING_ELEMENTS	1024

#define RING_DATA_SIZE 1024

struct oryx_ring_t *my_ring;

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
			free(data);
		}
	}
	
	oryx_task_deregistry_id(pthread_self());
	return NULL;
}

static __oryx_always_inline__
void * ring_wp (void *r)
{
	uint32_t	times = 0;
	int			sleeps = 0;
	void		*data;
	struct oryx_ring_t *ring = (struct oryx_ring_t *)r;

	FOREVER {
		if (NULL == (data = malloc(RING_DATA_SIZE)))
			continue;

		oryx_pattern_generate(data, RING_DATA_SIZE);
		if (oryx_ring_put(ring, data, RING_DATA_SIZE) != 0) {
			free(data);
			continue;
		}

		times ++;
		usleep(1000);

		/** dump ring desc every 3 sec */
		if (sleeps ++ == 3000) {
			sleeps = 0;
			oryx_ring_dump(ring);
			fprintf (stdout, "Write times %u\n", times);
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
	.lcore_mask	= INVALID_CORE,
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
	.lcore_mask	= INVALID_CORE,
	.ul_prio	= KERNEL_SCHED,
	.argc		= 0,
	.argv		= NULL,
	.ul_flags	= 0,	/** Can not be recyclable. */
};

int main (
	int		__oryx_unused_param__	argc,
	char	__oryx_unused_param__	** argv
)
{
	uint32_t	flags = 0;
	
	oryx_initialize();

	if (oryx_ring_create("test_ring", RING_ELEMENTS, flags, &my_ring) != 0) return 0;

	oryx_ring_dump(my_ring);

	wp_task.argc = 1;
	wp_task.argv = my_ring;
	oryx_task_registry(&wp_task);

	rp_task.argc = 1;
	rp_task.argv = my_ring;
	oryx_task_registry(&rp_task);

	oryx_task_launch();

	FOREVER {
		sleep(1);
	};

	return 0;
}

