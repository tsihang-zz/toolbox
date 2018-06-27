#ifndef __MEMCACHE_H__
#define __MEMCACHE_H__

struct oryx_kmcache_t {
	const char *name;
	os_mutex_t *lock;
	/** Object size. */
	size_t obj_size;
	size_t off;
	unsigned flags;
	int nr_objs;
	void *objs;
	void (*ctor)(void *);
};

extern struct oryx_kmcache_t *oryx_kc_create(const char *name, size_t size, size_t offset,
	unsigned long flags, void (*ctor)(void *));
extern void oryx_kc_free(struct oryx_kmcache_t *cachep, void *objp);
extern void *oryx_kc_alloc(struct oryx_kmcache_t *cachep);
extern unsigned int oryx_kc_size (struct oryx_kmcache_t *s);

#endif
