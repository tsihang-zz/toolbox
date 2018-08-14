
#include "oryx.h"

#define HTABLE_LOCK(ht)\
	if((ht)->ul_flags & HTABLE_SYNCHRONIZED)\
		do_mutex_lock((ht)->os_lock);

#define HTABLE_UNLOCK(ht)\
	if((ht)->ul_flags & HTABLE_SYNCHRONIZED)\
		do_mutex_unlock((ht)->os_lock);
	
static __oryx_always_inline__
void func_free (const ht_value_t v)
{
#ifdef ORYX_HASH_DEBUG
	fprintf (stdout, "free %s, %p\n", (char *)v, v);
#endif
	kfree (v);
}

static ht_key_t
func_hash (struct oryx_htable_t *ht,
		const ht_value_t v, uint32_t s) 
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
func_cmp (const ht_value_t v1, 
		uint32_t s1,
		const ht_value_t v2,
		uint32_t s2)
{
	int xret = 0;

	if (!v1 || !v2 || s1 != s2 ||
		memcmp(v1, v2, s2))
		xret = 1;
	
	return xret;
}

void oryx_htable_print(struct oryx_htable_t *ht)
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

struct oryx_htable_t* oryx_htable_init (uint32_t max_buckets, 
	ht_key_t (*hash_fn)(struct oryx_htable_t *, void *, uint32_t), 
	int (*cmp_fn)(void *, uint32_t, void *, uint32_t), 
	void (*free_fn)(void *), uint32_t ht_cfg) 
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
		oryx_thread_mutex_create (&ht->os_lock);
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

void oryx_htable_destroy(struct oryx_htable_t *ht)
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
                ht->free_fn(hb->data);
            kfree(hb);
            hb = next_hb;
        }
    }

    /* free bucket arrray */
	if (ht->array != NULL)
        kfree(ht->array);

	HTABLE_UNLOCK(ht);

	oryx_thread_mutex_destroy(ht->os_lock);
	
    kfree(ht);
}

int oryx_htable_add(struct oryx_htable_t *ht, ht_value_t data, uint32_t datalen)
{
	BUG_ON ((ht == NULL) || (data == NULL));

    ht_key_t hash = ht->hash_fn(ht, data, datalen);

    struct oryx_hbucket_t *hb = kmalloc(sizeof(struct oryx_hbucket_t), MPF_CLR, -1);
    if (unlikely(!hb))
		oryx_panic(-1,
			"kmalloc: %s", oryx_safe_strerror(errno));
	
	/** kmalloc.MPF_CLR means that the memory block pointed by its return 
	 *  has been CLEARED. */
    hb->data		= data;
    hb->next		= NULL;
	hb->ul_d_size	= datalen;
    
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
	fprintf (stdout, "add %s, %p\n", (char *)hb->data, hb->data);
#endif

    return 0;
}

int oryx_htable_del(struct oryx_htable_t *ht, ht_value_t data, uint32_t datalen)
{
	BUG_ON ((ht == NULL) || (data == NULL));
	
    ht_key_t hash = ht->hash_fn(ht, data, datalen);

	HTABLE_LOCK(ht);

    if (ht->array[hash] == NULL) {
		HTABLE_UNLOCK(ht);
		return -1;
    }
	
    if (ht->array[hash]->next == NULL) {
		if (ht->free_fn != NULL)
            ht->free_fn(ht->array[hash]->data);
        kfree(ht->array[hash]);
        ht->array[hash] = NULL;
		ht->active_count --;
		HTABLE_UNLOCK(ht);
        return 0;
    }

    struct oryx_hbucket_t *hb = ht->array[hash], *pb = NULL;
    do {
        if (ht->cmp_fn(hb->data, hb->ul_d_size, data, datalen) == 0) {
            if (pb == NULL) {
                /* root bucket */
                ht->array[hash] = hb->next;
            } else {
                /* child bucket */
                pb->next = hb->next;
            }

            /* remove this */
            if (ht->free_fn != NULL)
                ht->free_fn(hb->data);
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

void *oryx_htable_lookup(struct oryx_htable_t *ht, const ht_value_t data, uint32_t datalen)
{
	BUG_ON ((ht == NULL) || (data == NULL));

    ht_key_t hash = ht->hash_fn(ht, data, datalen);

	HTABLE_LOCK(ht);

    if (ht->array[hash] == NULL) {
		HTABLE_UNLOCK(ht);
        return NULL;
    }

    struct oryx_hbucket_t *hb = ht->array[hash];
    do {
		/** fprintf (stdout, "%s. %s\n", (char *)data, (char *)hashbucket->data); */
        if (ht->cmp_fn(hb->data, hb->ul_d_size, data, datalen) == 0) {
			HTABLE_UNLOCK(ht);
            return hb->data;
        }

        hb = hb->next;
    } while (hb != NULL);

	HTABLE_UNLOCK(ht);
	
    return NULL;
}

int oryx_htable_foreach_elem(struct oryx_htable_t *ht,
	void (*handler)(ht_value_t, uint32_t, void *, int), void *opaque, int opaque_size)
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
			handler(hb->data, hb->ul_d_size, opaque, opaque_size);
			refcount ++;
			hb = hb->next;
		} while (hb != NULL);
	}
	
	HTABLE_UNLOCK(ht);
	return refcount;
}

