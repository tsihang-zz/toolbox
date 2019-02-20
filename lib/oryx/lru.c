/*!
 * @file lru.c
 * @date 2017/08/29
 *
 * TSIHANG (haechime@gmail.com)
 */

#include "oryx.h"


/**
 *  \brief Create a new LRU cache
 *
 *  \param lc_name, the cache alias
 *  \param lc_max_elements, supported maximum elements
 *  \param lc_cfg, configurations
 *  \param lc, the cache
 */
__oryx_always_extern__
void oryx_lc_new
(
	IN const char *lc_name,
	IN size_t lc_max_elements,
	IN uint32_t lc_cfg,
	OUT void ** lc
)
{
    struct oryx_lc_t *lctx = (struct oryx_lc_t *)malloc(sizeof(struct oryx_lc_t));
    if (lctx == NULL)
		oryx_panic(0,
			"malloc: %s", oryx_safe_strerror(errno));

    memset(lctx, 0, sizeof(struct oryx_lc_t));
	lctx->name				=	lc_name;
	lctx->nr_max_elements	=	lc_max_elements;
	lctx->ul_flags			=	lc_cfg;
	lctx->nr_elements		=	0;

	oryx_sys_mutex_create(&lctx->mtx);

    (*lc) = lctx;
}

