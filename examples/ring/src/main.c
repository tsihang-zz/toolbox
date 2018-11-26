#include "oryx.h"

#define RING_ELEMENTS	1024

#define RING_DATA_SIZE 1024

struct oryx_ring_t *my_ring;

static __oryx_always_inline__
void * ring_rp (void *r)
{
	int			err;
	void		*value;
	uint16_t	valen = 0;
	struct oryx_ring_t *ring = (struct oryx_ring_t *)r;

	FOREVER {
		err = oryx_ring_get(ring, &value, &valen);
		if (err) {
			//fprintf(stdout, "empty\n");
			continue;
		} else {
			if (value) {
				free(value);
			}
		}
	}
	
	oryx_task_deregistry_id(pthread_self());
	return NULL;
}

static __oryx_always_inline__
void * ring_wp (void *r)
{
	int			err;
	char		*value;
	struct oryx_ring_t *ring = (struct oryx_ring_t *)r;

	FOREVER {
		if((value = malloc(RING_DATA_SIZE)) == NULL)
			continue;
		oryx_pattern_generate(value, RING_DATA_SIZE);
		err = oryx_ring_put(ring, value, RING_DATA_SIZE);
		if (err) {
			//fprintf(stdout, "full\n");
			free(value);
			continue;
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
	int			err;
	
	oryx_initialize();

	err = oryx_ring_create("test_ring",
			RING_ELEMENTS, RING_DATA_SIZE, flags, &my_ring);
	if (err) return 0;

	oryx_ring_dump(my_ring);

	wp_task.argc = 1;
	wp_task.argv = my_ring;
	oryx_task_registry(&wp_task);

	rp_task.argc = 1;
	rp_task.argv = my_ring;
	oryx_task_registry(&rp_task);

	oryx_task_launch();

	FOREVER {
		oryx_ring_dump(my_ring);
		sleep(1);
	};

	return 0;
}

