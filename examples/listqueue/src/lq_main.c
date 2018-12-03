#include "oryx.h"

#define MAX_LQ_NUM	4
static int quit;
static struct oryx_lq_ctx_t *lqset[MAX_LQ_NUM];
static uint64_t	dq_x[MAX_LQ_NUM];
static uint64_t eq_x[MAX_LQ_NUM];

struct lq_element_t {
	struct lq_prefix_t lq;
	int value;
}__attribute__((aligned(64)));


#define lq_element_size	(sizeof(struct lq_element_t))

#define VLIB_ENQUEUE_HANDLER_EXITED	(1 << 0)
typedef struct vlib_main_t {
	volatile uint32_t ul_flags;
} vlib_main_t;

vlib_main_t vlib_main;

static __oryx_always_inline__
struct oryx_lq_ctx_t * fetch_lq(uint64_t sand, struct oryx_lq_ctx_t **lq) {
	/* fetch an LQ */
	(*lq) = lqset[sand % MAX_LQ_NUM];
}

static
void * dequeue_handler (void __oryx_unused__ *r)
{
		struct lq_element_t *lqe;
		struct oryx_lq_ctx_t *lq = *(struct oryx_lq_ctx_t **)r;
		vlib_main_t *vm = &vlib_main;

		fprintf(stdout, "Starting thread %lu, %p\n", pthread_self(), lq);
		
		FOREVER {
			if (quit) {
				/* wait for enqueue handler exit. */
				while (vm->ul_flags & VLIB_ENQUEUE_HANDLER_EXITED)
					break;
				fprintf (stdout, "dequeue exiting ... ");
				/* drunk out all elements before thread quit */					
				while (NULL != (lqe = oryx_lq_dequeue(lq))){
					free(lqe);
				}
				fprintf (stdout, " exited!\n");
				break;
			}
			
			lqe = oryx_lq_dequeue(lq);
			if (lqe != NULL) {
				free(lqe);
				lqe = NULL;
			}
		}

		oryx_task_deregistry_id(pthread_self());
		return NULL;
}

static
void * enqueue_handler (void __oryx_unused__ *r)
{
		struct lq_element_t *lqe;
		static uint32_t sand = 1315423911;
		struct oryx_lq_ctx_t *lq;
		vlib_main_t *vm = &vlib_main;
		
		FOREVER {
			if (quit) {
				fprintf (stdout, "enqueue exited!\n");
				vm->ul_flags |= VLIB_ENQUEUE_HANDLER_EXITED;
				break;
			}
	
			lqe = malloc(lq_element_size);
			BUG_ON(lqe == NULL);
			memset(lqe, lq_element_size, 0);
			
			lqe->value = next_rand_(&sand);
			
			fetch_lq(sand, &lq);
			oryx_lq_enqueue(lq, lqe);
			/* virtual machine is not very efficient on memory allocation. */
			/* usleep(1); */
		}

		oryx_task_deregistry_id(pthread_self());
		return NULL;
}

static struct oryx_task_t dequeue = {
		.module 		= THIS,
		.sc_alias		= "Dequeue Task0",
		.fn_handler 		= dequeue_handler,
		.lcore_mask		= 0x08,
		.ul_prio		= KERNEL_SCHED,
		.argc			= 1,
		.argv			= NULL,
		.ul_flags		= 0,	/** Can not be recyclable. */
};

static struct oryx_task_t enqueue = {
		.module 		= THIS,
		.sc_alias		= "Enqueue Task",
		.fn_handler 		= enqueue_handler,
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
	struct oryx_lq_ctx_t *lq;

	for (i = 0; i < MAX_LQ_NUM; i ++) {
		lq = lqset[i];
		fprintf (stdout, "LQ[%d], nr_eq_refcnt %ld, nr_dq_refcnt %ld, buffered %lu(%d)\n",
			i, lq->nr_eq_refcnt, lq->nr_dq_refcnt, (lq->nr_eq_refcnt - lq->nr_dq_refcnt), lq->len);
		oryx_lq_destroy(lq);
	}
}

static void lq_runtime(void)
{
	int i;
	static size_t lqe_size = lq_element_size;
	static struct timeval start, end;
	static uint64_t nr_eq_swap_bytes_prev[MAX_LQ_NUM] = {0}, nr_eq_swap_elements_prev[MAX_LQ_NUM] = {0};
	static uint64_t nr_dq_swap_bytes_prev[MAX_LQ_NUM] = {0}, nr_dq_swap_elements_prev[MAX_LQ_NUM] = {0};
	uint64_t	nr_eq_swap_bytes_cur = 0, nr_eq_swap_elements_cur = 0;
	uint64_t	nr_dq_swap_bytes_cur = 0, nr_dq_swap_elements_cur = 0;	
	uint64_t	nr_cost_us;
	uint64_t	eq, dq;
	char pps_str[20], bps_str[20];
	struct oryx_lq_ctx_t *lq;
	static oryx_file_t *fp;
	const char *lq_runtime_file = "./lq_runtime_summary.txt";
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
	
	gettimeofday(&start, NULL);

	fprintf (fp, "Cost %lu us \n", nr_cost_us);
	for (i = 0; i < MAX_LQ_NUM; i ++) {
		lq = lqset[i];
		dq = lq->nr_dq_refcnt;
		eq = lq->nr_eq_refcnt;
		
		nr_eq_swap_bytes_cur		=	(eq * lqe_size) - nr_eq_swap_bytes_prev[i];
		nr_eq_swap_elements_cur 	=	eq - nr_eq_swap_elements_prev[i];
		
		nr_dq_swap_bytes_cur		=	(dq * lqe_size) - nr_dq_swap_bytes_prev[i];
		nr_dq_swap_elements_cur 	=	dq - nr_dq_swap_elements_prev[i];
		
		nr_eq_swap_bytes_prev[i]	=	(eq * lqe_size);
		nr_eq_swap_elements_prev[i]	=	eq;
		
		nr_dq_swap_bytes_prev[i]	=	(dq * lqe_size);
		nr_dq_swap_elements_prev[i]	=	dq;

		fprintf (fp, "LQ[%d], eq %ld (pps %s, bps %s), dq %ld (pps %s, bps %s), buffered %lu(%d)\n",
			i,
			eq, oryx_fmt_speed(fmt_pps(nr_cost_us, nr_eq_swap_elements_cur), pps_str, 0, 0), oryx_fmt_speed(fmt_bps(nr_cost_us, nr_eq_swap_bytes_cur), bps_str, 0, 0),
			dq, oryx_fmt_speed(fmt_pps(nr_cost_us, nr_dq_swap_elements_cur), pps_str, 0, 0), oryx_fmt_speed(fmt_bps(nr_cost_us, nr_dq_swap_bytes_cur), bps_str, 0, 0),
			(lq->nr_eq_refcnt - lq->nr_dq_refcnt), lq->len);
		fflush(fp);

	}
}

int main (
        int     __oryx_unused__   argc,
        char    __oryx_unused__   ** argv
)

{	
	int i;
	uint32_t	lq_cfg = 0;
	struct oryx_lq_ctx_t *lq;
	
	oryx_initialize();

	oryx_register_sighandler(SIGINT, lq_sigint);
	oryx_register_sighandler(SIGTERM, lq_sigint);

	for (i = 0; i < MAX_LQ_NUM; i ++) {
		/* new queue */
		oryx_lq_new("A new list queue", lq_cfg, (void **)&lq);
		oryx_lq_dump(lq);
		lqset[i] = lq;
		
		char name[64] = {0};
		sprintf (name, "Dequeue Task%d", i);
		struct oryx_task_t *t = malloc (sizeof(struct oryx_task_t));
		BUG_ON(t == NULL);
		memset(t, sizeof (struct oryx_task_t), 0);
		memcpy(t, &dequeue, sizeof (struct oryx_task_t));
		t->lcore_mask = INVALID_CORE;
		t->argc = 1;
		t->argv = &lqset[i];
		t->sc_alias = strdup(name);
		oryx_task_registry(t);

	}

	oryx_task_registry(&enqueue);
	
	oryx_task_launch();
	
	FOREVER {
		if (quit) {
			/* wait for handlers of enqueue and dequeue finish. */
			sleep (3);
			/* last runtime logging. */
			lq_runtime();
			lq_terminal();
			break;
		} else {
			lq_runtime();
			sleep (3);
		}
	};
	
	return 0;
}

