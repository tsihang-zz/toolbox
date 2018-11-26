#include "oryx.h"
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

	do_mutex_lock (cachep->lock);
	do_mutex_unlock (cachep->lock);

	node = kmalloc(cachep->obj_size, MPF_CLR, __oryx_unused_val__);
	if (cachep->ctor)
		cachep->ctor(node);

	return node;
}

void oryx_kc_free(struct oryx_kmcache_t *cachep, void *objp)
{
	assert(objp);

	do_mutex_lock (cachep->lock);
	/**
	 * FIXME UP.
	 */
	 kfree (objp);
	do_mutex_unlock(cachep->lock);
}


struct oryx_kmcache_t *
oryx_kc_create(const char *name, size_t size, size_t offset,
	unsigned long flags, void (*ctor)(void *))
{
	struct oryx_kmcache_t *ret = kmalloc(sizeof(*ret), MPF_CLR, __oryx_unused_val__);

	oryx_tm_create (&ret->lock);
	ret->obj_size = size;
	ret->name = name;
	ret->nr_objs = 0;
	ret->objs = NULL;
	ret->ctor = ctor;
	ret->off = offset;
	ret->flags = flags;
	
	return ret;
}


