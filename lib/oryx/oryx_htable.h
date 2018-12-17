#ifndef ORYX_HASH_TABLE_H
#define ORYX_HASH_TABLE_H

#include "htable.h"

ORYX_DECLARE (
	struct oryx_htable_t* oryx_htable_init (
		IN uint32_t max_buckets, 
		IN ht_key_t (*hash_fn)(IN struct oryx_htable_t *, IN void *, IN uint32_t), 
		IN int (*cmp_fn)(IN void *, IN uint32_t, IN void *, IN uint32_t), 
		IN void (*free_fn)(IN void *),
		IN uint32_t ht_cfg
	)
);
	
ORYX_DECLARE (
	void oryx_htable_destroy (
		IN struct oryx_htable_t *ht
	)
);

ORYX_DECLARE (
	void oryx_htable_print (
		IN struct oryx_htable_t *ht
	)
);

ORYX_DECLARE (
	int oryx_htable_add (
		IN struct oryx_htable_t *ht,
		IN ht_value_t value,
		IN uint32_t valen
	)
);

ORYX_DECLARE (
	int oryx_htable_del (
		IN struct oryx_htable_t *ht,
		IN ht_value_t value,
		IN uint32_t valen
	)
);

ORYX_DECLARE (
	int oryx_htable_foreach_elem (
		IN struct oryx_htable_t *ht,
		IN void (*handler)(IN ht_value_t, IN uint32_t, IN void *, IN int),
		IN void *opaque,
		IN int opaque_size
	)
);

static __oryx_always_inline__
void * oryx_htable_lookup 
(
	IN struct oryx_htable_t *ht,
	IN ht_value_t value,
	IN uint32_t valen
)
{
	BUG_ON ((ht == NULL) || (value == NULL));

    ht_key_t hash = ht->hash_fn(ht, value, valen);

	HTABLE_LOCK(ht);

    if (ht->array[hash] == NULL) {
		HTABLE_UNLOCK(ht);
        return NULL;
    }

    struct oryx_hbucket_t *hb = ht->array[hash];
    do {
		/** fprintf (stdout, "%s. %s\n", (char *)value, (char *)hashbucket->value); */
        if (ht->cmp_fn(hb->value, hb->valen, value, valen) == 0) {
			HTABLE_UNLOCK(ht);
            return hb->value;
        }

        hb = hb->next;
    } while (hb != NULL);

	HTABLE_UNLOCK(ht);
	
    return NULL;
}


#endif

