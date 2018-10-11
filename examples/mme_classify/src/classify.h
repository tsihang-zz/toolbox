#ifndef CLASSIFY_H
#define CLASSIFY_H

#include "main.h"

#define HAVE_CDR_CSV_FILE

#define MAX_LQ_NUM	8
#define LINE_LENGTH	256

typedef struct vlib_threadvar_ctx_t {
	struct oryx_lq_ctx_t *lq;
	uint32_t			unique_id;
	uint64_t			nr_unclassified_refcnt;
}vlib_threadvar_ctx_t;

extern vlib_threadvar_ctx_t vlib_threadvar_main[];

struct lq_element_t {
	struct lq_prefix_t lp;
	/* used to find an unique MME. */
	char mme_ip[32];
	uint64_t event_start_time;
	char value[LINE_LENGTH];
	size_t valen;
};

#define lq_element_size	(sizeof(struct lq_element_t))

static __oryx_always_inline__
struct lq_element_t *lqe_alloc(void)
{
	const size_t lqe_size = lq_element_size;
	struct lq_element_t *lqe;
	
	lqe = malloc(lqe_size);
	if(lqe) {
		lqe->lp.lnext = lqe->lp.lprev = NULL;
		lqe->valen = 0;
		lqe->event_start_time = 0;
		lqe->value[0] = 0;
		memset((void *)&lqe->mme_ip[0], 0, 32);
		memset((void *)&lqe->value[0], 0, LINE_LENGTH);
	}
	return lqe;
}

extern void classify_env_init(vlib_main_t *vm);
extern void classify_terminal(void);
extern void classify_runtime(void);
extern void do_dispatch(const char *value, size_t vlen);
extern void do_classify(vlib_threadvar_ctx_t *vtc, const struct lq_element_t *lqe);
extern void classify_tmr_handler(struct oryx_timer_t *tmr, int __oryx_unused_param__ argc, 
                char __oryx_unused_param__**argv);

#endif

