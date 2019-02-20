#ifndef MME_H
#define MME_H

#define VLIB_MME_VALID	(1 << 0)

typedef struct vlib_mme_t {
#define path_length	128
	char				path[path_length],
						pathr[path_length],
						name[32],
						*ip_str[32];
	int					nr_ip;
	vlib_file_t			file,			/* Current file hold 0 ~ 5 minutes */
						farray[288];	/* (24 * 60) / vm->threshold */
	uint32_t			ul_flags,
						lq_id;

	vlib_threadvar_t	*tv;

	sys_mutex_t			lock;


	struct list_head 	fhead;	/* Opened file handlers for this mme. */

	/* total entries for this MME,
	 * may equal with. */
	uint64_t	nr_rx_entries,
				nr_rx_entries_noimsi,
				nr_refcnt,				/* statistics of RIGHT write for each MME with IMSI */
				nr_refcnt_bytes,
				nr_refcnt_miss,			/* statistics of miss writing, cause -> fopen error*/
				nr_refcnt_miss_bytes,
				nr_refcnt_error,		/* statistics of error writing. */
				nr_refcnt_error_bytes,
				nr_refcnt_r,			/* statistics of RAW RIGHT writing for each MME with or without IMSI */
				nr_refcnt_bytes_r,
				nr_refcnt_miss_r,
				nr_refcnt_miss_bytes_r,
				nr_refcnt_error_r,
				nr_refcnt_error_bytes_r;

} vlib_mme_t;

#define MME_LOCK(mme)\
	(oryx_sys_mutex_lock(&(mme)->lock))
#define MME_UNLOCK(mme)\
	(oryx_sys_mutex_unlock(&(mme)->lock))
#define MME_LOCK_INIT(mme)\
	do_mutex_init(&(mme)->lock)

#define MME_FILE_VALID(mme)\
	((mme)->file.fp != NULL)
	
extern vlib_mme_t nr_global_mmes[];

typedef struct vlib_mmekey_t {
	char		ip[32];		/* IP address of this MME. */
	vlib_mme_t	*mme;
	uint64_t	nr_refcnt;	/* refcnt for this key (IP) */
} vlib_mmekey_t;

#define VLIB_MME_HASH_KEY_INIT_VAL {\
		.name	= NULL,\
		.ip		= NULL,\
	}

static __oryx_always_inline__
void fmt_mme_ip(char *out, const char *in)
{
	uint32_t a,b,c,d;

	sscanf(in, "%d.%d.%d.%d", &a, &b, &c, &d);
	sprintf (out, "%d.%d.%d.%d", a, b, c, d);
}

extern void mmekey_free (
	IN const ht_key_t __oryx_unused__ v
);
extern uint32_t mmekey_hval (
	IN const ht_key_t key
) ;
extern int mmekey_cmp (
	IN const ht_key_t v1,
	IN const ht_key_t v2
);

extern vlib_mmekey_t *mmekey_alloc(void);
extern vlib_mme_t *mme_find(const char *name, size_t nlen);
extern vlib_mme_t *mme_alloc(const char *name, size_t nlen);
extern void mme_print(ht_key_t  v,
				uint32_t __oryx_unused__ s,
				void __oryx_unused__*opaque,
				int __oryx_unused__ opaque_size);

extern vlib_mme_t *default_mme;

/* find MME within a hash table */
static __oryx_always_inline__
vlib_mme_t *mme_find_ip_h(struct oryx_hashtab_t *ht, const char *ip)
{
	void *s;
	vlib_mmekey_t *mmekey;
	vlib_mme_t		*mme = NULL;

	s = oryx_htable_lookup(ht, (ht_key_t)ip);
	if (s) {
		mmekey = (vlib_mmekey_t *) container_of (s, vlib_mmekey_t, ip);
		mme = mmekey->mme;
	}
	
	return mme == NULL ? default_mme : mme;
}


#endif

