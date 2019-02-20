#include "oryx.h"
#include "config.h"

vlib_mme_t nr_global_mmes[VLIB_MAX_MME_NUM];
/* Store those unknown entries. */
vlib_mme_t *default_mme = NULL;

extern unsigned long clac_crc32
(
	IN const unsigned char *s,
	IN unsigned int len
);

/*
 * Free function for a string
 */
void mmekey_free
(
	IN const ht_key_t __oryx_unused__ v
)
{
	/** Never free here! */
}

/*
 * Hashing function for a string
 */
uint32_t mmekey_hval
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
int mmekey_cmp
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

vlib_mmekey_t *mmekey_alloc(void)
{
	vlib_mmekey_t *v = malloc(sizeof (vlib_mmekey_t));
	BUG_ON(v == NULL);
	memset (v, 0, sizeof (vlib_mmekey_t));
	return v;
}

/* find and alloc MME */
vlib_mme_t *mme_find(const char *name, size_t nlen)
{
	int i;
	vlib_mme_t *mme;
	size_t s1, s2;
	
	BUG_ON(name == NULL || nlen != strlen(name));
	s1 = nlen;
	for (i = 0, s2 = 0; i < VLIB_MAX_MME_NUM; i ++) {
		mme = &nr_global_mmes[i];
		s2 = strlen(mme->name);
		if (s1 == s2 &&
			!strcmp(name, mme->name)) {
			goto finish;
		}
		
		mme = NULL;
	}

finish:
	return mme;
}

vlib_mme_t *mme_alloc(const char *name, size_t nlen)
{
	int i;
	vlib_mme_t *mme;

	for (i = 0; i < VLIB_MAX_MME_NUM; i ++) {
		mme = &nr_global_mmes[i];
		if (mme->ul_flags & VLIB_MME_VALID)
			continue;
		else {
			int j;
			for (j = 0; j < 288; j ++)
				file_reset(&mme->farray[j]);
			file_reset(&mme->file);
			mme->ul_flags |= VLIB_MME_VALID;
			mme->lq_id = 0;
			mme->tv = 0;
			INIT_LIST_HEAD(&mme->fhead);
			oryx_sys_mutex_create(&mme->lock);
			memcpy(&mme->name[0], &name[0], nlen);
			return mme;
		}
	}
	
	return NULL;
}

void mme_print(ht_key_t  v,
		uint32_t __oryx_unused__ s,
		void __oryx_unused__*opaque,
		int __oryx_unused__ opaque_size) {
	vlib_mme_t *mme;
	vlib_mmekey_t *mmekey = (vlib_mmekey_t *)container_of (v, vlib_mmekey_t, ip);
	FILE *fp = (FILE *)opaque;

	if (mmekey) {
		mme = mmekey->mme;
		fprintf (fp, "%16s%16s\n", "MME_NAME: ", mme->name);
		fprintf (fp, "%16s%16lu\n", "REFCNT: ",   mme->nr_refcnt);
	}

	fflush(fp);
}


