/*!
 * @file oryx_hashmap.h
 * @date 2017/08/29
 *
 * TSIHANG (haechime@gmail.com)
 */

#ifndef __ORYX_HASH_MAP_H__
#define __ORYX_HASH_MAP_H__

#include "hashmap.h"

ORYX_DECLARE (
	void oryx_hashmap_new (
		IN const char *name,
		IN uint32_t nr_max_elements,
		IN uint32_t nr_max_buckets,
		IN uint32_t hm_cfg,
		IN uint32_t (*hashkey_fn)(const hm_key_t),
		IN int (*cmpkey_fn)(const hm_key_t, const hm_key_t),
		IN void (*freekey_fn)(const hm_key_t, const hm_key_t),
		OUT void ** hashmap
	)
);

ORYX_DECLARE (
	void oryx_hashmap_destroy (
		IN void * hashmap
	)
);

ORYX_DECLARE (
	int oryx_hashmap_get (
		IN void * hashmap,
		IN hm_key_t key,
		IN hm_val_t *val
	)
);

ORYX_DECLARE (
	int oryx_hashmap_put (
		IN void * hashmap,
		IN hm_key_t key,
		IN hm_val_t val,
		IO hm_val_t *evicted
	)
);

ORYX_DECLARE (
	int oryx_hashmap_del (
		IN void * hashmap,
		IN hm_key_t key
	)
);

ORYX_DECLARE (
	int oryx_hashmap_resize (
		IN void * hashmap,
		uint32_t nr_max_buckets
	)
);

ORYX_DECLARE (
	void oryx_hashmap_print (
		IN void * hashmap
	)
);

ORYX_DECLARE (
	const char* oryx_hashmap_strerror (
		IN int err
	)
);



#endif

