#ifndef MME_HTABLE_H
#define MME_HTABLE_H

typedef struct vlib_mme_t {
	/* name of this MME. */
	char *name;
	/* IP address of this MME. */
	char *ip;
}vlib_mme_t;

#define VLIB_MME_HASH_KEY_INIT_VAL {\
		.name	= NULL,\
		.ip		= NULL,\
	}

extern void ht_mme_free (const ht_value_t __oryx_unused_param__ v);
extern ht_key_t ht_mme_hval (struct oryx_htable_t *ht,
		const ht_value_t v, uint32_t s) ;
extern int ht_mme_cmp (const ht_value_t v1, 
		uint32_t s1,
		const ht_value_t v2,
		uint32_t s2);

#endif

