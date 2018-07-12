#include "oryx.h"
#include "geo_cdr_table.h"
#include "geo_capture.h"
#include "geo_htable.h"
#include "geo_cdr_mpool.h"
#include "geo_cdr_queue.h"

void ht_geo_cdr_free (const ht_value_t __oryx_unused_param__ v)
{
	/** Never free here! */
}

ht_key_t ht_geo_cdr_hval (struct oryx_htable_t *ht,
		const ht_value_t v, uint32_t s) 
{
	struct geo_htable_key_t hk = GEO_CDR_HASH_KEY_INIT_VAL;
	uint8_t *d = (uint8_t *)&hk;
	uint32_t i;
	uint32_t hv = 0;

	hk.mme_code = ((struct geo_htable_key_t *)v)->mme_code;
	hk.v		= ((struct geo_htable_key_t *)v)->v;
	
     for (i = 0; i < s; i++) {
         if (i == 0)      hv += (((uint32_t)*d++));
         else if (i == 1) hv += (((uint32_t)*d++) * s);
         else             hv *= (((uint32_t)*d++) * i) + s + i;
     }

     hv *= s;
     hv %= ht->array_size;
     
     return hv;
}

int ht_geo_cdr_cmp (const ht_value_t v1, 
		uint32_t s1,
		const ht_value_t v2,
		uint32_t s2)
{
	int xret = 0;
	
	if (!v1 || !v2 || s1 != s2 ||
		(((struct geo_htable_key_t *)v1)->mme_code != ((struct geo_htable_key_t *)v2)->mme_code) ||
		(((struct geo_htable_key_t *)v1)->v != ((struct geo_htable_key_t *)v2)->v))
		xret = 1;	/** Compare failed. */

	return xret;
}

struct oryx_htable_t *geo_cdr_hash_table;


