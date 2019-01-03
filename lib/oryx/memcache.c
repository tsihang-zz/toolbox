#include "oryx.h"

#define MC_LOCK(mc)\
		oryx_sys_mutex_lock(&(mc)->mtx);

#define MC_UNLOCK(mc)\
		oryx_sys_mutex_unlock(&(mc)->mtx);
	

/*
 * Determine the size of a slab object
 */
unsigned int oryx_kc_size (struct oryx_kmcache_t *s)
{
	return s->obj_size;
}

void *oryx_kc_alloc(struct oryx_kmcache_t *cachep)
{
	void *node = NULL;

	MC_LOCK (cachep);
	MC_UNLOCK (cachep);

	node = kmalloc(cachep->obj_size, MPF_CLR, __oryx_unused_val__);
	if (cachep->ctor)
		cachep->ctor(node);

	return node;
}

void oryx_kc_free(struct oryx_kmcache_t *cachep, void *objp)
{
	assert(objp);

	MC_LOCK (cachep);
	/**
	 * FIXME UP.
	 */
	 kfree (objp);
	MC_UNLOCK(cachep);
}


struct oryx_kmcache_t *
oryx_kc_create(const char *name, size_t size, size_t offset,
	unsigned long flags, void (*ctor)(void *))
{
	struct oryx_kmcache_t *ret = kmalloc(sizeof(*ret), MPF_CLR, __oryx_unused_val__);

	oryx_sys_mutex_create (&ret->mtx);
	ret->obj_size = size;
	ret->name = name;
	ret->nr_objs = 0;
	ret->objs = NULL;
	ret->ctor = ctor;
	ret->off = offset;
	ret->flags = flags;
	
	return ret;
}


