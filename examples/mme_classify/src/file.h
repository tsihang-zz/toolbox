#ifndef FILE_H
#define FILE_H

extern uint64_t nr_files;;
extern uint64_t nr_handlers;

typedef struct vlib_fkey_t {
	uint64_t val;
}vlib_fkey_t;

#define VLIB_FILE_ONLINE	(1 << 0)
#define VLIB_FILE_NEW		(1 << 1)
typedef struct vlib_file_t {
#define	name_length	256
	FILE *fp;
	char fp_name[name_length];
	uint64_t entries;
	time_t		local_time;
	uint32_t	ul_flags;
}vlib_file_t;

static __oryx_always_inline__
int file_empty0(vlib_file_t *f)
{
	char ch;
	size_t nr_rb = 0;

	if (f->fp != NULL)
		nr_rb = fread(&ch, sizeof(char), 1, f->fp);

	return nr_rb == 0 ? 1 : 0;
}


static __oryx_always_inline__
int file_empty(vlib_file_t *f)
{
	FILE *fp;
	char ch;
	size_t nr_rb = 0;
	
	fp = fopen(f->fp_name, "r");
	if (fp != NULL) {
		nr_rb = fread(&ch, sizeof(char), 1, fp);
		fclose(fp);
	}
	
	return nr_rb == 0 ? 1 : 0;
}

static __oryx_always_inline__
size_t file_write(vlib_file_t *f, const char *val, size_t valen, bool flush)
{
	size_t nr_wb;
	
	nr_wb = fwrite(val, sizeof(char), valen, f->fp);
#if 0	
	fprintf(fp, "%s", val);
	if (flush)	fflush(fp);
#endif

	return nr_wb;
}

static __oryx_always_inline__
void file_close(vlib_file_t *f)
{
	if(f->fp == NULL)
		return;

	f->ul_flags &= ~VLIB_FILE_ONLINE;
	
	/* flush file before close it. */
	fflush(f->fp);
	fclose(f->fp);
	/* reset csv file handler. */
	f->fp = NULL;
	f->ul_flags = 0;
	f->local_time = 0;
	nr_handlers --;
	//fprintf (stdout, "fclose %s (%lu handlers)\n", f->fp_name, nr_handlers);
}

/* event_start_time <= TIME < event_end_time */
static __oryx_always_inline__
int file_open(const char *path,
	const char *mme_name, time_t start, time_t end, vlib_file_t *f)
{
	memset (f->fp_name, 0, name_length);
	
	sprintf (f->fp_name, "%s/DataExport.s1mme%s_%lu_%lu.csv",
			path, mme_name, start, end);

	/* open an exist file. */
	f->fp = fopen (f->fp_name, "a+");
	if (f->fp == NULL) {
		fprintf (stdout, "fopen(%s, %lu handlers): %s\n",
			f->fp_name, nr_handlers, oryx_safe_strerror(errno));
		exit(0);
	} else {
		nr_handlers ++;
		/* update local time. */
		f->local_time = start;
		f->entries = 0;
		f->ul_flags |= VLIB_FILE_ONLINE;
		if (file_empty0(f))
			f->ul_flags |= VLIB_FILE_NEW;
	}
	
	return f->fp ? 1 : 0;
}


/* event_start_time <= TIME < event_end_time */
static __oryx_always_inline__
int file_new(const char *path,
	const char *mme_name, time_t start, time_t end, vlib_file_t *f)
{
	memset (f->fp_name, 0, name_length);
	
	sprintf (f->fp_name, "%s/DataExport.s1mme%s_%lu_%lu.csv",
			path, mme_name, start, end);

	/* Create current file. */
	f->fp = fopen (f->fp_name, "a+");
	if (f->fp == NULL) {
		fprintf (stdout, "fopen(%lu handlers): %s\n",
			nr_handlers, oryx_safe_strerror(errno));
		exit(0);
	} else {
		nr_handlers ++;
		/* update local time. */
		f->entries = 0;
	}
	
	file_close(f);

	return 0;
}

static __oryx_always_inline__
int file_overtime(vlib_file_t *f, time_t now, int threshold)
{
	return (now > (f->local_time +  threshold * 60));
}

static __oryx_always_inline__
int file_overdisk(vlib_file_t *f, int max_entries)
{
	return ((int64_t)f->entries > (int64_t)max_entries);
}

extern void fkey_free (const ht_value_t __oryx_unused_param__ v);
extern ht_key_t fkey_hval (struct oryx_htable_t *ht,
		const ht_value_t v, uint32_t s);
extern int fkey_cmp (const ht_value_t v1, uint32_t s1,
		const ht_value_t v2, uint32_t s2);

extern vlib_fkey_t *fkey_alloc(void);

#endif

