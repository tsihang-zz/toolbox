/*!
 * @file lru.h
 * @date 2017/08/29
 *
 * TSIHANG (haechime@gmail.com)
 */

#ifndef __LRU_H__
#define __LRU_H__

/* Allow the use in C++ code.  */
#ifdef __cplusplus
extern "C" {
#endif

typedef void*	lc_key_t;
typedef void*	lc_val_t;

struct lc_element_t {
	ATOMIC_DECLARE(int, refcnt);	/**< reference count */
	lc_key_t	key;			/**< key identifier */
	lc_val_t	value;			/**< value */
	size_t	valen;				/**< length of @value */
};

struct oryx_lc_t {
	const char	RDONLY *name;
	uint32_t	ul_flags;
	sys_mutex_t	mtx;
	int		RDONLY nr_max_elements;
	int		nr_elements;
	struct oryx_hashtab_t*	ht;
};



/**
 *  \brief add an instance to a LRU cache
 *
 *  \param lc LRU cache
 *  \param lc_key, key of instance
 *  \param lc_val, value of instance
 */
static __oryx_always_inline__
int oryx_lc_put 
(
	IN void *lc,
	IN lc_key_t __oryx_unused__ key,
	IN lc_val_t __oryx_unused__ val
)
{
	int err;
	struct oryx_lc_t *lctx = (struct oryx_lc_t *)lc;

	oryx_sys_mutex_lock(&lctx->mtx);
	
	oryx_sys_mutex_unlock(&lctx->mtx);

	return err;
}

/**
 *  \brief get an instance to a LRU cache
 *
 *  \param lc LRU cache
 *  \param lc_key, key of instance
 *  \param lc_val, value of instance
 */
static __oryx_always_inline__
int oryx_lc_get 
(
	IN void *lc,
	IN lc_key_t __oryx_unused__ key,
	IN lc_val_t __oryx_unused__ val
)
{
	int err;
	struct oryx_lc_t *lctx = (struct oryx_lc_t *)lc;

	oryx_sys_mutex_lock(&lctx->mtx);
	
	oryx_sys_mutex_unlock(&lctx->mtx);

	return err;
}

#ifdef __cplusplus
}
#endif /* C++ */

#endif


