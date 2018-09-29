#include "oryx.h"
#include "mme_htable.h"

vlib_mme_t nr_global_mmes[MAX_MME_NUM];

void ht_mme_key_free (const ht_value_t __oryx_unused_param__ v)
{
	/** Never free here! */
}

ht_key_t ht_mme_key_hval (struct oryx_htable_t *ht,
		const ht_value_t v, uint32_t s) 
{
	uint8_t *d = (uint8_t *)v;
	uint32_t i;
	uint32_t hv = 0;

     for (i = 0; i < s; i++) {
         if (i == 0)      hv += (((uint32_t)*d++));
         else if (i == 1) hv += (((uint32_t)*d++) * s);
         else             hv *= (((uint32_t)*d++) * i) + s + i;
     }

     hv *= s;
     hv %= ht->array_size;
     
     return hv;
}

int ht_mme_key_cmp (const ht_value_t v1, 
		uint32_t s1,
		const ht_value_t v2,
		uint32_t s2)
{
	int xret = 0;
	
	if (!v1 || !v2 || s1 != s2 ||
		memcmp(v1, v2, s2))
		xret = 1;	/** Compare failed. */

	return xret;
}

