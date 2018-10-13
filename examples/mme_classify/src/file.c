#include "oryx.h"
#include "file.h"

uint64_t nr_files = 0;
uint64_t nr_handlers = 0;

void fkey_free (const ht_value_t __oryx_unused_param__ v)
{
	/** Never free here! */
}

ht_key_t fkey_hval (struct oryx_htable_t *ht,
		const ht_value_t v, uint32_t __oryx_unused_param__ s)
{
	uint64_t d = *(uint64_t *)v;
	uint32_t hv = 0;

     hv = d % ht->array_size;
     
     return hv;
}

int fkey_cmp (const ht_value_t v1, uint32_t s1,
		const ht_value_t v2, uint32_t s2)
{
	int xret = 0;
	
	if (!v1 || !v2 || s1 != s2 ||
		*(uint64_t *)v1 != *(uint64_t *)v2)
		xret = 1;	/** Compare failed. */

	return xret;
}

vlib_fkey_t *fkey_alloc(void)
{
	vlib_fkey_t *v = malloc(sizeof (vlib_fkey_t));
	BUG_ON(v == NULL);
	memset (v, 0, sizeof (vlib_fkey_t));
	return v;
}


