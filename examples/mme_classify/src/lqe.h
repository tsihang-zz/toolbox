#ifndef LQE_H
#define LQE_H

#define LQE_HAVE_IMSI	(1 << 0)
struct lq_element_t {
#define lqe_valen	256
	struct lq_prefix_t		lp;
	/* used to find an unique MME. */
	char					mme_ip[32],
							value[lqe_valen];
	size_t					valen,
							rawlen;

	uint32_t				ul_flags;
	vlib_mme_t				*mme;
	vlib_tm_grid_t			vtg;
};

#define LQE_VALUE(lqe) \
	((lqe)->value)
#define LQE_VALEN(lqe) \
	((lqe)->valen)
	
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
		lqe->ul_flags = 0;
		lqe->vtg.start = lqe->vtg.end = 0;
		memset((void *)&lqe->mme_ip[0], 0, 32);
		memset((void *)&lqe->value[0], 0, lqe_valen);
	}
	return lqe;
}


#endif

