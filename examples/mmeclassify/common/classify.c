#include "oryx.h"
#include "config.h"

static __oryx_unused__
vlib_main_t vlib_main_template = {
	.argc		=	0,
	.argv		=	NULL,
	.prgname	=	"mme_classify",
	.nr_mmes	=	0,
	.mme_htable	=	NULL,
	.nr_threads =	0,
	.dictionary =	"/vsu/data2/zidian/classifyMme",
	.classdir   =	"/vsu1/db/cdr_csv/event_GEO_LTE/formated",
	.savdir     =	"/vsu1/db/cdr_csv/event_GEO_LTE/sav",
	.inotifydir =	"/vsu/db/cdr_csv/event_GEO_LTE",
	//.inotifydir = "/data",
	.threshold  =	5,
	.dispatch_mode        = HASH,
	.max_entries_per_file = 80000,
};
	
uint32_t	epoch_time_sec;
vlib_threadvar_t vlib_tv_main[VLIB_MAX_LQ_NUM];
char 	*inotify_home = "/vsu/db/cdr_csv/event_GEO_LTE",
		inotify_file[BUFSIZ];
const char *classify_home = "/root/classify_home";
const char *formated_home = "/vsu1/db/cdr_csv/event_GEO_LTE/formated";

static __oryx_always_inline__
void update_event_start_time(char *value, size_t valen)
{
	char *p;
	int sep_refcnt = 0;
	const char sep = ',';
	size_t step;
	bool event_start_time_cpy = 0, find_imsi = 0;
	char *ps = NULL, *pe = NULL;
	char ntime[32] = {0};
	struct timeval t;
	uint64_t time;

	gettimeofday(&t, NULL);
	time = (t.tv_sec * 1000 + (t.tv_usec / 1000));
	
	for (p = (const char *)&value[0], step = 0, sep_refcnt = 0;
				*p != '\0' && *p != '\n'; ++ p, step ++) {			

		/* skip entries without IMSI */
		if (!find_imsi && sep_refcnt == 5) {
			if (*p == sep) 
				continue;
			else
				find_imsi = 1;
		}

		/* skip first 2 seps and copy event_start_time */
		if (sep_refcnt == 2) {
			event_start_time_cpy = 1;
		}

		/* valid entry dispatch ASAP */
		//if (sep_refcnt == 45 && find_imsi)
		if (sep_refcnt == 45)
			goto update_time;

		if (*p == sep)
			sep_refcnt ++;

		/* stop copy */
		if (sep_refcnt == 3) {
			event_start_time_cpy = 0;
		}

		/* soft copy.
		 * Time stamp is 13 bytes-long */
		if (event_start_time_cpy) {
			if (ps == NULL)
				ps = p;
			pe = p;
		}

	}

	/* Wrong CDR entry, go back to caller ASAP. */
	//fprintf (stdout, "not update time\n");
	//return;

update_time:

	sprintf (ntime, "%lu", time);
	strncpy (ps, ntime, (pe - ps + 1));
	//fprintf (stdout, "(len %lu, %lu), %s", (pe - ps + 1), time, value);
	return;
}

static __oryx_always_inline__
struct oryx_lq_ctx_t * fetch_lq(uint64_t lq_id, struct oryx_lq_ctx_t **lq) {
	vlib_main_t *vm = &vlib_main;
	/* fetch an LQ */
	vlib_threadvar_t *vtc = &vlib_tv_main[lq_id % vm->nr_threads];
	(*lq) = vtc->lq;
}

static __oryx_always_inline__
vlib_threadvar_t *vlib_alloc_tv(void)
{
	vlib_main_t *vm = &vlib_main;
	return &vlib_tv_main[vm->nr_mmes % vm->nr_threads];
}

static __oryx_always_inline__
vlib_file_t **vlib_alloc_file(void)
{
	vlib_main_t *vm = &vlib_main;
	const int nr_blocks = (24 * 60) / vm->threshold;
	return (vlib_file_t **)malloc(sizeof(vlib_file_t) * nr_blocks);
}

static void load_dictionary(vlib_main_t *vm)
{
#define mme_dict_length	256

	char		*p,
				sep = ',',
				ip[32] = {0},
				name[32] = {0},
				line[mme_dict_length] = {0};

	void		*s;
	FILE *fp;
	
	const char	*file = "src/cfg/dictionary",
				*pdir = vm->classdir,
				*sdir = vm->savdir;
	
	size_t		step,
				nlen,
				iplen;

	int			i,
				sep_refcnt = 0,
				err;

	vlib_mmekey_t	*mmekey;
	vlib_mme_t		*mme;

	/* Overwrite MME dictionary. */
	if (vm->dictionary)
		file = vm->dictionary;
	
	fp = fopen(file, "r");
	if(!fp) {
		fprintf (stdout, "Cannot open %s \n", file);
		exit(0);
	}

	while (fgets (line, mme_dict_length, fp)) {

		if(line[0] == '#') {
			memset (line, 0, mme_dict_length);
			continue;
		}
		
		for (p = &line[0], step = 0, sep_refcnt = 0;
				*p != '\0' && *p != '\n'; ++ p, step ++) {

			if (*p == sep)
				sep_refcnt ++;
			
			if (sep_refcnt == 1) {

				/* Parse MME name. */			
				memset (&name[0], 0, 32);
				memcpy (&name[0], &line[0], (p - line));

				/* skip the last sep ',' */
				++ p;
				
				/* Parse MME IP */
				memset (&ip[0], 0, 32);
				fmt_mme_ip(ip, p);

				nlen = strlen(name);
				iplen = strlen(ip);
				
				/* find same mme */
				s = oryx_htable_lookup(vm->mme_htable, ip, iplen);
				if (s) {
					mmekey = (vlib_mmekey_t *) container_of (s, vlib_mmekey_t, ip);
					if ((mme = mmekey->mme) != NULL) {
						fprintf (stdout, "find same mme %s by %s\n", mme->name, mmekey->ip);
					} else {
						fprintf (stdout, "Cannot assign a MME for MMEKEY! exiting ...\n");
						exit (0);
					}
				} else {
					/* find and alloc MME */
					if ((mme = mme_find(name, nlen)) == NULL) {
						mme = mme_alloc(name, nlen);
						if(mme){
							/* bind a thread for this MME */
							if(vm->nr_threads){
								mme->lq_id = (vm->nr_mmes % vm->nr_threads);
								mme->tv = vlib_alloc_tv();
								//mme->farray = vlib_alloc_file();
							}
							vm->nr_mmes ++;
						} else {
							fprintf (stdout, "Cannot alloc MME %s\n", name);
							exit(0);
						}
					}
					/* alloc MMEKEY */
					mmekey = mmekey_alloc();
					if(!mmekey)
						break;

					mme->ip_str[mme->nr_ip ++] = strdup(ip);
					mmekey->mme = mme;
					memcpy(&mmekey->ip[0], &ip[0], iplen);
					
					/* add mmekey dictionary to hash table.
					 * Actually, there are more than 1 IP for one MME.
					 * So, use IP of which MME as index to find unique MME> */
					BUG_ON(oryx_htable_add(vm->mme_htable, mmekey->ip, strlen(mmekey->ip)) != 0);
				}
				break;
			}
		}
		memset (line, 0, mme_dict_length);
	}
	
	fclose (fp);

	/* Prepare Default MME (the last one) */
	default_mme = mme_alloc("default", strlen("default"));
	if(default_mme) {
		if(vm->nr_threads){
			default_mme->lq_id = (vm->nr_mmes % vm->nr_threads);
			default_mme->tv = vlib_alloc_tv();
			//default_mme->farray = vlib_alloc_file();
		}
		vm->nr_mmes ++;
	}

	err = oryx_mkdir(classify_home, NULL);
	if (err) {
		fprintf(stdout, "mkdir: %s (%s)\n",
			oryx_safe_strerror(errno), pdir);
	}

	for (i = 0; i < vm->nr_mmes; i ++) {
		mme = &nr_global_mmes[i];
		fprintf(stdout, "MME (%s, %p):\n", mme->name, mme);
		if(mme->tv)
			fprintf(stdout, "\tbind to tv %u\n", mme->tv->unique_id);
		fprintf(stdout, "\t(");
		int j = 0;
		for (j = 0; j < mme->nr_ip; j ++)
			fprintf(stdout, "\"%s\"", mme->ip_str[j]);
		fprintf(stdout, ")\n");
	}

	err = oryx_mkdir(pdir, NULL);
	if (err) {
		fprintf(stdout, "mkdir: %s (%s)\n",
			oryx_safe_strerror(errno), pdir);
	}
	
	err = oryx_mkdir(sdir, NULL);
	if (err) {
		fprintf(stdout, "mkdir: %s (%s)\n",
			oryx_safe_strerror(errno), sdir);
	}

	/* Prepare Classify CSV files for each MME. */
	for (i = 0; i < vm->nr_mmes; i ++) {	
		mme = &nr_global_mmes[i];

		MME_LOCK(mme);
		sprintf (mme->path, "%s/%s", pdir, mme->name);
		sprintf (mme->pathr, "%s", sdir);
		fprintf (stdout, "mkdir %s \n", mme->path);
		err = oryx_mkdir(mme->path, NULL);
		if (err) {
			fprintf(stdout, "mkdir: %s (%s)\n",
				oryx_safe_strerror(errno), mme->path);
		}	
		/* Prepare a new CSV file.
		 * At this stage, mme->fp is definable NULL.
		 * To make sure that there is a valid file handler 
		 * before writing CSV to disk. */

		/* File for current stage */
		BUG_ON(mme->file.fp != NULL);
		MME_UNLOCK(mme);
	}

}

void classify_terminal(void)
{
	int i;
	struct oryx_lq_ctx_t *lq;
	vlib_threadvar_t *vtc;
	vlib_main_t *vm = &vlib_main;

	for (i = 0; i < vm->nr_threads; i ++) {
		vtc = &vlib_tv_main[i];
		lq = vtc->lq;
		fprintf (stdout, "LQ[%d], nr_eq_refcnt %ld, nr_dq_refcnt %ld, buffered %lu(%d)\n",
			i, lq->nr_eq_refcnt, lq->nr_dq_refcnt, (lq->nr_eq_refcnt - lq->nr_dq_refcnt), lq->len);
		oryx_lq_destroy(lq);
	}
}

static __oryx_always_inline__
int baker_entry(const char *value, size_t valen, struct lq_element_t *lqe)
{
	char		*p,
				*s,
				time[32] = {0};
	const char	sep = ',';
	int 		sep_refcnt = 0;
	size_t		tlen = 0;
	time_t		tv_usec = 0;
	bool		keep_cpy				= true,
				event_start_time_cpy	= false,
				find_imsi				= false;

	for (p = (const char *)&value[0], sep_refcnt = 0;
				*p != '\0' && *p != '\n'; ++ p) {			

		/* skip entries without IMSI */
		if (sep_refcnt == 5 && !find_imsi) {
			if (*p == sep) {
				find_imsi = 0;
				//break;
			}
			else {
				find_imsi = 1;
				//fprintf(stdout, "%s\n", p);
			}
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

	/* error */
	return -1;
	
dispatch:

	//fmt_mme_ip(lqe->mme_ip, p);
	for (p, s = &lqe->mme_ip[0]; *p != '\0' && *p != '\n'; ++ p, ++ s) {
		*s = *p;
		lqe->iplen ++;
	}
	sscanf(time, "%lu", &tv_usec);

	lqe->value[lqe->valen]		= '\0';
	lqe->value[lqe->valen - 1]	= '\n';
	lqe->rawlen 				= valen;
	lqe->tv_sec 				= tv_usec/1000;
	if(find_imsi) lqe->ul_flags |= LQE_HAVE_IMSI;
	//else fprintf(stdout, "%s", lqe->value);

	return 0;
}

static __oryx_always_inline__
int write_lqe_data(vlib_file_t *f, const char *value, size_t valen)
{
	size_t nr_wb;

#if defined(HAVE_F_CACHE)
	/* call fwrite to flush them all */
	if(f->offset + valen > file_cache_size) {
		fwrite(f->cache, sizeof(char), f->offset, f->fp);
		f->offset = 0;
	}
	memcpy(f->cache + f->offset, value, valen);
	f->offset += valen;
	nr_wb = valen;
#else
	nr_wb = file_write(f, value, valen);
#endif
	return nr_wb == valen ? 0 : 1;
}


static __oryx_always_inline__
void  mme_try_fopen(vlib_mme_t *mme,
		const vlib_tm_grid_t *vtg, vlib_file_t *f)
{
	char *home = NULL;
	
	if (f->fp == NULL) {
		goto try_new_file;
	} else {
		/* check current file is valid for use. */
		if (f->local_time != vtg->start) {
			/* close previous file before open a new one. */
			file_close(f);
			fprintf(stdout, "file closed\n");
			goto try_new_file;
		}
	}
	BUG_ON(f->fp == NULL);
	return;
	
try_new_file:

#if defined(HAVE_CLASSIFY_HOME)
	home = classify_home;
#else
	home = formated_home;
#endif
	file_open(home, mme->name, vtg, f);
}

static __oryx_always_inline__
void write_lqe(vlib_mme_t *mme, const struct lq_element_t *lqe)
{
	vlib_file_t 			*f;
	vlib_main_t				*vm = &vlib_main;
	size_t					valen = LQE_VALEN(lqe);
	const vlib_tm_grid_t	*vtg = &lqe->vtg;

	/* lock MME */
	//MME_LOCK(mme);
	//f  = &mme->file;
	f 	=  &mme->farray[vtg->block];
	
	ATOMIC64_INC(&mme->nr_rx_entries);
	ATOMIC64_INC(&mme->nr_refcnt_r);
	ATOMIC64_ADD(&mme->nr_refcnt_bytes_r, valen);	

	/* no IMSI find in this entry. */
	if(!(lqe->ul_flags & LQE_HAVE_IMSI)) {
		ATOMIC64_INC(&mme->nr_rx_entries_noimsi);
		ATOMIC64_INC(&vm->nr_rx_entries_without_imsi);
		goto finish;
	} else {
		mme_try_fopen(mme, vtg, f);
		if (!f->fp) {
			ATOMIC64_INC(&mme->nr_refcnt_miss);
			ATOMIC64_ADD(&mme->nr_refcnt_miss_bytes, valen);
			goto finish;
		} else {
			/* write CDR to disk ASAP */
			if (!write_lqe_data (f, (const char *)&lqe->value[0], valen)) {
				ATOMIC64_INC(&mme->nr_refcnt);
				ATOMIC64_ADD(&mme->nr_refcnt_bytes, valen);
			} else {
				ATOMIC64_INC(&mme->nr_refcnt_error);
				ATOMIC64_ADD(&mme->nr_refcnt_error_bytes, valen);
			}
		}
	}

finish:
	/* unlock MME */
	//MME_UNLOCK(mme);
	return;
}

static __oryx_always_inline__
void do_classify_final(struct lq_element_t *lqe)
{
	write_lqe(lqe->mme, lqe);
}

static __oryx_always_inline__
void do_dispatch(const char *value, size_t vlen)
{
	struct lq_element_t *lqe;
	vlib_threadvar_t *tv;
	vlib_mme_t *mme;
	vlib_main_t *vm = &vlib_main;
	struct oryx_lq_ctx_t *lq;

	ATOMIC64_INC(&vm->nr_rx_entries);

	if(!vm->nr_threads)
		return;
	
	lqe = lqe_alloc();
	if(!lqe) {
		ATOMIC64_INC(&vm->nr_rx_entries_undispatched);
	} else {
		if(!baker_entry(value, vlen, lqe)) {
			lqe->mme	= mme_find_ip_h(vm->mme_htable, lqe->mme_ip, strlen(lqe->mme_ip));
			calc_tm_grid(&lqe->vtg, vm->threshold, lqe->tv_sec);
			mme = lqe->mme;
			tv  = mme->tv;
			oryx_lq_enqueue(tv->lq, lqe);
			ATOMIC64_INC(&vm->nr_rx_entries_dispatched);
		} else {
			ATOMIC64_INC(&vm->nr_rx_entries_undispatched);			
		}
	}
}

static __oryx_always_inline__
void do_classify_entry 
(
	IN const char *value,
	IN size_t valen
)
{
	vlib_main_t *vm = &vlib_main;
	struct lq_element_t *lqe,
						lqe0 = {
							.mme	= NULL,
							.lp		= {NULL, NULL},
							.vtg	= {0, 0},
							.mme_ip = {0},
							.value	= {0},
							.valen	= 0,
							.rawlen = 0,
							.ul_flags = 0
						};

	lqe = &lqe0;

	ATOMIC64_INC(&vm->nr_rx_entries);
	
	if(!baker_entry(value, valen, lqe)) {
		lqe->mme	= mme_find_ip_h(vm->mme_htable, lqe->mme_ip, lqe->iplen);
		calc_tm_grid(&lqe->vtg, vm->threshold, lqe->tv_sec);
		do_classify_final(lqe);
		ATOMIC64_INC(&vm->nr_rx_entries_dispatched);
	} else {
		ATOMIC64_INC(&vm->nr_rx_entries_undispatched);
	}
}

static void mme_stat (const FILE *fp)
{
	int i;
	vlib_mme_t *mme;
	vlib_main_t *vm = &vlib_main;
	struct oryx_fmt_buff_t fb = FMT_BUFF_INITIALIZATION;

	char					fmtstring[128] = {0},
							pps_str[20],
							bps_str[20],
							pps_str1[20];
	uint64_t	mme_nr_rx_entries,
				mme_nr_rx_entries_noimsi,
				mme_nr_refcnt,
				mme_nr_refcnt_r,
				mme_nr_refcnt_miss,
				mme_nr_refcnt_error,
				nr_classified_refcnt	= 0,
				nr_classified_refcnt_r	= 0,
				nr_missed_refcnt		= 0,
				nr_error_refcnt 		= 0,
				vm_nr_rx_entries = ATOMIC64_READ(&vm->nr_rx_entries);

	//fprintf (fp, "\n\n");
	fprintf (fp, "\tStatistics for %d MME(s)\n", vm->nr_mmes);
	fprintf (fp, "%24s%64s%32s%32s\n", " ", "Rx(Rx%Rx_TOTAL), -IMSI(-IMSI%Rx)", "+WR(+WR%Rx)", "+W(+W%Rx) Delta");
	fprintf (fp, "%24s%64s%32s%32s\n", " ", "---------------------------------", "-----------", "--------------");
	fflush(fp);

	/** Statistics for each MME */
	for (i = 0; i < vm->nr_mmes; i ++) {
		oryx_format_reset(&fb);
		mme = &nr_global_mmes[i];

		mme_nr_rx_entries = ATOMIC64_READ(&mme->nr_rx_entries);
		mme_nr_refcnt = ATOMIC64_READ(&mme->nr_refcnt);
		mme_nr_refcnt_r = ATOMIC64_READ(&mme->nr_refcnt_r);
		mme_nr_refcnt_miss = ATOMIC64_READ(&mme->nr_refcnt_miss);
		mme_nr_refcnt_error = ATOMIC64_READ(&mme->nr_refcnt_error);
		mme_nr_rx_entries_noimsi = ATOMIC64_READ(&mme->nr_rx_entries_noimsi);

		nr_missed_refcnt += mme_nr_refcnt_miss;
		nr_error_refcnt  += mme_nr_refcnt_error;
		
		/* format MME name */	
		oryx_format(&fb, "%24s", mme->name);

		/* format MME refcnt */
		sprintf (fmtstring, "%lu (%.2f%%), %lu (%.2f%%)",
				mme_nr_rx_entries, ratio_of(mme_nr_rx_entries, vm_nr_rx_entries),
				mme_nr_rx_entries_noimsi, ratio_of(mme_nr_rx_entries_noimsi, mme_nr_rx_entries));
		oryx_format(&fb, "%64s", fmtstring);

		sprintf (fmtstring, "%lu (%.2f%%)",
				mme_nr_refcnt_r, ratio_of(mme_nr_refcnt_r, mme_nr_rx_entries));
		oryx_format(&fb, "%32s", fmtstring);

		int delta = ((mme_nr_rx_entries - mme_nr_rx_entries_noimsi) - mme_nr_refcnt);
		sprintf (fmtstring, "%lu (%.2f%%) %u",
				mme_nr_refcnt,
				ratio_of(mme_nr_refcnt, mme_nr_rx_entries),
				delta);
		oryx_format(&fb, "%32s", fmtstring);

		/* flush to file */
		oryx_format(&fb, "%s", "\n");
		fprintf(fp, FMT_DATA(fb), FMT_DATA_LENGTH(fb));
		fflush(fp);

		nr_classified_refcnt += mme_nr_refcnt;
		nr_classified_refcnt_r += mme_nr_refcnt_r;
	}

	/* Summary */
	/* format Last line for summary */
	oryx_format_reset(&fb);
	oryx_format(&fb, "%24s", " ");
	oryx_format(&fb, "%64s", " ");
	oryx_format(&fb, "%32s", " ");
	sprintf (fmtstring, "%s (%.2f%%) %s",
			" ",
			ratio_of(nr_classified_refcnt, vm->nr_rx_entries),
			" ");	
	oryx_format(&fb, "%32s", fmtstring);
	oryx_format(&fb, "%s", 	 "\n");
	fprintf(fp, FMT_DATA(fb), FMT_DATA_LENGTH(fb));
	fflush(fp);

	oryx_format_free(&fb);
	fprintf (fp, "\n\n");
	fflush(fp);

}

static void classify_tmr_handler (
				struct oryx_timer_t __oryx_unused__ *tmr,
				int __oryx_unused__ argc, 
                char __oryx_unused__**argv)
{
	int i;
	static uint64_t 			duration = 0,
								nr_eq_swap_bytes_prev[VLIB_MAX_LQ_NUM]		= {0},
								nr_eq_swap_elements_prev[VLIB_MAX_LQ_NUM]	= {0},
								nr_dq_swap_bytes_prev[VLIB_MAX_LQ_NUM]		= {0},
								nr_dq_swap_elements_prev[VLIB_MAX_LQ_NUM]	= {0},
								nr_rx_entries_prev = 0;
	
	static size_t				lqe_size = lq_element_size;
	static struct timeval		start,
								end;

	uint64_t					nr_eq_swap_bytes_cur[VLIB_MAX_LQ_NUM]		= {0},
								nr_eq_swap_elements_cur[VLIB_MAX_LQ_NUM]		= {0},
								nr_dq_swap_bytes_cur[VLIB_MAX_LQ_NUM]		= {0},
								nr_dq_swap_elements_cur[VLIB_MAX_LQ_NUM]		= {0},
								eq[VLIB_MAX_LQ_NUM] = {0},
								dq[VLIB_MAX_LQ_NUM] = {0},
								nr_eq_total_refcnt = 0,
								nr_dq_total_refcnt = 0,
								nr_classified_refcnt	= 0,
								nr_classified_refcnt_r	= 0,
								nr_missed_refcnt		= 0,
								nr_error_refcnt 		= 0,
								nr_unclassified_refcnt	= 0,
								nr_cost_us = 0,
								nr_rx_entries = 0;

	struct oryx_fmt_buff_t fb = FMT_BUFF_INITIALIZATION;

	char					fmtstring[128] = {0},
							pps_str[20],
							bps_str[20],
							pps_str1[20],
							clearfile[128] = "cat /dev/null > ";
	struct oryx_lq_ctx_t *lq;
	vlib_threadvar_t *vtc;
	vlib_mme_t *mme;
	vlib_main_t *vm = &vlib_main;
	static FILE *fp;
	const char *lq_runtime_file = "/var/run/classify_runtime_summary.txt";

	strcat(clearfile, lq_runtime_file);
	do_system(clearfile);

	if(!fp) {
		fp = fopen(lq_runtime_file, "a+");
		if(!fp) fp = stdout;
	}

	gettimeofday(&end, NULL);
	/* cost before last time. */
	nr_cost_us = oryx_elapsed_us(&start, &end);
	gettimeofday(&start, NULL);

	uint64_t vm_nr_rx_entries = ATOMIC64_READ(&vm->nr_rx_entries);
	uint64_t vm_nr_rx_entries_without_imsi = ATOMIC64_READ(&vm->nr_rx_entries_without_imsi);
	uint64_t vm_nr_rx_entries_undispatched = ATOMIC64_READ(&vm->nr_rx_entries_undispatched);
	uint64_t vm_nr_rx_entries_dispatched = ATOMIC64_READ(&vm->nr_rx_entries_dispatched);

	nr_rx_entries = vm_nr_rx_entries - nr_rx_entries_prev;
	nr_rx_entries_prev = vm_nr_rx_entries;
	
	duration += (nr_cost_us / 1000000);

	/** Statistics for each QUEUE */
	for (i = 0; i < vm->nr_threads; i ++) {
		vtc = &vlib_tv_main[i];
		lq = vtc->lq;

		nr_unclassified_refcnt += vtc->nr_unclassified_refcnt;

		dq[i] = lq->nr_dq_refcnt;
		eq[i] = lq->nr_eq_refcnt;

		nr_eq_total_refcnt += eq[i];
		nr_dq_total_refcnt += dq[i];
		
		nr_eq_swap_bytes_cur[i] 	=	(eq[i] * lqe_size) - nr_eq_swap_bytes_prev[i];
		nr_eq_swap_elements_cur[i]	=	eq[i] - nr_eq_swap_elements_prev[i];
		
		nr_dq_swap_bytes_cur[i] 	=	(dq[i] * lqe_size) - nr_dq_swap_bytes_prev[i];
		nr_dq_swap_elements_cur[i]	=	dq[i] - nr_dq_swap_elements_prev[i];
		
		nr_eq_swap_bytes_prev[i]	=	(eq[i] * lqe_size);
		nr_eq_swap_elements_prev[i] =	eq[i];
		
		nr_dq_swap_bytes_prev[i]	=	(dq[i] * lqe_size);
		nr_dq_swap_elements_prev[i] =	dq[i];
	}

	fprintf (fp, "Cost %lu us, duration %lu sec\n", nr_cost_us, (duration - epoch_time_sec));
	fprintf (fp, "\n");
	fflush(fp);
	
	fprintf (fp, "Classify Configurations\n");
	fprintf (fp, "\tdictionary: %s\n", vm->dictionary);
	fprintf (fp, "\tthreshold : %d minute(s)\n", vm->threshold);
	fprintf (fp, "\twarehouse : %s\n", vm->classdir);
	fprintf (fp, "\tsavehouse : %s\n", vm->savdir);
	fprintf (fp, "\tparallel  : %d thread(s)\n", vm->nr_threads);
	fprintf (fp, "\n");
	fflush(fp);

	fprintf (fp, "Statistics for %d thread(s)\n", vm->nr_threads);
	for (i = 0; i < vm->nr_threads; i ++) {
		fprintf (fp, "\tLQ[%d], blocked %lu element(s)\n", i, eq[i] - dq[i]);
		fprintf (fp, "\t\t enqueue %.2f%% %lu (pps %s, bps %s)\n",
			ratio_of(eq[i], nr_eq_total_refcnt),
			eq[i],
			oryx_fmt_program_counter(fmt_pps(nr_cost_us, nr_eq_swap_elements_cur[i]), pps_str, 0, 0),
			oryx_fmt_program_counter(fmt_bps(nr_cost_us, nr_eq_swap_bytes_cur[i]), bps_str, 0, 0));
		fprintf (fp, "\t\t dequeue %.2f%% %lu (pps %s, bps %s)\n",
			ratio_of(dq[i], nr_eq_total_refcnt),
			dq[i],
			oryx_fmt_program_counter(fmt_pps(nr_cost_us, nr_dq_swap_elements_cur[i]), pps_str, 0, 0),
			oryx_fmt_program_counter(fmt_bps(nr_cost_us, nr_dq_swap_bytes_cur[i]), bps_str, 0, 0));
	}
	fflush(fp);
	

	fprintf (fp, "\n\n");
	fprintf (fp, "Statistics for %d MME(s)\n", vm->nr_mmes);
	fprintf (fp, "%24s%64s%32s%32s\n", " ", "Rx(Rx%Rx_TOTAL), -IMSI(-IMSI%Rx)", "+WR(+WR%Rx)", "+W(+W%Rx) Delta");
	fprintf (fp, "%24s%64s%32s%32s\n", " ", "---------------------------------", "-----------", "--------------");
	fflush(fp);


	uint64_t	mme_nr_rx_entries,
				mme_nr_rx_entries_noimsi,
				mme_nr_refcnt,
				mme_nr_refcnt_r,
				mme_nr_refcnt_miss,
				mme_nr_refcnt_error;
				
	/** Statistics for each MME */
	for (i = 0; i < vm->nr_mmes; i ++) {
		oryx_format_reset(&fb);
		mme = &nr_global_mmes[i];

		mme_nr_rx_entries = ATOMIC64_READ(&mme->nr_rx_entries);
		mme_nr_refcnt = ATOMIC64_READ(&mme->nr_refcnt);
		mme_nr_refcnt_r = ATOMIC64_READ(&mme->nr_refcnt_r);
		mme_nr_refcnt_miss = ATOMIC64_READ(&mme->nr_refcnt_miss);
		mme_nr_refcnt_error = ATOMIC64_READ(&mme->nr_refcnt_error);
		mme_nr_rx_entries_noimsi = ATOMIC64_READ(&mme->nr_rx_entries_noimsi);

		nr_missed_refcnt += mme_nr_refcnt_miss;
		nr_error_refcnt  += mme_nr_refcnt_error;
		
		/* format MME name */	
		oryx_format(&fb, "%24s", mme->name);

		/* format MME refcnt */
		sprintf (fmtstring, "%lu (%.2f%%), %lu (%.2f%%)",
				mme_nr_rx_entries, ratio_of(mme_nr_rx_entries, vm_nr_rx_entries),
				mme_nr_rx_entries_noimsi, ratio_of(mme_nr_rx_entries_noimsi, mme_nr_rx_entries));
		oryx_format(&fb, "%64s", fmtstring);

		sprintf (fmtstring, "%lu (%.2f%%)",
				mme_nr_refcnt_r, ratio_of(mme_nr_refcnt_r, mme_nr_rx_entries));
		oryx_format(&fb, "%32s", fmtstring);

		int delta = ((mme_nr_rx_entries - mme_nr_rx_entries_noimsi) - mme_nr_refcnt);
		sprintf (fmtstring, "%lu (%.2f%%) %u",
				mme_nr_refcnt, ratio_of(mme_nr_refcnt, mme_nr_rx_entries), delta);
		oryx_format(&fb, "%32s", fmtstring);

		/* flush to file */
		oryx_format(&fb, "%s", "\n");
		fprintf(fp, FMT_DATA(fb), FMT_DATA_LENGTH(fb));
		fflush(fp);

		nr_classified_refcnt += mme_nr_refcnt;
		nr_classified_refcnt_r += mme_nr_refcnt_r;
	}
	oryx_format_free(&fb);
	fprintf (fp, "\n\n");
	fflush(fp);
	
	/* Overall classified ratio. */
	fprintf (fp, "Statistics Summary\n");
	fprintf (fp, "\trx_files %lu, +classified_files %lu\n",
		ATOMIC64_READ(&vm->nr_rx_files), nr_classified_files);
	fprintf (fp, "\tRx %lu, +dispatched %lu (%.2f%%), -imsi %lu (%.2f%%)\n",
			vm_nr_rx_entries,
			vm_nr_rx_entries_dispatched, ratio_of(vm_nr_rx_entries_dispatched, vm_nr_rx_entries),
			vm_nr_rx_entries_without_imsi, ratio_of(vm_nr_rx_entries_without_imsi, vm_nr_rx_entries));
	fprintf (fp, "\t+WR %lu (%.2f%%), +W %lu (%.2f%%), -classified: %lu (%.2f%%)\n",
			nr_classified_refcnt_r, ratio_of(nr_classified_refcnt_r, vm_nr_rx_entries),
			nr_classified_refcnt,	ratio_of(nr_classified_refcnt, vm_nr_rx_entries),
			nr_unclassified_refcnt, ratio_of(nr_unclassified_refcnt, vm_nr_rx_entries));
	fprintf (fp, "\tDebug\n");
	fprintf (fp, "\t\t%lu opened handler(s), fopen times (f %lu, fr %lu, fe %lu)\n",
				nr_handlers, nr_fopen_times, nr_fopen_times_r, nr_fopen_times_error);
	fprintf (fp, "\t\t-dispatched %lu, +w miss %lu, +w error %lu\n",
				vm_nr_rx_entries_undispatched, nr_missed_refcnt, nr_error_refcnt);
	fprintf (fp, "\t\teq_ticks %lu, dq_ticks %lu\n", vm->nr_thread_eq_ticks, vm->nr_thread_dq_ticks);
	fprintf (fp, "\t\tinotify (%lu/%lu, pps %s/s) %s\n", lq_nr_dq(fmgr_q), lq_nr_eq(fmgr_q),
					oryx_fmt_program_counter(fmt_pps(nr_cost_us, nr_rx_entries), pps_str1, 0, 0), inotify_file);
	fprintf (fp, "\n");
	fflush(fp);
	
}


static
void * dequeue_handler (void __oryx_unused__ *r)
{
		struct lq_element_t *lqe;
		vlib_threadvar_t *vtc = (vlib_threadvar_t *)r;
		struct oryx_lq_ctx_t *lq = vtc->lq;
		vlib_main_t *vm = &vlib_main;
		
		/* wait for enqueue handler start. */
		while (vm->ul_flags & VLIB_ENQUEUE_HANDLER_STARTED)
			break;

		fprintf(stdout, "Starting dequeue handler(%lu): vtc=%p, lq=%p\n", pthread_self(), vtc, lq);		
		FOREVER {
			lqe = NULL;
			vm->nr_thread_dq_ticks ++;
			
			if (!running) {
				/* wait for enqueue handler exit. */
				while (vm->ul_flags & VLIB_ENQUEUE_HANDLER_EXITED)
					break;
				fprintf (stdout, "dequeue exiting ... ");
				/* drunk out all elements before thread quit */					
				while (NULL != (lqe = oryx_lq_dequeue(lq))){
					do_classify_final(lqe);
					free(lqe);
				}
				fprintf (stdout, " exited!\n");
				break;
			}
			
			lqe = oryx_lq_dequeue(lq);
			if (lqe != NULL) {
				do_classify_final(lqe);
				free(lqe);
			}
		}

		oryx_task_deregistry_id(pthread_self());
		return NULL;
}

static struct oryx_task_t dequeue = {
		.module 		= THIS,
		.sc_alias		= "Dequeue Task0",
		.fn_handler 	= dequeue_handler,
		.lcore_mask		= 0x08,
		.ul_prio		= KERNEL_SCHED,
		.argc			= 1,
		.argv			= NULL,
		.ul_flags		= 0,	/** Can not be recyclable. */
};

void classify_initialization(vlib_main_t *vm)
{
	int i;
	uint32_t	lq_cfg = 0;
	vlib_threadvar_t *vtc;

	for (i = 0; i < vm->nr_threads; i ++) {
		vtc = &vlib_tv_main[i];
		vtc->unique_id = i;
		/* new queue */
		oryx_lq_new("A new list queue", lq_cfg, (void **)&vtc->lq);
		oryx_lq_dump(vtc->lq);

		char name[64] = {0};
		sprintf (name, "Dequeue Task%d", i);
		struct oryx_task_t *t = malloc (sizeof(struct oryx_task_t));
		BUG_ON(t == NULL);
		memset(t, 0, sizeof (struct oryx_task_t));
		memcpy(t, &dequeue, sizeof (struct oryx_task_t));
		t->lcore_mask = INVALID_CORE;
		//t->lcore_mask = (1 << (i + DEQUEUE_LCORE_ID));
		t->ul_prio = KERNEL_SCHED;
		t->argc = 1;
		t->argv = vtc;
		t->sc_alias = strdup(name);
		oryx_task_registry(t);
	}

	load_dictionary(vm);

#if 0
	oryx_tmr_start(oryx_tmr_create (1,
						"Classify Runtime TMR",
						TMR_OPTIONS_PERIODIC | TMR_OPTIONS_ADVANCED,
						classify_tmr_handler,
						0,
						NULL,
						3000));
#endif
}

int classify_result (const char *oldpath, vlib_conf_t *conf)
{
	FILE				*fp = NULL;
	char				pps_str0[20],
						pps_str1[20],
						file_size_buf[20] = {0},
						logfile[256] = {0};

	fprintf(stdout, "\nClassify Result\n");
	fprintf(stdout, "%3s%12s%s\n",	" ", "File: ",	oldpath);
	fprintf(stdout, "%3s%12s%lu/%lu, cost %lu usec\n", " ", "Entries: ",
		conf->nr_entries, conf->nr_entries, conf->tv_usec);
	fprintf(stdout, "%3s%12s%s/s, avg %s/s\n",	" ", "Speed: ",
		oryx_fmt_program_counter(fmt_pps(conf->tv_usec, conf->nr_local_entries), pps_str0, 0, 0),
		oryx_fmt_program_counter(fmt_pps(conf->tv_usec, conf->nr_local_entries), pps_str1, 0, 0));

	if (conf->ul_flags & VLIB_CONF_LOG) {
		sprintf(logfile, "%s/classify_result.log", classify_home);
		fp = fopen(logfile, "a+");
		if(!fp) {
			fprintf(stdout, "fopen: %s\n", oryx_safe_strerror(errno));
			return 0;
		}
		fprintf(fp, "Summary: %s (%lu entries, %s, cost %lu usec, %s/s)\n",
				oldpath,
				conf->nr_local_entries,
				oryx_fmt_program_counter(conf->nr_size, file_size_buf, 0, 0),
				conf->tv_usec,
				oryx_fmt_program_counter(fmt_pps(conf->tv_usec, conf->nr_local_entries), pps_str0, 0, 0));
		mme_stat(fp);
		fclose(fp);
	}
	
	mme_stat(stdout);
	return 0;
}

int classify_offline 
(
	IN const char *oldpath,
	IN vlib_conf_t *conf
)
{
	FILE				*fp = NULL;
	uint64_t			nr_local_entries = 0,
						nr_local_size = 0,
						tv_usec = 0;
	size_t				entry_size = 0;
	vlib_main_t			*vm = &vlib_main;
	char				entry[lqe_valen]	= {0};
	struct timeval		start,
						end;
	int err;

	fp = fopen(oldpath, "r");
	if(!fp) {
		fprintf (stdout, "fopen: %s \n", oryx_safe_strerror(errno));
		return -1;
	}

	ATOMIC64_INC(&vm->nr_rx_files);

	fprintf (stdout, "\nReading %s\n", oldpath);

	err = gettimeofday(&start, NULL);
	if(err) {
		fprintf(stdout, "gettimeofday: %s\n", oryx_safe_strerror(errno));
	}

	/* skip first lines, this line is a CSV header. */
	if(!fgets(entry, lqe_valen, fp)) {
		fprintf(stdout, "empty file\n");
		fclose(fp);
		goto finish;
	}

	/* A loop read the part of lines. */
	FOREVER {
		memset (entry, 0, lqe_valen);
		if(!fgets (entry, lqe_valen, fp) || !running)
			break;
		entry_size = strlen(entry);		
		nr_local_entries ++;
		nr_local_size += entry_size;
		do_classify_entry (entry, entry_size);
	}
	fclose(fp);

finish:	

	gettimeofday(&end, NULL);	
	tv_usec = oryx_elapsed_us(&start, &end);

	if (conf) {
		conf->nr_entries		= ATOMIC64_READ(&vm->nr_rx_entries);
		conf->nr_local_entries	= nr_local_entries;
		conf->nr_size			= nr_local_size;
		conf->tv_usec			= tv_usec;
	}

	/* clean up all file handlers */

#if 0	
#if defined(HAVE_SANTA_CPY2)
	err = remove(oldpath);
	if(err) {
		fprintf(stdout, "\nrm %s\n", oldpath, oryx_safe_strerror(errno));
	}
#else
	char	newpath[256] = {0},
			move[256] = {0};

	sprintf(newpath, "%s/", vm->savdir);
	sprintf(move, "mv %s %s", oldpath, newpath);
	fprintf(stdout, "\n(*)done, %s\n", move);
	err = do_system(move);
	if(err) {
		fprintf(stdout, "\nmv: %s\n", oryx_safe_strerror(errno));
	}
#endif
#else
	if (conf->ul_flags & VLIB_CONF_NONE)
		return 0;
	else if (conf->ul_flags & VLIB_CONF_MV) {
		char	newpath[256] = {0},
				move[256] = {0};

		sprintf(newpath, "%s/", vm->savdir);
		sprintf(move, "mv %s %s", oldpath, newpath);
		fprintf(stdout, "\n(*)done, %s\n", move);
		err = do_system(move);
		if(err) {
			fprintf(stdout, "\nmv: %s\n", oryx_safe_strerror(errno));
		}
	} else if (conf->ul_flags & VLIB_CONF_RM) {
		err = remove(oldpath);
		if(err) {
			fprintf(stdout, "\nremove: %s\n", oryx_safe_strerror(errno));
		}
	}
#endif
	return 0;
}

#if 0
static
void * enqueue_handler (void __oryx_unused__ *r)
{
		vlib_main_t *vm = &vlib_main;
		struct fq_element_t *fqe;
		
		vm->ul_flags |= VLIB_ENQUEUE_HANDLER_STARTED;
		FOREVER {
			vm->nr_thread_eq_ticks ++;
			if (!running) {
				fprintf (stdout, "Thread(%lu): enqueue exited!\n", pthread_self());
				vm->ul_flags |= VLIB_ENQUEUE_HANDLER_EXITED;

				/* drunk out all elements before thread quit */					
				while (NULL != (fqe = oryx_lq_dequeue(fmgr_q))){
					memset(inotify_file, 0, BUFSIZ);
					sprintf(inotify_file, "%s", fqe->name);
					vlib_conf_t conf = {
						.ul_flags = VLIB_CONF_MV
					};
					classify_offline(fqe->name, &conf); 	
					fmgr_move(fqe->name, &conf);
					free(fqe);
				}
				fprintf (stdout, " exited!\n");
				break;
			}
			
			fqe = oryx_lq_dequeue(fmgr_q);
			if (fqe == NULL)
				continue;
			if(!oryx_path_exsit(fqe->name)){
				free(fqe);
				continue;
			}					

			memset(inotify_file, 0, BUFSIZ);
			sprintf(inotify_file, "%s", fqe->name);				
			vlib_conf_t conf = {
				.ul_flags = VLIB_CONF_MV
			};
			classify_offline(fqe->name, &conf);		
			fmgr_move(fqe->name, &conf);

			free(fqe);
		}
		oryx_task_deregistry_id(pthread_self());
		return NULL;
}

struct oryx_task_t enqueue = {
		.module 		= THIS,
		.sc_alias		= "Enqueue Task",
		.fn_handler 	= enqueue_handler,
		.lcore_mask		= (1 << 2),//INVALID_CORE,
		.ul_prio		= KERNEL_SCHED,
		.argc			= 0,
		.argv			= NULL,
		.ul_flags		= 0,	/** Can not be recyclable. */
};
#endif
