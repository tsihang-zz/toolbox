/*!
 * @file htable.c
 * @date 2017/08/29
 *
 * TSIHANG (haechime@gmail.com)
 */

#include "oryx.h"
	
static void
func_free
(
	IN const ht_value_t v
)
{
#ifdef ORYX_HASH_DEBUG
	fprintf (stdout, "free %s, %p\n", (char *)v, v);
#endif
	kfree (v);
}

static ht_key_t
func_hash
(
	IN struct oryx_htable_t *ht,
	IN const ht_value_t v,
	IN uint32_t s
) 
{
     uint8_t *d = (uint8_t *)v;
     uint32_t i;
     ht_key_t hv = 0;

     for (i = 0; i < s; i++) {
         if (i == 0)      hv += (((uint32_t)*d++));
         else if (i == 1) hv += (((uint32_t)*d++) * s);
         else             hv *= (((uint32_t)*d++) * i) + s + i;
     }

     hv *= s;
     hv %= ht->array_size;
     
     return hv;
}

static int
func_cmp
(
	IN const ht_value_t v1,
	IN uint32_t s1,
	IN const ht_value_t v2,
	IN uint32_t s2
)
{
	int err = 0;

	if (!v1 || !v2 || s1 != s2 ||
		memcmp(v1, v2, s2))
		err = 1;
	
	return err;
}

__oryx_always_extern__
void oryx_htable_print
(
	IN struct oryx_htable_t *ht
)
{
	BUG_ON(ht == NULL);

	fprintf (stdout, "\n----------- Hash Table Summary ------------\n");
	fprintf (stdout, "Buckets:				%" PRIu32 "\n", ht->array_size);
	fprintf (stdout, "Active:					%" PRIu32 "\n", ht->active_count);
	fprintf (stdout, "Hashfn:					%pF\n", ht->hash_fn);
	fprintf (stdout, "Freefn:					%pF\n", ht->free_fn);
	fprintf (stdout, "Compfn:					%pF\n", ht->cmp_fn);
	fprintf (stdout, "Feature:				\n");
	fprintf (stdout, "					%s\n", ht->ul_flags & HTABLE_SYNCHRONIZED ? "Synchronized" : "Non-Synchronized");
	
	fprintf (stdout, "-----------------------------------------\n");
}

__oryx_always_extern__
struct oryx_htable_t* oryx_htable_init
(
	IN uint32_t max_buckets, 
	IN ht_key_t (*hash_fn)(IN struct oryx_htable_t *, IN void *, IN uint32_t), 
	IN int (*cmp_fn)(IN void *, IN uint32_t, IN void *, IN uint32_t), 
	IN void (*free_fn)(IN void *),
	IN uint32_t ht_cfg
) 
{
	struct oryx_htable_t *ht = NULL;

	/* setup the filter */
	ht = kmalloc(sizeof(struct oryx_htable_t),
			MPF_CLR, __oryx_unused_val__);
	BUG_ON((ht == NULL) || (max_buckets == 0));

	/* kmalloc.MPF_CLR means that the memory block pointed by its return 
	 * has been CLEARED.
	 */
	ht->hash_fn			= hash_fn ? hash_fn : func_hash;
	ht->cmp_fn			= cmp_fn ? cmp_fn : func_cmp;
	ht->free_fn			= free_fn ? free_fn : func_free;
	ht->ul_flags		= ht_cfg;
	ht->array_size		= max_buckets;

	if (ht->ul_flags & HTABLE_SYNCHRONIZED) {
		oryx_sys_mutex_create (&ht->mtx);
	}

	/** setup the bitarray */
	ht->array = kmalloc(ht->array_size * sizeof(struct oryx_hbucket_t *),
					MPF_CLR, __oryx_unused_val__);
	if (unlikely(!ht->array))
		oryx_panic(-1,
			"kmalloc: %s", oryx_safe_strerror(errno));

	/* kmalloc.MPF_CLR means that the memory block pointed by its return 
	 * has been CLEARED. 
	 */
	if (ht->ul_flags & HTABLE_PRINT_INFO)
		oryx_htable_print (ht);

	return ht;
}

__oryx_always_extern__
void oryx_htable_destroy
(
	IN struct oryx_htable_t *ht
)
{
    int i = 0;

	BUG_ON(ht == NULL);

	HTABLE_LOCK(ht);
	
    /* free the buckets */
    for (i = 0; i < ht->array_size; i++) {
        struct oryx_hbucket_t *hb = ht->array[i];
        while (hb != NULL) {
            struct oryx_hbucket_t *next_hb = hb->next;
            if (ht->free_fn != NULL)
                ht->free_fn(hb->value);
            kfree(hb);
            hb = next_hb;
        }
    }

    /* free bucket arrray */
	if (ht->array != NULL)
        kfree(ht->array);

	HTABLE_UNLOCK(ht);

	if (ht->ul_flags & HTABLE_SYNCHRONIZED)
		oryx_sys_mutex_destroy(&ht->mtx);
	
    kfree(ht);
}

__oryx_always_extern__
int oryx_htable_add
(
	IO struct oryx_htable_t *ht,
	IN ht_value_t value,
	IN uint32_t valen
)
{
	BUG_ON ((ht == NULL) || (value == NULL));
	ht_key_t hash = ht->hash_fn(ht, value, valen);

	struct oryx_hbucket_t *hb = kmalloc(sizeof(struct oryx_hbucket_t), MPF_CLR, -1);
	if (unlikely(!hb))
		oryx_panic(-1,
			"kmalloc: %s", oryx_safe_strerror(errno));
	
	/** kmalloc.MPF_CLR means that the memory block pointed by its return 
	 *  has been CLEARED. */
	hb->value	= value;
	hb->valen	= valen;
	hb->next	= NULL;
    
	HTABLE_LOCK(ht);
	if (ht->array[hash] == NULL) {
		ht->array[hash] = hb;
	} else {
		hb->next = ht->array[hash];
		ht->array[hash] = hb;
	}
	ht->active_count++;
	HTABLE_UNLOCK(ht);

#ifdef ORYX_HASH_DEBUG
	fprintf (stdout, "add %s, %p\n", (char *)hb->value, hb->value);
#endif
	return 0;
}

__oryx_always_extern__
int oryx_htable_del
(
	IO struct oryx_htable_t *ht,
	IN ht_value_t value,
	IN uint32_t valen
)
{
	BUG_ON ((ht == NULL) || (value == NULL));
	ht_key_t hash = ht->hash_fn(ht, value, valen);

	HTABLE_LOCK(ht);
	if (ht->array[hash] == NULL) {
		HTABLE_UNLOCK(ht);
		return -1;
	}

	if (ht->array[hash]->next == NULL) {
		if (ht->free_fn != NULL)
			ht->free_fn(ht->array[hash]->value);
		kfree(ht->array[hash]);
		ht->array[hash] = NULL;
		ht->active_count --;
		HTABLE_UNLOCK(ht);
		return 0;
	}

	struct oryx_hbucket_t *hb = ht->array[hash], *pb = NULL;
	do {
		if (ht->cmp_fn(hb->value, hb->valen, value, valen) == 0) {
			if (pb == NULL) {
				/* root bucket */
				ht->array[hash] = hb->next;
			} else {
				/* child bucket */
				pb->next = hb->next;
			}
			/* remove this */
			if (ht->free_fn != NULL)
				ht->free_fn(hb->value);
			kfree(hb);
			ht->active_count --;
			HTABLE_UNLOCK(ht);
			return 0;
		}

		pb = hb;
		hb = hb->next;

	} while (hb != NULL);
	HTABLE_UNLOCK(ht);

	return -1;
}

__oryx_always_extern__
int oryx_htable_foreach_elem
(
	IN struct oryx_htable_t *ht,
	IN void (*handler)(IN ht_value_t, IN uint32_t, IN void *, IN int),
	IN void *opaque,
	IN int opaque_size
)
{
	BUG_ON(ht == NULL);

	int refcount = 0;
	int i;
	struct oryx_hbucket_t *hb;

	/* skip if no handler for saving performence. */
	if(handler == NULL)
		return 0;
	
	HTABLE_LOCK(ht);
	
	for (i = 0; i < htable_active_slots(ht); i ++) {
		hb = ht->array[i];
		if (hb == NULL) continue;
		do {
			handler(hb->value, hb->valen, opaque, opaque_size);
			refcount ++;
			hb = hb->next;
		} while (hb != NULL);
	}
	
	HTABLE_UNLOCK(ht);
	return refcount;
}

