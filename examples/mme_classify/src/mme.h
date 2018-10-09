#ifndef MME_H
#define MME_H

#define MAX_MME_NUM	1024

#define MME_CSV_THRESHOLD	1 /* miniutes */

enum {
	OVERTIME,
	OVERDISK,
};

extern uint32_t	epoch_time_sec;

#define VLIB_FILE_NEW	(1 << 0)
typedef struct vlib_file_t {
	FILE *fp;
	char fp_name[128];
	uint64_t entries;
	time_t		local_time;
	uint32_t	ul_flags;
}vlib_file_t;

#define VLIB_MME_VALID	(1 << 0)
typedef struct vlib_mme_t {
	char		name[32];
	vlib_file_t	file;
	uint64_t	nr_refcnt;
	uint64_t	nr_miss;
	uint64_t	nr_refcnt_bytes;
	//FILE		*fp;
	//char		fp_name[128];
	//time_t	local_time;
	uint32_t	ul_flags;	
	os_mutex_t	lock;
} vlib_mme_t;

#define MME_LOCK(mme)\
	(do_mutex_lock(&(mme)->lock))
#define MME_UNLOCK(mme)\
	(do_mutex_unlock(&(mme)->lock))
#define MME_LOCK_INIT(mme)\
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

static __oryx_always_inline__
void write_one_line(FILE *fp, const char *val, bool flush)
{
	fprintf(fp, "%s", val);
	if (flush)	fflush(fp);
}

static __oryx_always_inline__
void csv_close(vlib_mme_t *mme, int reason)
{
	vlib_file_t *f = &mme->file;

	if(f->fp == NULL)
		return;
	
	fprintf (stdout, "Closing file \"%s\" reason %s\n",
			f->fp_name, reason == OVERTIME ? "overtime" : "overdisk");
	/* flush file before close it. */
	fflush(f->fp);
	/* overtime or overdisk, close this file. */
	fclose(f->fp);
	/* reset csv file handler. */
	f->fp = NULL;
}

static __oryx_always_inline__
int cvs_new(vlib_mme_t *mme, time_t start, int threshold, const char *warehouse)
{
	vlib_file_t *f = &mme->file;
	
	memset (f->fp_name, 0, 128);
	sprintf (f->fp_name, "%s/%s/DataExport.s1mme%s_%lu_%lu.csv",
		warehouse, mme->name, mme->name, start, start + (threshold * 60));

	f->fp = fopen (f->fp_name, "a+");
	if (f->fp == NULL) {
		fprintf (stdout, "Cannot fopen file %s\n", f->fp_name);
	} else {
		fprintf (stdout, "Create new file %s\n", f->fp_name);
		/* update local time. */
		f->entries = 0;
		f->local_time = start;
		f->ul_flags |= VLIB_FILE_NEW;
	}
	
	return f->fp ? 1 : 0;
}

static __oryx_always_inline__
int cvs_empty(vlib_mme_t *mme)
{
	vlib_file_t *f = &mme->file;
	FILE *fp;
	int ch = EOF;
	
	fp = fopen(f->fp_name, "r");
	if (fp != NULL) {
		ch = fgetc(fp);	
		fclose(fp);
	}
	
	return ch == EOF;
}

static __oryx_always_inline__
int is_overtime(vlib_mme_t *mme, time_t now, int threshold)
{
	vlib_file_t *f = &mme->file;
	return (now > (f->local_time +  threshold * 60));
}

static __oryx_always_inline__
int is_overdisk(vlib_mme_t *mme, int max_entries)
{
	vlib_file_t *f = &mme->file;
	return ((int64_t)f->entries > (int64_t)max_entries);
}

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
extern vlib_mme_key_t *mmekey_alloc(void);
extern vlib_mme_t *mme_find(const char *name, size_t nlen);
extern vlib_mme_t *mme_alloc(const char *name, size_t nlen);
extern void mme_print(ht_value_t  v,
				uint32_t __oryx_unused_param__ s,
				void __oryx_unused_param__*opaque,
				int __oryx_unused_param__ opaque_size);

#endif

