#ifndef MME_H
#define MME_H

#define MAX_MME_NUM	1024

extern uint32_t	epoch_time_sec;

#define VLIB_MME_VALID	(1 << 0)
typedef struct vlib_mme_t {
	char		name[32];
	vlib_file_t	file;		/* Current file hold 0 ~ 5 minutes */
	vlib_file_t filer;		/* Raw CSV file */
	vlib_file_t filep;		/* Previous file hold -5 ~ 0 minutes */
	vlib_file_t filen;		/* Next file hold 5 ~ 10 minutes */
	uint64_t	nr_refcnt;
	uint64_t	nr_refcnt_bytes;
	uint64_t	nr_refcnt_r;
	uint64_t	nr_refcnt_bytes_r;
	uint64_t	nr_refcnt_noimsi;
	uint64_t	nr_err_refcnt_overtime;
	uint32_t	ul_flags;
	struct oryx_htable_t *file_hash_tab;
	os_mutex_t	lock;
#define path_length	128
	char		path[path_length];
	char		pathr[path_length];
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

static __oryx_always_inline__
void  mme_try_fopen(const char *path, const char *name,
		vlib_tm_grid_t *vtg, vlib_file_t *f)
{
	if (f->fp == NULL) {
		file_open(path, name, vtg->start, vtg->end, f);
		if (f->ul_flags & VLIB_FILE_NEW) {
			fprintf (stdout, "Write CSV head to %s\n", f->fp_name);
			/* Write CSV header before writting an entry
			 * when vlib_file_t is new. */
			file_write(f, MME_CSV_HEADER, strlen(MME_CSV_HEADER), 1);
			f->ul_flags &= ~VLIB_FILE_NEW;
		}
	} else {
		/* check current file is valid for use. */
		if (f->local_time != vtg->start) {
			/* close previous file before open a new one. */
			file_close(f);
			file_open(path, name, vtg->start, vtg->end, f);
			if (f->ul_flags & VLIB_FILE_NEW) {
				fprintf (stdout, "Write CSV head to %s\n", f->fp_name);
				/* Write CSV header before writting an entry
				 * when vlib_file_t is new. */
				file_write(f, MME_CSV_HEADER, strlen(MME_CSV_HEADER), 1);
				f->ul_flags &= ~VLIB_FILE_NEW;
			}
		}
		BUG_ON(f->fp == NULL);
	}
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

#endif

