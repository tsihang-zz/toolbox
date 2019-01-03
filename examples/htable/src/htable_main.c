#include "oryx.h"

struct oryx_htable_t *htable;
int active_elements = 0;
struct hash_key_t {
	char name[32];
};

static void
ht__free (const ht_value_t __oryx_unused__ v)
{
	/** Never free here! */
}

static ht_key_t
ht__hval (struct oryx_htable_t *ht,
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

static int
ht__cmp (const ht_value_t v1, 
		uint32_t s1,
		const ht_value_t v2,
		uint32_t s2)
{
	int err = 0;

	if (!v1 || !v2 ||s1 != s2 ||
		memcmp(v1, v2, s2))
		err = 1;

	return err;
}

static void htable_handler(ht_value_t __oryx_unused__ v,
				uint32_t __oryx_unused__ s,
				void *opaque,
				int __oryx_unused__ opaque_size) {
	int *actives = (int *)opaque;
	(*actives) ++;
}

int main (
	int 	__oryx_unused__	argc,
	char	__oryx_unused__	** argv
)
{
	int i;

	oryx_initialize();

	htable	= oryx_htable_init(DEFAULT_HASH_CHAIN_SIZE, 
				ht__hval, ht__cmp, ht__free, 0);

	for (i = 0; i < 10000; i ++) {
		struct hash_key_t *key = malloc(sizeof (struct hash_key_t));
		BUG_ON(key == NULL);
		oryx_pattern_generate(&key->name[0], 32);
		oryx_htable_add(htable, (ht_value_t)key, sizeof(struct hash_key_t));
	}
	
	int refcount = oryx_htable_foreach_elem(htable,
				htable_handler, &active_elements, sizeof(active_elements));
	
	oryx_htable_print(htable);
	fprintf (stdout, "%d =? %d\n", refcount, active_elements);
	
	oryx_htable_destroy(htable);
	fprintf(stdout, "Press Ctrl+c for quit ...\n");

	FOREVER;
}

