#include "oryx.h"

#define MAX_LQ_NUM	2
static int quit;
static struct oryx_lq_ctx_t *lqset[MAX_LQ_NUM];
static struct CounterCtx ctx;
static counter_id nr_eq_refcnt_id;
static counter_id nr_dq_refcnt_id;

struct lq_element_t {
	struct lq_prefix_t lq;
	int value;
}__attribute__((aligned(64)));

#define lq_element_size	(sizeof(struct lq_element_t))

static __oryx_always_inline__
struct oryx_lq_ctx_t * fetch_lq(uint64_t sand, struct oryx_lq_ctx_t **lq) {
	/* fetch an LQ */
	(*lq) = lqset[sand % MAX_LQ_NUM];
}

static
void * dequeue_handler (void __oryx_unused_param__ *r)
{
		struct lq_element_t *lqe;
		struct oryx_lq_ctx_t *lq = *(struct oryx_lq_ctx_t **)r;

		fprintf(stdout, "Starting thread %lu, %p\n", pthread_self(), lq);
		
		FOREVER {
			if (quit) {
				/* drunk out all elements before thread quit */					
				while (NULL != (lqe = oryx_lq_dequeue(lq))){
					free(lqe);
					oryx_counter_inc(&ctx, nr_dq_refcnt_id);
				}

				break;
			}
			
			lqe = oryx_lq_dequeue(lq);
			if (lqe != NULL) {
				free(lqe);
				lqe = NULL;
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
		struct oryx_lq_ctx_t *lq;
		
		FOREVER {
			if (quit)
				break;
	
			lqe = malloc(lq_element_size);
			BUG_ON(lqe == NULL);
			memset(lqe, lq_element_size, 0);
			
			lqe->value = next_rand_(&sand);
			
			fetch_lq(sand, &lq);
			oryx_lq_enqueue(lq, lqe);
			oryx_counter_inc(&ctx, nr_eq_refcnt_id);
			usleep(1);
		}

		oryx_task_deregistry_id(pthread_self());
		return NULL;
}

static struct oryx_task_t dequeue = {
		.module 		= THIS,
		.sc_alias		= "Dequeue Task0",
		.fn_handler 	= dequeue_handler,
		.lcore_mask		= 0x08,
		.ul_prio		= KERNEL_SCHED,
		.argc			= 1,
		.argv			= NULL,
		.ul_flags		= 0,	/** Can not be recyclable. */
};

static struct oryx_task_t dequeue0 = {
		.module 		= THIS,
		.sc_alias		= "Dequeue Task0",
		.fn_handler 	= dequeue_handler,
		.lcore_mask		= 0x08,
		.ul_prio		= INVALID_CORE,
		.argc			= 1,
		.argv			= &lqset[0],
		.ul_flags		= 0,	/** Can not be recyclable. */
};

static struct oryx_task_t dequeue1 = {
		.module 		= THIS,
		.sc_alias		= "Dequeue Task1",
		.fn_handler 	= dequeue_handler,
		.lcore_mask		= 0x08,
		.ul_prio		= INVALID_CORE,
		.argc			= 1,
		.argv			= &lqset[1],
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
	int i;
	uint64_t eq, dq;
	struct oryx_lq_ctx_t *lq;
	
	eq = oryx_counter_get(&ctx, nr_eq_refcnt_id);
	dq = oryx_counter_get(&ctx, nr_dq_refcnt_id);

	fprintf (stdout, "nr_eq_refcnt_id %ld, nr_dq_refcnt_id %ld, buffered %ld\n",
		eq, dq, (eq - dq));

	for (i = 0; i < MAX_LQ_NUM; i ++) {
		lq = lqset[i];
		oryx_lq_destroy(lq);
	}
}

static void lq_runtime(void)
{
	int i;
	static struct timeval start, end;
	static uint64_t nr_eq_swap_bytes_prev = 0, nr_eq_swap_elements_prev = 0;
	static uint64_t nr_dq_swap_bytes_prev = 0, nr_dq_swap_elements_prev = 0;
	uint64_t	nr_eq_swap_bytes_cur = 0, nr_eq_swap_elements_cur = 0;
	uint64_t	nr_dq_swap_bytes_cur = 0, nr_dq_swap_elements_cur = 0;	
	uint64_t	nr_cost_us;
	uint64_t	eq, dq;
	uint64_t	nr_buffed = 0;
	char pps_str[20], bps_str[20];
	struct oryx_lq_ctx_t *lq;
	static oryx_file_t *fp;
	const char *lq_runtime_file = "./lq_runtime_summary.txt";
	int each;
	char cat_null[128] = "cat /dev/null > ";

	strcat(cat_null, lq_runtime_file);
	system(cat_null);

	if(!fp) {
		fp = fopen(lq_runtime_file, "a+");
		if(!fp) fp = stdout;
	}

	gettimeofday(&end, NULL);
	
	/* cost before last time. */
	nr_cost_us = tm_elapsed_us(&start, &end);
	dq = oryx_counter_get(&ctx, nr_dq_refcnt_id);
	eq = oryx_counter_get(&ctx, nr_eq_refcnt_id);
	
	nr_eq_swap_bytes_cur		=	(eq * lq_element_size) - nr_eq_swap_bytes_prev;
	nr_eq_swap_elements_cur 	=	eq - nr_eq_swap_elements_prev;
	
	nr_dq_swap_bytes_cur		=	(dq * lq_element_size) - nr_dq_swap_bytes_prev;
	nr_dq_swap_elements_cur 	=	dq - nr_dq_swap_elements_prev;
	
	nr_eq_swap_bytes_prev		=	(eq * lq_element_size);
	nr_eq_swap_elements_prev	=	eq;
	
	nr_dq_swap_bytes_prev		=	(dq * lq_element_size);
	nr_dq_swap_elements_prev	=	dq;
	
	gettimeofday(&start, NULL);

	nr_buffed = 0;
	for (i = 0; i < MAX_LQ_NUM; i ++) {
		lq = lqset[i];
		nr_buffed += lq->len; 
	}
	
	fprintf (fp, "cost %lu us, eq %ld (pps %s, bps %s), dq %ld (pps %s, bps %s), buffered %ld\n",
		nr_cost_us,
		eq, oryx_fmt_speed(fmt_pps(nr_cost_us, nr_eq_swap_elements_cur), pps_str, 0, 0), oryx_fmt_speed(fmt_bps(nr_cost_us, nr_eq_swap_bytes_cur), bps_str, 0, 0),
		dq, oryx_fmt_speed(fmt_pps(nr_cost_us, nr_dq_swap_elements_cur), pps_str, 0, 0), oryx_fmt_speed(fmt_bps(nr_cost_us, nr_dq_swap_bytes_cur), bps_str, 0, 0),
		nr_buffed);
	fflush(fp);

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
	int i;
	uint32_t	lq_cfg = 0;
	struct oryx_lq_ctx_t *lq;
	
	oryx_initialize();

	oryx_register_sighandler(SIGINT, lq_sigint);
	oryx_register_sighandler(SIGTERM, lq_sigint);

	lq_env_init();

	for (i = 0; i < MAX_LQ_NUM; i ++) {
		/* new queue */
		oryx_lq_new("A new list queue", lq_cfg, &lq);
		oryx_lq_dump(lq);
		lqset[i] = lq;
	}

	oryx_task_registry(&dequeue0);
	oryx_task_registry(&dequeue1);
	oryx_task_registry(&enqueue);
	
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

