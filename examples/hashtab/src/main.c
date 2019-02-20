#include "oryx.h"

struct oryx_hashtab_t *htable;
int active_elements = 0;
struct hash_key_t {
	char name[32];
};

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
	IN const ht_key_t __oryx_unused__ v
)
{
	/** Never free here! */
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

static void htable_handler(ht_key_t __oryx_unused__ v,
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

	htable	= oryx_hashtab_new("HASHTABLE",
					1024000,
					DEFAULT_HASH_CHAIN_SIZE,
					hashtab_hashkey,
					hashtab_cmpkey,
					hashtab_freekey,
					0);

	for (i = 0; i < 10000; i ++) {
		struct hash_key_t *key = malloc(sizeof (struct hash_key_t));
		BUG_ON(key == NULL);
		oryx_pattern_generate(&key->name[0], 32);
		oryx_hashtab_add(htable, (ht_key_t)key, sizeof(struct hash_key_t));
	}
	
	int refcount = oryx_hashtab_foreach(htable,
				htable_handler, &active_elements, sizeof(active_elements));
	
	oryx_hashtab_print(htable);
	fprintf (stdout, "%d =? %d\n", refcount, active_elements);
	
	oryx_hashtab_destroy(htable);
	fprintf(stdout, "Press Ctrl+c for quit ...\n");

	FOREVER;
}

