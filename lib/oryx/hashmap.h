/*!
 * @file hashmap.h
 * @date 2017/08/29
 *
 * TSIHANG (haechime@gmail.com)
 */

#ifndef __HASH_MAP_H__
#define __HASH_MAP_H__

/* Allow the use in C++ code.  */
#ifdef __cplusplus
extern "C" {
#endif

#define HAVE_HASHMAP_DEBUG

typedef void*	hm_key_t;
typedef void*	hm_val_t;

struct oryx_hashmap_bucket_t {
	hm_val_t		value;	/**< value */
	hm_key_t		key;	/**< keyof(value) */
	struct			oryx_hashmap_bucket_t *next;
};

#define	HASHMAP_DEFAULT_CHAINS	8
#define HASHMAP_MAX_LOAD_FACTOR .75	
#define HASHMAP_ENABLE_UPDATE	(1 << 0)
struct oryx_hashmap_t {
	const char	RDONLY *name;
	struct oryx_hashmap_bucket_t	**array;
	uint32_t		RDONLY nr_max_elements;	/**< maximum allowed elements in @array */
	uint32_t		RDONLY nr_max_buckets;
	uint32_t		nr_elements;		/**< current elements count in @array */
	uint32_t		ul_flags;
	sys_mutex_t		mtx;
	enum {HM_SUCCESS, HM_FAILURE, HM_HIT, HM_MISS, HM_FULL} err; /**< inner errno */

	void (*freekey_fn)(IN const hm_key_t, IN const hm_val_t);
	uint32_t (*hashkey_fn)(IN const hm_key_t);	/* function for create a hash value
							 * with the given parameter *v */
	int (*cmpkey_fn)(IN const hm_key_t, IN const hm_key_t);	/* 0: equal,
							 * otherwise a value less than zero returned. */
#if defined(HAVE_HASHMAP_DEBUG)
	uint64_t memory;		/**< for debug only, in bytes */
#endif
};

#define oryx_hashmap_usage(hashmap) \
	ratio_of(((struct oryx_hashmap_t *)(hashmap))->nr_elements, ((struct oryx_hashmap_t *)(hashmap))->nr_max_elements)
#define oryx_hashmap_load(hashmap) \
	((double)((struct oryx_hashmap_t *)(hashmap))->nr_elements / ((struct oryx_hashmap_t *)(hashmap))->nr_max_buckets)
#define oryx_hashmap_curr_buckets(hashmap) (((struct oryx_hashmap_t *)(hashmap))->nr_max_buckets)

#ifdef __cplusplus
}
#endif /* C++ */

#endif

