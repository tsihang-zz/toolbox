#ifndef QUEUE_H
#define QUEUE_H

/* Define a queue for storing flows */
struct qctx_t {
    void		*top;
    void		*bot;
	const char	*name;
    uint32_t	len;

	int (*insert)(struct qctx_t *q, void *f);
	int (*remove)(struct qctx_t *q, void *f);
	
#ifdef DBG_PERF
    uint32_t dbg_maxlen;
#endif /* DBG_PERF */
    os_mutex_t m;
};

struct element_fn_t {
	int (*insert)(struct qctx_t *q, void *v);
	int (*remove)(struct qctx_t *q, void *v);
	int (*verify)(struct qctx_t *q, void *v);
	void (*init)(void *v);
	void (*deinit)(void *v);
};

void fq_sample(void);

#endif
