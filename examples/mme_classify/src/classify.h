#ifndef CLASSIFY_H
#define CLASSIFY_H

#define LQE_HAVE_IMSI	(1 << 0)
struct lq_element_t {
#define lqe_valen	256
	struct lq_prefix_t		lp;
	/* used to find an unique MME. */
	char					mme_ip[32],
							value[lqe_valen];
	size_t					valen,
							rawlen;

	uint32_t				ul_flags;
	vlib_mme_t				*mme;
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
		const vlib_tm_grid_t *vtg, vlib_file_t *f)
{		
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

	file_open(classify_home, mme->name, vtg->start, vtg->end, f);
	if (f->ul_flags & VLIB_FILE_NEW) {
		fprintf(stdout, "Write CSV head to %s\n", f->filepath);
		/* Write CSV header before writting an entry
		 * when vlib_file_t is new. */
		file_write(f, MME_CSV_HEADER, strlen(MME_CSV_HEADER));
		f->ul_flags &= ~VLIB_FILE_NEW;
		/* add this file to head of file list.
		 * You know, we can image that a */
		oryx_list_add(&f->fnode, &mme->fhead);
	}

}

static __oryx_always_inline__
void write_lqe(vlib_mme_t *mme, const struct lq_element_t *lqe)
{
	vlib_file_t 			*f;
	size_t					valen = LQE_VALEN(lqe);
	const vlib_tm_grid_t	*vtg = &lqe->vtg;

	/* lock MME */
	//MME_LOCK(mme);

	f  = &mme->file;

	mme->nr_rx_entries ++;
	/* Only for test. */
	mme->nr_refcnt_r ++;
	mme->nr_refcnt_bytes_r += valen;

	if (!(lqe->ul_flags & LQE_HAVE_IMSI))
		mme->nr_rx_entries_noimsi ++;
	else {
		mme_try_fopen(mme, vtg, f);
		if (!f->fp) {
			mme->nr_refcnt_miss ++;
			mme->nr_refcnt_miss_bytes += valen;
			goto finish;
		} else {
			/* write CDR to disk ASAP */
			if (!write_lqe_data (f, (const char *)&lqe->value[0], valen)) {
				mme->nr_refcnt ++;
				mme->nr_refcnt_bytes += valen;
			} else {
				mme->nr_refcnt_error ++;
				mme->nr_refcnt_error_bytes += valen;
			}
		}
	}

finish:
	/* unlock MME */
	//MME_UNLOCK(mme);
	return;
}

extern void classify_env_init(vlib_main_t *vm);
extern void classify_terminal(void);
extern void classify_runtime(void);

#endif

