
#include "oryx.h"

static __oryx_always_inline__
oryx_status_t func_lock (os_lock_t *lock)
{
	lock = lock;
	return 0;
}

static __oryx_always_inline__
oryx_status_t func_unlock (os_lock_t *lock)
{
	lock = lock;
	return 0;
}


static __oryx_always_inline__
void func_free (ht_value_t v)
{
#ifdef ORYX_HASH_DEBUG
	printf ("free %s, %p\n", (char *)v, v);
#endif
	kfree (v);
}

static ht_key_t
func_hash (struct oryx_htable_t *ht,
		void *v, u32 s) 
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
func_cmp (ht_value_t v1, 
		u32 s1,
		ht_value_t v2,
		u32 s2)
{
	int xret = 0;

	if (!v1 || !v2 || s1 != s2 ||
		memcmp(v1, v2, s2))
		xret = 1;
	
	return xret;
}

void oryx_htable_print(struct oryx_htable_t *ht)
{
	printf("\n----------- Hash Table Summary ------------\n");
	printf("Buckets:				%" PRIu32 "\n", ht->array_size);
	printf("Active:					%" PRIu32 "\n", ht->count);
	printf("Hashfn:					%pF\n", ht->hash_fn);
	printf("Freefn:					%pF\n", ht->free_fn);
	printf("Compfn:					%pF\n", ht->cmp_fn);
	printf("Feature:				\n");
	printf("					%s\n", ht->ul_flags & HTABLE_SYNCHRONIZED ? "Synchronized" : "Non-Synchronized");
	
	printf("-----------------------------------------\n");
}

struct oryx_htable_t* oryx_htable_init (uint32_t max_buckets, 
	ht_key_t (*hash_fn)(struct oryx_htable_t *, void *, u32), 
	int (*cmp_fn)(void *, u32, void *, u32), 
	void (*free_fn)(void *), u32 flags) 
{

    struct oryx_htable_t *ht = NULL;

    if (max_buckets == 0) {
        goto error;
    }
	
    /* setup the filter */
    ht = kmalloc(sizeof(struct oryx_htable_t), MPF_CLR, -1);
    if (unlikely(ht == NULL))
    	goto error;

	/** kmalloc.MPF_CLR means that the memory block pointed by its return 
	 *  has been CLEARED. */
	/** memset(ht,0,sizeof(struct oryx_htable_t)); */

	ht->ht_lock_fn = func_lock;
	ht->ht_unlock_fn = func_unlock;
	ht->hash_fn = hash_fn ? hash_fn : func_hash;
	ht->cmp_fn = cmp_fn ? cmp_fn : func_cmp;
	ht->free_fn = free_fn ? free_fn : func_free;
	ht->ul_flags = flags;

	if (ht->ul_flags & HTABLE_SYNCHRONIZED) {
		oryx_thread_mutex_create (&ht->os_lock);
		/** Overwrite. */
		ht->ht_lock_fn = oryx_thread_mutex_lock;
		ht->ht_unlock_fn = oryx_thread_mutex_unlock;
	}
	
	ht->array_size = max_buckets;
    /** setup the bitarray */
    ht->array = kmalloc(ht->array_size * sizeof(struct oryx_hbucket_t *), MPF_CLR, -1);
    if (ht->array == NULL)
        goto error;

	/** kmalloc.MPF_CLR means that the memory block pointed by its return 
	 *  has been CLEARED. */
    /** memset(ht->array,0,ht->array_size * sizeof(struct oryx_hbucket_t *)); */
	if(ht->ul_flags & HTABLE_PRINT_INFO)
		oryx_htable_print (ht);

    return ht;

error:
    if (ht != NULL) {
        if (ht->array != NULL)
            kfree(ht->array);

        kfree(ht);
    }
    return NULL;
}

void oryx_htable_destroy(struct oryx_htable_t *ht)
{
    int i = 0;

    if (ht == NULL)
        return;

	ht->ht_lock_fn(ht->os_lock);
	
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

	ht->ht_unlock_fn(ht->os_lock);

	oryx_thread_mutex_destroy(ht->os_lock);
	
    kfree(ht);
}

int oryx_htable_add(struct oryx_htable_t *ht, ht_value_t data, u32 datalen)
{
    if (ht == NULL || data == NULL)
        return -1;

    ht_key_t hash = ht->hash_fn(ht, data, datalen);

    struct oryx_hbucket_t *hb = kmalloc(sizeof(struct oryx_hbucket_t), MPF_CLR, -1);
    if (unlikely(hb == NULL))
        goto error;
	
	/** kmalloc.MPF_CLR means that the memory block pointed by its return 
	 *  has been CLEARED. */
	/** memset(hb, 0, sizeof(struct oryx_hbucket_t)); */
    hb->data = data;
    hb->ul_d_size = datalen;
    hb->next = NULL;
	
	ht->ht_lock_fn(ht->os_lock);
    if (ht->array[hash] == NULL) {
        ht->array[hash] = hb;
    } else {
        hb->next = ht->array[hash];
        ht->array[hash] = hb;
    }
    ht->count++;
	ht->ht_unlock_fn(ht->os_lock);
#ifdef ORYX_HASH_DEBUG
	printf ("add %s, %p\n", (char *)hb->data, hb->data);
#endif

    return 0;
error:
    return -1;
}

int oryx_htable_del(struct oryx_htable_t *ht, ht_value_t data, u32 datalen)
{
    ht_key_t hash = ht->hash_fn(ht, data, datalen);

	ht->ht_lock_fn(ht->os_lock);

    if (ht->array[hash] == NULL) {
		ht->ht_unlock_fn(ht->os_lock);
		return -1;
    }
	
    if (ht->array[hash]->next == NULL) {
		if (ht->free_fn != NULL)
            ht->free_fn(ht->array[hash]->data);
        kfree(ht->array[hash]);
        ht->array[hash] = NULL;
		ht->count --;
		ht->ht_unlock_fn(ht->os_lock);
        return 0;
    }

    struct oryx_hbucket_t *hb = ht->array[hash], *prev_hb = NULL;
    do {
        if (ht->cmp_fn(hb->data, hb->ul_d_size, data, datalen) == 0) {
            if (prev_hb == NULL) {
                /* root bucket */
                ht->array[hash] = hb->next;
            } else {
                /* child bucket */
                prev_hb->next = hb->next;
            }

            /* remove this */
            if (ht->free_fn != NULL)
                ht->free_fn(hb->data);
            kfree(hb);
	     	ht->count --;
			ht->ht_unlock_fn(ht->os_lock);
            return 0;
        }

        prev_hb = hb;
        hb = hb->next;
    } while (hb != NULL);

	ht->ht_unlock_fn(ht->os_lock);

    return -1;
}

void *oryx_htable_lookup(struct oryx_htable_t *ht, ht_value_t data, u32 datalen)
{
    ht_key_t hash = 0;

    if (ht == NULL)
        return NULL;

    hash = ht->hash_fn(ht, data, datalen);

	ht->ht_lock_fn(ht->os_lock);

    if (ht->array[hash] == NULL) {
		ht->ht_unlock_fn(ht->os_lock);
        return NULL;
    }

    struct oryx_hbucket_t *hb = ht->array[hash];
    do {
		/** printf ("%s. %s\n", (char *)data, (char *)hashbucket->data); */
        if (ht->cmp_fn(hb->data, hb->ul_d_size, data, datalen) == 0) {
			ht->ht_unlock_fn(ht->os_lock);
            return hb->data;
        }

        hb = hb->next;
    } while (hb != NULL);

	ht->ht_unlock_fn(ht->os_lock);
	
    return NULL;
}


