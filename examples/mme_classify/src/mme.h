#ifndef MME_H
#define MME_H

#define MAX_MME_NUM	1024

extern uint32_t	epoch_time_sec;


#define VLIB_MME_VALID	(1 << 0)
typedef struct vlib_mme_t {
	char		name[32];
	vlib_file_t	file;		/* Current file hold 0 ~ 5 minutes */
	vlib_file_t filer;		/* Raw CSV file */
	struct list_head fhead;	/* Opened file handlers for this mme. */


	char		*ip_str[32];
	int			nr_ip;

	/* total entries for this MME,
	 * may equal with. */
	uint64_t	nr_rx_entries;
	uint64_t	nr_rx_entries_noimsi;

	/* statistics of RIGHT write for each MME with IMSI */
	uint64_t	nr_refcnt;
	uint64_t	nr_refcnt_bytes;
	/* statistics of miss writing, cause -> fopen error*/
	uint64_t	nr_refcnt_miss;
	uint64_t	nr_refcnt_miss_bytes;
	/* statistics of error writing. */
	uint64_t	nr_refcnt_error;
	uint64_t	nr_refcnt_error_bytes;	

	/* statistics of RAW RIGHT writing for each MME with or without IMSI */
	uint64_t	nr_refcnt_r;
	uint64_t	nr_refcnt_bytes_r;
	uint64_t	nr_refcnt_miss_r;
	uint64_t	nr_refcnt_miss_bytes_r;
	uint64_t	nr_refcnt_error_r;
	uint64_t	nr_refcnt_error_bytes_r;
	
	uint32_t	ul_flags;
	os_mutex_t	lock;
#define path_length	128
	char		path[path_length];
	char		pathr[path_length];

	uint32_t	lq_id;
} vlib_mme_t;

#define MME_LOCK(mme)\
	(do_mutex_lock(&(mme)->lock))
#define MME_UNLOCK(mme)\
	(do_mutex_unlock(&(mme)->lock))
#define MME_LOCK_INIT(mme)\
	do_mutex_init(&(mme)->lock)

#define MME_FILE_VALID(mme)\
	((mme)->file.fp != NULL)
	
extern vlib_mme_t nr_global_mmes[];

typedef struct vlib_mmekey_t {
	/* IP address of this MME. */
	char		ip[32];
	vlib_mme_t	*mme;
	uint64_t	nr_refcnt;	/* refcnt for this key (IP) */
} vlib_mmekey_t;

#define VLIB_MME_HASH_KEY_INIT_VAL {\
		.name	= NULL,\
		.ip		= NULL,\
	}

#define MME_CSV_HEADER \
	",,Event Start,Event Stop,Event Type,IMSI,IMEI,,,,,,,,eCell ID,,,,,,,,,,,,,,,,,,,,,,,,,,MME UE S1AP ID,eNodeB UE S1AP ID,eNodeB CP IP Address\n"


static __oryx_always_inline__
void fmt_mme_ip(char *out, const char *in)
{
	uint32_t a,b,c,d;

	sscanf(in, "%d.%d.%d.%d", &a, &b, &c, &d);
	sprintf (out, "%d.%d.%d.%d", a, b, c, d);
}

extern void mmekey_free (const ht_value_t __oryx_unused_param__ v);
extern ht_key_t mmekey_hval (struct oryx_htable_t *ht,
					const ht_value_t v, uint32_t s) ;
extern int mmekey_cmp (const ht_value_t v1, uint32_t s1,
					const ht_value_t v2, uint32_t s2);
extern vlib_mmekey_t *mmekey_alloc(void);
extern vlib_mme_t *mme_find(const char *name, size_t nlen);
extern vlib_mme_t *mme_alloc(const char *name, size_t nlen);
extern void mme_print(ht_value_t  v,
				uint32_t __oryx_unused_param__ s,
				void __oryx_unused_param__*opaque,
				int __oryx_unused_param__ opaque_size);

extern vlib_mme_t *default_mme;

/* find MME within a hash table */
static __oryx_always_inline__
vlib_mme_t *mme_find_ip_h(struct oryx_htable_t *ht, const char *ip, size_t iplen)
{
	void *s;
	vlib_mmekey_t *mmekey;
	vlib_mme_t		*mme = NULL;

	s = oryx_htable_lookup(ht, (const void *)ip, iplen);
	if (s) {
		mmekey = (vlib_mmekey_t *) container_of (s, vlib_mmekey_t, ip);
		mme = mmekey->mme;
	}
	
	return mme == NULL ? default_mme : mme;
}


#endif

