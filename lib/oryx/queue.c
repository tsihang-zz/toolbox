#include "oryx.h"

static struct qctx_t *FlowQueueInit (const char *fq_name,	struct qctx_t *q)
{
    if (q != NULL) {
        memset(q, 0, sizeof(struct qctx_t));
		q->name = fq_name;
        FQLOCK_INIT(q);
    }
    return q;
}

int fq_new(const char *fq_name, struct qctx_t ** fq)
{
    struct qctx_t *q = (struct qctx_t *)malloc(sizeof(struct qctx_t));
    if (q == NULL) {
        oryx_loge(0,
			"Fatal error encountered in FlowQueueNew. Exiting...");
        exit(EXIT_SUCCESS);
    }
    (*fq) = FlowQueueInit(fq_name, q);
	
    return 0;
}

/**
 *  \brief Destroy a queue
 *
 *  \param q the queue to destroy
 */
void fq_destroy (struct qctx_t * fq)
{
	FQLOCK_DESTROY(fq);
}

void fq_dump(struct qctx_t *fq)
{
	printf("%16s%32s\n", "qname: ", fq->name);
	printf("%16s%32d\n", "qlen: ", fq->len);
}

