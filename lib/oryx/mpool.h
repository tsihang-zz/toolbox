#ifndef MPOOL_H
#define MPOOL_H

struct element_fn_t {
	int (*insert)(struct qctx_t *q, void *v);
	int (*remove)(struct qctx_t *q, void *v);
	int (*verify)(struct qctx_t *q, void *v);
	void (*init)(struct qctx_t *q, void *v);
	void (*deinit)(struct qctx_t *q, void *v);
};

extern void mpool_init(void ** mp, const char *mp_name,
			int nb_steps, int nb_bytes_per_element, int cache_line_size,
			struct element_fn_t *efn);

extern void mpool_uninit(void * mp);
extern void mpool_free (void * mp , void *elem);
extern void * mpool_alloc(void * mp);


void mpool_sample(void);

#endif

