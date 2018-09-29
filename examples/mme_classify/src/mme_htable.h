#ifndef MME_H
#define MME_H

#define MAX_MME_NUM	1024

#define MME_CSV_HOME	"./test"

typedef struct vlib_mme_t {
	char name[32];
	uint64_t	nr_refcnt;
	FILE	*fp;
	os_mutex_t lock;
} vlib_mme_t;

extern vlib_mme_t	nr_global_mmes[];

typedef struct vlib_mme_key_t {
	/* name of this MME. */
	//char name[32];
	/* IP address of this MME. */
	char ip[32];
	vlib_mme_t *mme;
	uint64_t	nr_refcnt;	/* refcnt for this key (IP) */
} vlib_mme_key_t;

#define VLIB_MME_HASH_KEY_INIT_VAL {\
		.name	= NULL,\
		.ip		= NULL,\
	}

extern void ht_mme_key_free (const ht_value_t __oryx_unused_param__ v);
extern ht_key_t ht_mme_key_hval (struct oryx_htable_t *ht,
		const ht_value_t v, uint32_t s) ;
extern int ht_mme_key_cmp (const ht_value_t v1, 
		uint32_t s1,
		const ht_value_t v2,
		uint32_t s2);

#endif

