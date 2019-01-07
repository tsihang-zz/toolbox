/*!
 * @file memcache.h
 * @date 2017/08/29
 *
 * TSIHANG (haechime@gmail.com)
 */

#ifndef __MEMCACHE_H__
#define __MEMCACHE_H__

/* Allow the use in C++ code.  */
#ifdef __cplusplus
extern "C" {
#endif

struct oryx_kmcache_t {
	const char *name;
	sys_mutex_t mtx;
	/** Object size. */
	size_t obj_size;
	size_t off;
	unsigned flags;
	int nr_objs;
	void *objs;
	void (*ctor)(void *);
};

ORYX_DECLARE(
	struct oryx_kmcache_t *oryx_kc_create (const char *name, size_t size, size_t offset,
							unsigned long flags, void (*ctor)(void *))
);

ORYX_DECLARE(
	void oryx_kc_free(struct oryx_kmcache_t *cachep, void *objp)
);

ORYX_DECLARE(
	void *oryx_kc_alloc(struct oryx_kmcache_t *cachep)
);

ORYX_DECLARE(
	unsigned int oryx_kc_size (struct oryx_kmcache_t *s)
);

#ifdef __cplusplus
}
#endif /* C++ */

#endif
