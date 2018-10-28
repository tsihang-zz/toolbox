#include "oryx.h"
#include "config.h"

vlib_mme_t nr_global_mmes[VLIB_MAX_MME_NUM];
/* Store those unknown entries. */
vlib_mme_t *default_mme = NULL;

void mmekey_free (const ht_value_t __oryx_unused_param__ v)
{
	/** Never free here! */
}

ht_key_t mmekey_hval (struct oryx_htable_t *ht,
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

int mmekey_cmp (const ht_value_t v1, 
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
			MME_LOCK_INIT(mme);
			memcpy(&mme->name[0], &name[0], nlen);
			return mme;
		}
	}
	
	return NULL;
}

void mme_print(ht_value_t  v,
		uint32_t __oryx_unused_param__ s,
		void __oryx_unused_param__*opaque,
		int __oryx_unused_param__ opaque_size) {
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


