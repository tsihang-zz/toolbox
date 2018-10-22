#ifndef CLASSIFY_H
#define CLASSIFY_H

#define HAVE_LOCAL_TEST
#define MAX_LQ_NUM	8
#define ENQUEUE_LCORE_ID 0
#define DEQUEUE_LCORE_ID 1

typedef struct vlib_threadvar_ctx_t {
	struct oryx_lq_ctx_t *lq;
	uint32_t			unique_id;
	uint64_t			nr_unclassified_refcnt;
}vlib_threadvar_ctx_t;

extern vlib_threadvar_ctx_t vlib_threadvar_main[];
extern const char *classify_home;

#define LQE_HAVE_IMSI	(1 << 0)
struct lq_element_t {
#define lqe_valen	256
	struct lq_prefix_t		lp;
	/* used to find an unique MME. */
	char					mme_ip[32];
	vlib_mme_t				*mme;
	uint32_t				ul_flags;
	char					value[lqe_valen];
	size_t					valen;
	size_t					rawlen;
	vlib_tm_grid_t			vtg;
};

#define LQE_VALUE(lqe) \
	((lqe)->value)
#define LQE_VALEN(lqe) \
	((lqe)->valen)
	
#define lq_element_size	(sizeof(struct lq_element_t))

static __oryx_always_inline__
struct lq_element_t *lqe_alloc(void)
{
	const size_t lqe_size = lq_element_size;
	struct lq_element_t *lqe;
	
	lqe = malloc(lqe_size);
	if(lqe) {
		lqe->lp.lnext = lqe->lp.lprev = NULL;
		lqe->valen = 0;
		lqe->ul_flags = 0;
		lqe->vtg.start = lqe->vtg.end = 0;
		memset((void *)&lqe->mme_ip[0], 0, 32);
		memset((void *)&lqe->value[0], 0, lqe_valen);
	}
	return lqe;
}

static __oryx_always_inline__
int write_lqe_data(vlib_file_t *f, const char *value, size_t valen)
{
	size_t nr_wb;

#if 0
	/* flush to disk every 5000 entries. */
	if (f->entries % 50000 == 0)
		f->ul_flags = VLIB_FILE_FLUSH;
#endif

	nr_wb = file_write(f, value, valen);
	return nr_wb == valen ? 0 : 1;
}

static __oryx_always_inline__
void  mme_try_fopen(vlib_mme_t *mme,
		vlib_tm_grid_t *vtg, vlib_file_t *f)
{
	vlib_main_t *vm = &vlib_main;
	char *path = classify_home;
		
	if (f->fp == NULL) {
		goto try_new_file;
	} else {
		/* check current file is valid for use. */
		if (f->local_time != vtg->start) {
			/* close previous file before open a new one. */
			file_close(f);
			goto try_new_file;
		}
	}
	
	BUG_ON(f->fp == NULL);
	return;
	
try_new_file:

	file_open(path, mme->name, vtg->start, vtg->end, f);
	if (f->ul_flags & VLIB_FILE_NEW) {
		fprintf (stdout, "Write CSV head to %s\n", f->abs_fname);
		/* Write CSV header before writting an entry
		 * when vlib_file_t is new. */
		file_write(f, MME_CSV_HEADER, strlen(MME_CSV_HEADER));
		f->ul_flags &= ~VLIB_FILE_NEW;
		nr_classified_files ++;
	}

}

static __oryx_always_inline__
void write_lqe(vlib_mme_t *mme, const struct lq_element_t *lqe)
{
	vlib_file_t *f, *fr;
	vlib_main_t *vm = &vlib_main;
	vlib_tm_grid_t *vtg = &lqe->vtg;

	/* lock MME */
	MME_LOCK(mme);

	f  = &mme->file;
	fr = &mme->filer;

	mme->nr_rx_entries ++;

	if (!(lqe->ul_flags & LQE_HAVE_IMSI))
		mme->nr_rx_entries_noimsi ++;

#if 0	
	/* for test */
	if (!fr->fp || fr->local_time != vtg.start)
		nr_fopen_times_r ++;
	
	mme_try_fopen(mme->pathr, mme->name, &vtg, fr);
	if (!fr->fp) {
		mme->nr_refcnt_miss_r ++;
		mme->nr_refcnt_miss_bytes_r += LQE_VALEN(lqe);
	} else {
		/* Anyway write raw. */
		if (!write_lqe_data (fr, (const char *)&lqe->value[0], LQE_VALEN(lqe))) {
			mme->nr_refcnt_r ++;
			mme->nr_refcnt_bytes_r += LQE_VALEN(lqe);
		} else {
			mme->nr_refcnt_error_r ++;
			mme->nr_refcnt_error_bytes_r += LQE_VALEN(lqe);
		}
	}
#else
	mme->nr_refcnt_r ++;
	mme->nr_refcnt_bytes_r += LQE_VALEN(lqe);
#endif


	/* Write those CDR with IMSI */ 
	if (lqe->ul_flags & LQE_HAVE_IMSI) {
#if 1
		/* Open files on RAMDISK */
		mme_try_fopen(mme, vtg, f);
		if (!f->fp) {
			mme->nr_refcnt_miss ++;
			mme->nr_refcnt_miss_bytes += LQE_VALEN(lqe);
			goto finish;
		} else {
			/* write CDR to disk ASAP */
			if (!write_lqe_data (f, (const char *)&lqe->value[0], LQE_VALEN(lqe))) {
				mme->nr_refcnt ++;
				mme->nr_refcnt_bytes += LQE_VALEN(lqe);
			} else {
				mme->nr_refcnt_error ++;
				mme->nr_refcnt_error_bytes += LQE_VALEN(lqe);
			}
		}
#else
		mme->nr_refcnt ++;
		mme->nr_refcnt_bytes += LQE_VALEN(lqe);
#endif
	}

finish:
	/* unlock MME */
	MME_UNLOCK(mme);
	return;
}

static __oryx_always_inline__
void do_classify_online(const char *value, size_t vlen)
{
	char		*p, time[32] = {0};
	int			sep_refcnt = 0;
	const char	sep = ',';
	size_t		step, tlen = 0;
	time_t		tv_usec = 0;
	struct lq_element_t *lqe, lqe0;
	struct oryx_lq_ctx_t *lq;
	bool keep_cpy = 1, event_start_time_cpy = 0, find_imsi = 0;
	vlib_main_t *vm = &vlib_main;

	lqe = &lqe0;
	memset (lqe, 0, sizeof (struct lq_element_t));
	
	vm->nr_rx_entries ++;

	for (p = (const char *)&value[0], step = 0, sep_refcnt = 0;
				*p != '\0' && *p != '\n'; ++ p, step ++) {			

		/* skip entries without IMSI */
		if (!find_imsi && sep_refcnt == 5) {
			if (*p == sep) {
				vm->nr_rx_entries_without_imsi ++;
				//break;
			}
			else
				find_imsi = 1;
		}

		/* skip first 2 seps and copy event_start_time */
		if (sep_refcnt == 2) {
			event_start_time_cpy = 1;
		}

		/* skip last three columns */
		if (sep_refcnt == 43)
			keep_cpy = 0;

		/* valid entry dispatch ASAP */
		if (sep_refcnt == 45)
			goto dispatch;

		/* soft copy */
		if (keep_cpy) {
			lqe->value[lqe->valen ++] = *p;
		}

		if (*p == sep)
			sep_refcnt ++;

		/* stop copy */
		if (sep_refcnt == 3) {
			event_start_time_cpy = 0;
		}

		/* soft copy.
		 * Time stamp is 13 bytes-long */
		if (event_start_time_cpy) {
			time[tlen ++] = *p;
		}
	}

	vm->nr_rx_entries_undispatched ++;
	return;

dispatch:
 
	lqe->value[lqe->valen - 1] = '\n';
	lqe->value[lqe->valen] = '\0';
	
	/* soft copy. */
	lqe->ul_flags	= find_imsi ? LQE_HAVE_IMSI : 0;
	lqe->rawlen		= vlen;
	sscanf(time, "%lu", &tv_usec);	
	calc_tm_grid(&lqe->vtg, vm->threshold, tv_usec/1000);
	
	fmt_mme_ip(lqe->mme_ip, p);
	lqe->mme = mme_find_ip_h(vm->mme_htable, lqe->mme_ip, strlen(lqe->mme_ip));

	write_lqe(lqe->mme, lqe);
	vm->nr_rx_entries_dispatched ++;
}

extern void classify_env_init(vlib_main_t *vm);
extern void classify_terminal(void);
extern void classify_runtime(void);

#endif

