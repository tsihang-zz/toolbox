#include "oryx.h"

static uint64_t tx_pkts, rx_pkts;

static int running = 1;
static struct oryx_shmring_t *my_ring;

static void sigint_handler(int sig) {
	fprintf (stdout, "signal %d ...\n", sig);
	running = 0;
}

static __oryx_always_inline__
void * shmring_rp (void *r)
{
	void		*data;
	size_t		data_size = 0;
	int			err;
	struct oryx_shmring_t *ring = (struct oryx_shmring_t *)r;

	FOREVER {
		err = oryx_shmring_get(ring, &data, &data_size);
		if (err) {
			continue;
		}
		
		if (data) {
			rx_pkts ++;
		}
	}
	
	oryx_task_deregistry_id(pthread_self());
	return NULL;
}

static __oryx_always_inline__
void * shmring_wp (void *r)
{
	uint32_t	times = 0;
	int			sleeps = 0,
				err;
	char		data[NR_SHMRING_ELEMENT_SIZE];
	struct oryx_shmring_t *ring = (struct oryx_shmring_t *)r;

	FOREVER {
		size_t nr_bytes = oryx_pattern_generate(data, NR_SHMRING_ELEMENT_SIZE);
		err = oryx_shmring_put(ring, data, nr_bytes);
		if (err) {
			continue;
		}
		tx_pkts ++;
	}
	oryx_task_deregistry_id(pthread_self());
	return NULL;
}

static struct oryx_task_t rp_task =
{
	.module		= THIS,
	.sc_alias	= "Share Ring RP Task",
	.fn_handler	= shmring_rp,
	.lcore_mask	= INVALID_CORE,
	.ul_prio	= KERNEL_SCHED,
	.argc		= 0,
	.argv		= NULL,
	.ul_flags	= 0,	/** Can not be recyclable. */
};


static struct oryx_task_t wp_task =
{
	.module		= THIS,
	.sc_alias	= "Share Ring WP Task",
	.fn_handler	= shmring_wp,
	.lcore_mask	= INVALID_CORE,
	.ul_prio	= KERNEL_SCHED,
	.argc		= 0,
	.argv		= NULL,
	.ul_flags	= 0,	/** Can not be recyclable. */
};

int main(
	int 	__oryx_unused__	argc,
	char	__oryx_unused__	** argv
)
{
	int err;
	
	oryx_initialize();
	oryx_register_sighandler(SIGINT,	sigint_handler);
	oryx_register_sighandler(SIGTERM,	sigint_handler);

	err = oryx_shmring_create("test_shmring",
			0,
			0,
			0,
			&my_ring);
	if (err) {
		return 0;
	}

	oryx_shmring_dump(my_ring);
	
	wp_task.argc = 1;
	wp_task.argv = my_ring;
	oryx_task_registry(&wp_task);

	rp_task.argc = 1;
	rp_task.argv = my_ring;
	oryx_task_registry(&rp_task);

	oryx_task_launch();

	FOREVER {
		sleep(3);		
		if(!running)
			break;
		oryx_shmring_dump(&my_ring);
		fprintf(stdout, "rx_pkts %lu, tx_pkts %lu\n", rx_pkts, tx_pkts);
	};

	oryx_shmring_destroy(my_ring);

	return 0;
}

