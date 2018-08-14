#include "oryx.h"

#define DRM_LQ_NUM	2

struct lq_element_t {
	struct lq_element_t *next, *prev;
	uint32_t tmsi;
	uint32_t imsi;
};

#define lq_element_size	(sizeof(struct lq_element_t))

struct drm_lq_ctx_t {
	int	drm_lq_id;
	struct oryx_lq_ctx_t *lq;
	uint64_t nr_eq_refcnt_id;
	uint64_t nr_dq_refcnt_id;
	struct oryx_htable_t *ht;
};

static struct drm_lq_ctx_t drm_lq_ctx[] = {
	{
		.drm_lq_id = 0,
	},
	{
		.drm_lq_id = 1,
	}
};

static int quit;

static void
ht_lqe_free (const ht_value_t __oryx_unused_param__ v)
{
	/** To avoid warnings. */
}

static ht_key_t
ht_lqe_hval (struct oryx_htable_t *ht,
		const ht_value_t v, uint32_t s) 
{
     uint8_t *d = (uint8_t *)v;
     uint32_t i;
     uint32_t hv = 0;

     for (i = 0; i < s; i++) {
         if (i == 0)      hv += (((uint32_t)*d++));
         else if (i == 1) hv += (((uint32_t)*d++) * s);
         else             hv *= (((uint32_t)*d++) * i) + s + i;
     }

     hv *= s;
     hv %= ht->array_size;
     
     return hv;
}

static int
ht_lqe_cmp (const ht_value_t v1, 
		uint32_t s1,
		const ht_value_t v2,
		uint32_t s2)
{
	int xret = 0;

	if (!v1 || !v2 ||s1 != s2 ||
		memcmp(v1, v2, s2))
		xret = 1;
	
	return xret;
}


static __oryx_always_inline__
void lq_element_free(struct lq_element_t *lqe)
{
	if (likely(lqe)) free(lqe);
	else fprintf (stdout, "%p\n", lqe);
}

static void *
drm_dequeue_handler (void __oryx_unused_param__ *r)
{
		struct lq_element_t *lqe;
		int lq_id = *(int *)r;
		struct drm_lq_ctx_t *drm_lq = &drm_lq_ctx[lq_id];
		char lq_file[32] = "./", lq_file_result[32] = "./";
		oryx_file_t * fp, *fp_result;
		
		sprintf (lq_file, "%s%d.txt", "DRM_LQ", lq_id);
		oryx_path_remove(lq_file);

		sprintf (lq_file_result, "%s%d_result.txt", "DRM_LQ", lq_id);
		oryx_path_remove(lq_file_result);

		oryx_mkfile(lq_file, &fp, "a+");
		BUG_ON(fp == NULL);

		oryx_mkfile(lq_file_result, &fp_result, "a+");
		BUG_ON(fp_result == NULL);

		FOREVER {
			lqe = NULL;
			
			if (quit) {
				/* drunk out all elements before thread quit */					
				while (NULL != (lqe = oryx_lq_dequeue(drm_lq->lq))) {
					drm_lq->nr_dq_refcnt_id ++;
					fprintf (fp, "%p\t%8u\t%8u\n", lqe, lqe->tmsi, lqe->imsi);
					fflush(fp);

					lq_element_free(lqe);
				}

				break;
			}
			
			lqe = oryx_lq_dequeue(drm_lq->lq);
			if (likely (lqe)) {
				drm_lq->nr_dq_refcnt_id ++;
				fprintf (fp, "%p\t%8u\t%8u\n", lqe, lqe->tmsi, lqe->imsi);
				fflush(fp);

				lq_element_free(lqe);
			}
		}

		oryx_task_deregistry_id(pthread_self());
		return NULL;
}

static void *
dispatcher_handler (void __oryx_unused_param__ *r)
{
	struct lq_element_t *lqe;
	static uint32_t sand = 1315423911;
	struct drm_lq_ctx_t *drm_lq;
	
	FOREVER {
		if (quit)
			break;
		
		usleep (100000);

		lqe = malloc(sizeof(struct lq_element_t));
		if(unlikely(!lqe)) {
			usleep (1000);
			continue;
		}
		
		lqe->tmsi	= (next_rand_(&sand) % 10);
		drm_lq		= &drm_lq_ctx[lqe->tmsi % DRM_LQ_NUM];
		drm_lq->nr_eq_refcnt_id ++;
		oryx_lq_enqueue(drm_lq->lq, lqe);
	}
	
	oryx_task_deregistry_id(pthread_self());
	return NULL;

}

static struct oryx_task_t dequeue0 = {
		.module 		= THIS,
		.sc_alias		= "Dequeue0 Task",
		.fn_handler 	= drm_dequeue_handler,
		.lcore_mask		= 0x02,
		.ul_prio		= KERNEL_SCHED,
		.argc			= 1,
		.argv			= &drm_lq_ctx[0].drm_lq_id,
		.ul_flags		= 0,	/** Can not be recyclable. */
};

static struct oryx_task_t dequeue1 = {
		.module 		= THIS,
		.sc_alias		= "Dequeue1 Task",
		.fn_handler 	= drm_dequeue_handler,
		.lcore_mask		= 0x04,
		.ul_prio		= KERNEL_SCHED,
		.argc			= 1,
		.argv			= &drm_lq_ctx[1].drm_lq_id,
		.ul_flags		= 0,	/** Can not be recyclable. */
};

static struct oryx_task_t dispatcher = {
		.module 		= THIS,
		.sc_alias		= "Dispatcher Task",
		.fn_handler 	= dispatcher_handler,
		.lcore_mask		= 0x08,
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

static void lq_terminal(int lq_id)
{
	uint64_t eq, dq;
	struct drm_lq_ctx_t *drm_lq = &drm_lq_ctx[lq_id];
	
	eq = drm_lq->nr_eq_refcnt_id;
	dq = drm_lq->nr_dq_refcnt_id;

	fprintf (stdout, "nr_eq_refcnt_id %ld, nr_dq_refcnt_id %ld, buffered %ld\n",
		eq, dq, (eq - dq));
	
	oryx_lq_destroy(drm_lq->lq);
}

static void lq_runtime(int lq_id)
{
	static struct timeval start[DRM_LQ_NUM], end[DRM_LQ_NUM];
	static uint64_t nr_eq_swap_bytes_prev[DRM_LQ_NUM] = {0}, nr_eq_swap_elements_prev[DRM_LQ_NUM] = {0};
	static uint64_t nr_dq_swap_bytes_prev[DRM_LQ_NUM] = {0}, nr_dq_swap_elements_prev[DRM_LQ_NUM] = {0};
	uint64_t	nr_eq_swap_bytes_cur = 0, nr_eq_swap_elements_cur = 0;
	uint64_t	nr_dq_swap_bytes_cur = 0, nr_dq_swap_elements_cur = 0;	
	uint64_t	nr_cost_us;
	uint64_t	eq, dq;
	char pps_str[20], bps_str[20];
	struct drm_lq_ctx_t *drm_lq = &drm_lq_ctx[lq_id];
	
	gettimeofday(&end[lq_id], NULL);
	
	/* cost before last time. */
	nr_cost_us = tm_elapsed_us(&start[lq_id], &end[lq_id]);			
	eq = drm_lq->nr_eq_refcnt_id;
	dq = drm_lq->nr_dq_refcnt_id;

	
	nr_eq_swap_bytes_cur		= (eq * lq_element_size) - nr_eq_swap_bytes_prev[lq_id];
	nr_eq_swap_elements_cur 	= eq - nr_eq_swap_elements_prev[lq_id];
	
	nr_dq_swap_bytes_cur		= (dq * lq_element_size) - nr_dq_swap_bytes_prev[lq_id];
	nr_dq_swap_elements_cur 	= dq - nr_dq_swap_elements_prev[lq_id];
	
	nr_eq_swap_bytes_prev[lq_id]		= (eq * lq_element_size);
	nr_eq_swap_elements_prev[lq_id]	= eq;
	
	nr_dq_swap_bytes_prev[lq_id]		= (dq * lq_element_size);
	nr_dq_swap_elements_prev[lq_id]	= dq;
	
	gettimeofday(&start[lq_id], NULL);
	
	fprintf (stdout, "lq_id %d, eq %ld (pps %s, bps %s), dq %ld (pps %s, bps %s), buffered %ld\n",
		lq_id,
		eq, oryx_fmt_speed(fmt_pps(nr_cost_us, nr_eq_swap_elements_cur), pps_str, 0, 0), oryx_fmt_speed(fmt_bps(nr_cost_us, nr_eq_swap_bytes_cur), bps_str, 0, 0),
		dq, oryx_fmt_speed(fmt_pps(nr_cost_us, nr_dq_swap_elements_cur), pps_str, 0, 0), oryx_fmt_speed(fmt_bps(nr_cost_us, nr_dq_swap_bytes_cur), bps_str, 0, 0),
		(eq - dq));

}

static void lq_env_init(void)
{
	int i;
	char name[32];
	uint32_t	lq_cfg = 0;

	for (i = 0; i < DRM_LQ_NUM; i ++) {
		struct drm_lq_ctx_t *drm_lq = &drm_lq_ctx[i];
		/* Create DRM LQs */
		memset(&name[0], 0, 32);
		sprintf (name, "DRM LQ%d", i);
		oryx_lq_new(name, lq_cfg, &drm_lq->lq);
		oryx_lq_dump(drm_lq->lq);

		drm_lq->nr_eq_refcnt_id = 0;
		drm_lq->nr_dq_refcnt_id = 0;
		drm_lq->drm_lq_id = i;

		drm_lq->ht = oryx_htable_init(DEFAULT_HASH_CHAIN_SIZE, 
							ht_lqe_hval, ht_lqe_cmp, ht_lqe_free, 0);
	}
}

int main (
        int     __oryx_unused_param__   argc,
        char    __oryx_unused_param__   ** argv
)

{	
	fprintf (stdout, "id= %d\n", drm_lq_ctx[1].drm_lq_id);
	
	oryx_initialize();

	oryx_register_sighandler(SIGINT, lq_sigint);
	oryx_register_sighandler(SIGTERM, lq_sigint);

	lq_env_init();
	
	oryx_task_registry(&dispatcher);
	oryx_task_registry(&dequeue0);
	oryx_task_registry(&dequeue1);

	oryx_task_launch();

	int i;
	FOREVER {
		if (quit) {
			/* wait for dequeue finish. */
			sleep (3);
			
			for (i = 0; i < DRM_LQ_NUM; i ++)
				lq_terminal(i);
			break;
		}
		for (i = 0; i < DRM_LQ_NUM; i ++)
			lq_runtime(i);
		sleep (3);
	};
	
	return 0;
}

