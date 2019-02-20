/*!
 * @file oryx_hashtable.h
 * @date 2017/08/29
 *
 * TSIHANG (haechime@gmail.com)
 */

#ifndef __ORYX_HASH_TABLE_H__
#define __ORYX_HASH_TABLE_H__

#include "hashtab.h"

ORYX_DECLARE (
	struct oryx_hashtab_t* oryx_hashtab_new (
		IN const char *name,
		IN uint32_t nr_max_elements,
		IN uint32_t nr_max_buckets,
		IN uint32_t (*hashkey_fn)(IN ht_key_t), 
		IN int (*cmpkey_fn)(IN ht_key_t, IN ht_key_t), 
		IN void (*freekey_fn)(IN ht_key_t),
		IN uint32_t ht_cfg
	)
);
	
ORYX_DECLARE (
	void oryx_hashtab_destroy (
		IN struct oryx_hashtab_t *ht
	)
);

ORYX_DECLARE (
	void oryx_hashtab_print (
		IN struct oryx_hashtab_t *ht
	)
);

ORYX_DECLARE (
	int oryx_hashtab_add (
		IO struct oryx_hashtab_t *ht,
		IN ht_key_t value,
		IN uint32_t valen
	)
);

ORYX_DECLARE (
	int oryx_hashtab_del (
		IO struct oryx_hashtab_t *ht,
		IN ht_key_t value
	)
);

ORYX_DECLARE (
	int oryx_hashtab_foreach (
		IN struct oryx_hashtab_t *ht,
		IN void (*handler)(IN ht_key_t, IN uint32_t, IN void *, IN int),
		IN void *opaque,
		IN int opaque_size
	)
);

static __oryx_always_inline__
void * oryx_htable_lookup 
(
	IN struct oryx_hashtab_t *ht,
	IN ht_key_t value
)
{
	BUG_ON ((ht == NULL) || (value == NULL));

    uint32_t hash = (ht->hashkey_fn(value) % ht->nr_max_buckets);

	HTABLE_LOCK(ht);

    if (ht->array[hash] == NULL) {
		HTABLE_UNLOCK(ht);
        return NULL;
    }

    struct oryx_hbucket_t *hb = ht->array[hash];
    do {
		/** fprintf (stdout, "%s. %s\n", (char *)value, (char *)hashbucket->value); */
        if (ht->cmpkey_fn(hb->value, value) == 0) {
			HTABLE_UNLOCK(ht);
            return hb->value;
        }

        hb = hb->next;
    } while (hb != NULL);

	HTABLE_UNLOCK(ht);
	
    return NULL;
}


#endif

