#ifndef MME_H
#define MME_H

#define MAX_MME_NUM	1024

#define MME_CSV_THRESHOLD	1 /* miniutes */

enum {
	OVERTIME,
	OVERDISK,
};

extern uint32_t	epoch_time_sec;

#define VLIB_MME_VALID	(1 << 0)
typedef struct vlib_mme_t {
	char		name[32];
	uint64_t	nr_refcnt;
	uint64_t	nr_miss;
	FILE		*fp;
	char		fp_name[128];
	uint32_t	ul_flags;
	time_t		local_time;
	os_mutex_t	lock;
} vlib_mme_t;

#define MME_LOCK(mme)\
	(do_mutex_lock(&(mme)->lock))
#define MME_UNLOCK(mme)\
	(do_mutex_unlock(&(mme)->lock))
#define MME_LOCK_INIT(q)\
	do_mutex_init(&(mme)->lock)

extern vlib_mme_t nr_global_mmes[];

typedef struct vlib_mme_key_t {
	/* IP address of this MME. */
	char		ip[32];
	vlib_mme_t	*mme;
	uint64_t	nr_refcnt;	/* refcnt for this key (IP) */
} vlib_mme_key_t;

#define VLIB_MME_HASH_KEY_INIT_VAL {\
		.name	= NULL,\
		.ip		= NULL,\
	}

#define MME_CSV_HEADER \
	",,Event Start,Event Stop,Event Type,IMSI,IMEI,,,,,,,,eCell ID,,,,,,,,,,,,,,,,,,,,,,,,,,MME UE S1AP ID,eNodeB UE S1AP ID,eNodeB CP IP Address,MME Code,M-TMSI,MME IP Address\n"

extern void ht_mme_key_free (const ht_value_t __oryx_unused_param__ v);
extern ht_key_t ht_mme_key_hval (struct oryx_htable_t *ht,
		const ht_value_t v, uint32_t s) ;
extern int ht_mme_key_cmp (const ht_value_t v1, 
		uint32_t s1,
		const ht_value_t v2,
		uint32_t s2);

extern vlib_mme_key_t *mmekey_alloc(void);
extern vlib_mme_t *mme_find(const char *name, size_t nlen);
extern vlib_mme_t *mme_alloc(const char *name, size_t nlen);
extern void mme_print(ht_value_t  v,
				uint32_t __oryx_unused_param__ s,
				void __oryx_unused_param__*opaque,
				int __oryx_unused_param__ opaque_size);

#endif

