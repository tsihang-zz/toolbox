#include "oryx.h"

static int quit;
static struct oryx_lq_ctx_t *lq0;
static struct CounterCtx ctx;
static counter_id nr_eq_refcnt_id;
static counter_id nr_dq_refcnt_id;

struct lq_element_t {
	struct lq_element_t *next, *prev;
	int value;
};

#define lq_element_size	(sizeof(struct lq_element_t))

static
void * dequeue_handler (void __oryx_unused_param__ *r)
{
		struct lq_element_t *lqe;
		
		FOREVER {
			if (quit) {
				/* drunk out all elements before thread quit */					
				while (NULL != (lqe = oryx_lq_dequeue(lq0))){
					free(lqe);
					oryx_counter_inc(&ctx, nr_dq_refcnt_id);
				}

				break;
			}
			
			lqe = oryx_lq_dequeue(lq0);
			if (likely (lqe)) {
				free(lqe);
				oryx_counter_inc(&ctx, nr_dq_refcnt_id);
			}
		}

		oryx_task_deregistry_id(pthread_self());
		return NULL;
}

static
void * enqueue_handler (void __oryx_unused_param__ *r)
{
		struct lq_element_t *lqe;
		static uint32_t sand = 1315423911;
		
		FOREVER {
			if (quit)
				break;

			lqe = malloc(sizeof(struct lq_element_t));
			BUG_ON(lqe == NULL);
			lqe->value = next_rand_(&sand);
			oryx_lq_enqueue(lq0, lqe);
			oryx_counter_inc(&ctx, nr_eq_refcnt_id);
		}

		oryx_task_deregistry_id(pthread_self());
		return NULL;
}

static struct oryx_task_t dequeue = {
		.module 		= THIS,
		.sc_alias		= "Dequeue Task",
		.fn_handler 	= dequeue_handler,
		.lcore_mask		= 0x08,
		.ul_prio		= KERNEL_SCHED,
		.argc			= 0,
		.argv			= NULL,
		.ul_flags		= 0,	/** Can not be recyclable. */
};

static struct oryx_task_t enqueue = {
		.module 		= THIS,
		.sc_alias		= "Enqueue Task",
		.fn_handler 	= enqueue_handler,
		.lcore_mask		= 0x04,
		.ul_prio		= KERNEL_SCHED,
		.argc			= 0,
		.argv			= NULL,
		.ul_flags		= 0,	/** Can not be recyclable. */
};

static void lq_sigint(int sig)
{
	fprintf (stdout, "signal %d ...\n", sig);
	quit = 1;
}

static void lq_terminal(void)
{
	uint64_t eq, dq;
	
	eq = oryx_counter_get(&ctx, nr_eq_refcnt_id);
	dq = oryx_counter_get(&ctx, nr_dq_refcnt_id);

	fprintf (stdout, "nr_eq_refcnt_id %ld, nr_dq_refcnt_id %ld, buffered %ld\n",
		eq, dq, (eq - dq));
	
	oryx_lq_destroy(lq0);
}

static void lq_runtime(void)
{
	static struct timeval start, end;
	static uint64_t nr_eq_swap_bytes_prev = 0, nr_eq_swap_elements_prev = 0;
	static uint64_t nr_dq_swap_bytes_prev = 0, nr_dq_swap_elements_prev = 0;
	uint64_t	nr_eq_swap_bytes_cur = 0, nr_eq_swap_elements_cur = 0;
	uint64_t	nr_dq_swap_bytes_cur = 0, nr_dq_swap_elements_cur = 0;	
	uint64_t	nr_cost_us;
	uint64_t	eq, dq;
	char pps_str[20], bps_str[20];

	gettimeofday(&end, NULL);
	
	/* cost before last time. */
	nr_cost_us = tm_elapsed_us(&start, &end);			
	eq = oryx_counter_get(&ctx, nr_eq_refcnt_id);
	dq = oryx_counter_get(&ctx, nr_dq_refcnt_id);
	
	nr_eq_swap_bytes_cur		=	(eq * lq_element_size) - nr_eq_swap_bytes_prev;
	nr_eq_swap_elements_cur 	=	eq - nr_eq_swap_elements_prev;
	
	nr_dq_swap_bytes_cur		=	(dq * lq_element_size) - nr_dq_swap_bytes_prev;
	nr_dq_swap_elements_cur 	=	dq - nr_dq_swap_elements_prev;
	
	nr_eq_swap_bytes_prev		= (eq * lq_element_size);
	nr_eq_swap_elements_prev	= eq;
	
	nr_dq_swap_bytes_prev		= (dq * lq_element_size);
	nr_dq_swap_elements_prev	= dq;
	
	gettimeofday(&start, NULL);
	
	fprintf (stdout, "cost %lu us, eq %ld (pps %s, bps %s), dq %ld (pps %s, bps %s), buffered %ld\n",
		nr_cost_us,
		eq, oryx_fmt_speed(fmt_pps(nr_cost_us, nr_eq_swap_elements_cur), pps_str, 0, 0), oryx_fmt_speed(fmt_bps(nr_cost_us, nr_eq_swap_bytes_cur), bps_str, 0, 0),
		dq, oryx_fmt_speed(fmt_pps(nr_cost_us, nr_dq_swap_elements_cur), pps_str, 0, 0), oryx_fmt_speed(fmt_bps(nr_cost_us, nr_dq_swap_bytes_cur), bps_str, 0, 0),
		(eq - dq));

}

static void lq_env_init(void)
{
	memset(&ctx, 0, sizeof(struct CounterCtx));

	nr_eq_refcnt_id = oryx_register_counter("enqueue times", "nr_eq_refcnt_id", &ctx);
	BUG_ON(nr_eq_refcnt_id != 1);
	nr_dq_refcnt_id = oryx_register_counter("dequeue times", "nr_dq_refcnt_id", &ctx);
	BUG_ON(nr_dq_refcnt_id != 2);
	
	oryx_counter_get_array_range(1, 
		atomic_read(&ctx.curr_id), &ctx);

}

int main (
        int     __oryx_unused_param__   argc,
        char    __oryx_unused_param__   ** argv
)

{	
	uint32_t	lq_cfg = 0;

	oryx_initialize();

	oryx_register_sighandler(SIGINT, lq_sigint);
	oryx_register_sighandler(SIGTERM, lq_sigint);

	lq_env_init();

	/* new queue */
	oryx_lq_new("A new list queue", lq_cfg, &lq0);
	oryx_lq_dump(lq0);
	
	oryx_task_registry(&enqueue);
	oryx_task_registry(&dequeue);

	oryx_task_launch();
	
	FOREVER {
		if (quit) {
			/* wait for dequeue finish. */
			sleep (3);
			lq_terminal();
			break;
		}
		lq_runtime();
		sleep (3);
	};
	
	return 0;
}

