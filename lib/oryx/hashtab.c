/*!
 * @file hashtable.c
 * @date 2017/08/29
 *
 * TSIHANG (haechime@gmail.com)
 */

#include "oryx.h"


extern unsigned long clac_crc32
(
	IN const unsigned char *s,
	IN unsigned int len
);

/*
 * Free function for a string
 */
static void
hashtab_freekey
(
	IN const ht_key_t v
)
{
#ifdef ORYX_HASH_DEBUG
	fprintf (stdout, "free %s, %p\n", (char *)v, v);
#endif
	kfree (v);
}

/*
 * Hashing function for a string
 */
static uint32_t
hashtab_hashkey
(
	IN const ht_key_t key
) 
{
	unsigned long k = clac_crc32((unsigned char*)(key), strlen(key));

	/* Robert Jenkins' 32 bit Mix Function */
	k += (k << 12);
	k ^= (k >> 22);
	k += (k << 4);
	k ^= (k >> 9);
	k += (k << 10);
	k ^= (k >> 2);
	k += (k << 7);
	k ^= (k >> 12);

	/* Knuth's Multiplicative Method */
	k = (k >> 3) * 2654435761;

	return k;
}

/*
 * Compare function for a string
 */
static __oryx_always_inline__
int hashtab_cmpkey
(
	IN const hm_key_t k0,
	IN const hm_key_t k1
)
{
	int err = 0;

	if (!k0 || !k1 ||
		strcmp(k0, k1))
		err = 1;

	return err;
}


__oryx_always_extern__
void oryx_hashtab_print
(
	IN struct oryx_hashtab_t *ht
)
{
	BUG_ON(ht == NULL);

	fprintf (stdout, "\n----------- Hash Table Summary ------------\n");
	fprintf (stdout, "Buckets:				%" PRIu32 "\n", ht->nr_max_buckets);
	fprintf (stdout, "Active:					%" PRIu32 "\n", ht->nr_elements);
	fprintf (stdout, "Hashfn:					%pF\n", ht->hashkey_fn);
	fprintf (stdout, "Freefn:					%pF\n", ht->freekey_fn);
	fprintf (stdout, "Compfn:					%pF\n", ht->cmpkey_fn);
	fprintf (stdout, "Feature:				\n");
	fprintf (stdout, "					%s\n", ht->ul_flags & HTABLE_SYNCHRONIZED ? "Synchronized" : "Non-Synchronized");
	
	fprintf (stdout, "-----------------------------------------\n");
}

__oryx_always_extern__
struct oryx_hashtab_t* oryx_hashtab_new
(
	IN const char *name,
	IN uint32_t nr_max_elements,
	IN uint32_t nr_max_buckets,
	IN uint32_t (*hashkey_fn)(IN ht_key_t), 
	IN int (*cmpkey_fn)(IN ht_key_t, IN ht_key_t), 
	IN void (*freekey_fn)(IN ht_key_t),
	IN uint32_t ht_cfg
)
{
	struct oryx_hashtab_t *ht = NULL;

	/* setup the filter */
	ht = kmalloc(sizeof(struct oryx_hashtab_t),
			MPF_CLR, __oryx_unused_val__);
	BUG_ON((ht == NULL) || (nr_max_buckets == 0));

	/* kmalloc.MPF_CLR means that the memory block pointed by its return 
	 * has been CLEARED. */
	ht->name			=	name;
	ht->hashkey_fn		=	hashkey_fn ? hashkey_fn : hashtab_hashkey;
	ht->cmpkey_fn		=	cmpkey_fn  ? cmpkey_fn  : hashtab_cmpkey;
	ht->freekey_fn		=	freekey_fn ? freekey_fn : hashtab_freekey;
	ht->ul_flags		=	ht_cfg;
	ht->nr_max_buckets	=	nr_max_buckets;
	ht->nr_max_elements =	nr_max_elements;

	if (ht->ul_flags & HTABLE_SYNCHRONIZED) {
		oryx_sys_mutex_create (&ht->mtx);
	}

	/** setup the bitarray */
	ht->array = kmalloc(ht->nr_max_buckets * sizeof(struct oryx_hbucket_t *),
					MPF_CLR, __oryx_unused_val__);
	if (unlikely(!ht->array))
		oryx_panic(-1,
			"kmalloc: %s", oryx_safe_strerror(errno));

	/* kmalloc.MPF_CLR means that the memory block pointed by its return 
	 * has been CLEARED. */
	if (ht->ul_flags & HTABLE_PRINT_INFO)
		oryx_hashtab_print (ht);

	return ht;
}

__oryx_always_extern__
void oryx_hashtab_destroy
(
	IN struct oryx_hashtab_t *ht
)
{
    int i = 0;

	BUG_ON(ht == NULL);

	HTABLE_LOCK(ht);
	
    /* free the buckets */
    for (i = 0; i < (int)ht->nr_max_buckets; i++) {
        struct oryx_hbucket_t *hb = ht->array[i];
        while (hb != NULL) {
            struct oryx_hbucket_t *next_hb = hb->next;
            if (ht->freekey_fn != NULL)
                ht->freekey_fn(hb->value);
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
int oryx_hashtab_add
(
	IO struct oryx_hashtab_t *ht,
	IN ht_key_t value,
	IN uint32_t valen
)
{
	BUG_ON ((ht == NULL) || (value == NULL));
	uint32_t hash = (ht->hashkey_fn(value) % ht->nr_max_buckets);

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
	ht->nr_elements++;
	HTABLE_UNLOCK(ht);

#ifdef ORYX_HASH_DEBUG
	fprintf (stdout, "add %s, %p\n", (char *)hb->value, hb->value);
#endif
	return 0;
}

__oryx_always_extern__
int oryx_hashtab_del
(
	IO struct oryx_hashtab_t *ht,
	IN ht_key_t value
)
{
	BUG_ON ((ht == NULL) || (value == NULL));
	uint32_t hash = (ht->hashkey_fn(value) % ht->nr_max_buckets);

	HTABLE_LOCK(ht);
	if (ht->array[hash] == NULL) {
		HTABLE_UNLOCK(ht);
		return -1;
	}

	if (ht->array[hash]->next == NULL) {
		if (ht->freekey_fn != NULL)
			ht->freekey_fn(ht->array[hash]->value);
		kfree(ht->array[hash]);
		ht->array[hash] = NULL;
		ht->nr_elements --;
		HTABLE_UNLOCK(ht);
		return 0;
	}

	struct oryx_hbucket_t *hb = ht->array[hash], *pb = NULL;
	do {
		if (ht->cmpkey_fn(hb->value, value) == 0) {
			if (pb == NULL) {
				/* root bucket */
				ht->array[hash] = hb->next;
			} else {
				/* child bucket */
				pb->next = hb->next;
			}
			/* remove this */
			if (ht->freekey_fn != NULL)
				ht->freekey_fn(hb->value);
			kfree(hb);
			ht->nr_elements --;
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
int oryx_hashtab_foreach
(
	IN struct oryx_hashtab_t *ht,
	IN void (*handler)(IN ht_key_t, IN uint32_t, IN void *, IN int),
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
	
	for (i = 0; i < (int)htable_active_slots(ht); i ++) {
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

