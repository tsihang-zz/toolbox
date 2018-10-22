#include "oryx.h"
#include "main.h"
//#include "libcommfunc.h"

vlib_main_t vlib_main = {
	.argc		=	0,
	.argv		=	NULL,
	.prgname	=	"mme_classify",
	.nr_mmes	=	0,
	.mme_htable	=	NULL,
	.nr_threads =	1,
	.dictionary =	"/vsu/data2/zidian/classifyMme",
	.classdir   =	"/vsu1/db/cdr_csv/event_GEO_LTE/formated",
	.savdir     =	"/vsu1/db/cdr_csv/event_GEO_LTE/sav",
	.inotifydir =	"/vsu/db/cdr_csv/event_GEO_LTE",
	//.inotifydir	=	"/data",
	.threshold  =	5,
	.dispatch_mode        = HASH,
	.max_entries_per_file = 80000,

};

const char *classify_home = "/data";

vlib_threadvar_ctx_t vlib_threadvar_main[MAX_LQ_NUM];

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
	vlib_threadvar_ctx_t *vtc = &vlib_threadvar_main[lq_id % vm->nr_threads];
	(*lq) = vtc->lq;
}

static void load_dictionary(vlib_main_t *vm)
{
#define mme_dict_length	256
	/* load dictionary */
	FILE *fp;
	const char *file = "src/cfg/dictionary";
	char line[mme_dict_length] = {0};
	char *p;
	int sep_refcnt = 0;
	char sep = ',';
	size_t step;
	vlib_mmekey_t	*mmekey;
	vlib_mme_t		*mme;
	void *s;
	char ip[32] = {0};
	char name[32] = {0};
	size_t nlen, iplen;
	int i;
	
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
							mme->lq_id = (vm->nr_mmes % vm->nr_threads);
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
		default_mme->lq_id = (vm->nr_mmes % vm->nr_threads);
		vm->nr_mmes ++;
	}

	for (i = 0; i < vm->nr_mmes; i ++) {
		mme = &nr_global_mmes[i];
		fprintf(stdout, "MME (%s, %p):\n", mme->name, mme);
		fprintf(stdout, "\tbind to thread %lu\n", mme->lq_id);
		fprintf(stdout, "\t(", mme->nr_ip);
		int j = 0;
		for (j = 0; j < mme->nr_ip; j ++)
			fprintf(stdout, "\"%s\"", mme->ip_str[j]);
		fprintf(stdout, ")\n");
	}

	const char *pdir = vm->classdir;
	const char *sdir = vm->savdir;
		
	mkdir(pdir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	mkdir(sdir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

	/* Prepare Classify CSV files for each MME. */
	for (i = 0; i < vm->nr_mmes; i ++) {	
		mme = &nr_global_mmes[i];

		MME_LOCK(mme);
		sprintf (mme->path, "%s/%s", pdir, mme->name);
		sprintf (mme->pathr, "%s", sdir);
		fprintf (stdout, "mkdir %s \n", mme->path);
		mkdir(mme->path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
		
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
	vlib_threadvar_ctx_t *vtc;
	vlib_main_t *vm = &vlib_main;

	for (i = 0; i < vm->nr_threads; i ++) {
		vtc = &vlib_threadvar_main[i];
		lq = vtc->lq;
		fprintf (stdout, "LQ[%d], nr_eq_refcnt %ld, nr_dq_refcnt %ld, buffered %lu(%d)\n",
			i, lq->nr_eq_refcnt, lq->nr_dq_refcnt, (lq->nr_eq_refcnt - lq->nr_dq_refcnt), lq->len);
		oryx_lq_destroy(lq);
	}
}

void classify_runtime(void)
{
	int i;
	static uint64_t duration = 0;
	static size_t lqe_size = lq_element_size;
	static struct timeval start, end;
	static uint64_t nr_eq_swap_bytes_prev[MAX_LQ_NUM] = {0}, nr_eq_swap_elements_prev[MAX_LQ_NUM] = {0};
	static uint64_t nr_dq_swap_bytes_prev[MAX_LQ_NUM] = {0}, nr_dq_swap_elements_prev[MAX_LQ_NUM] = {0};
	uint64_t	nr_eq_swap_bytes_cur[MAX_LQ_NUM] = {0}, nr_eq_swap_elements_cur[MAX_LQ_NUM] = {0};
	uint64_t	nr_dq_swap_bytes_cur[MAX_LQ_NUM] = {0}, nr_dq_swap_elements_cur[MAX_LQ_NUM] = {0};	
	uint64_t	nr_cost_us = 0;
	uint64_t	eq[MAX_LQ_NUM] = {0}, dq[MAX_LQ_NUM] = {0};
	uint64_t	nr_eq_total_refcnt = 0, nr_dq_total_refcnt = 0;
	uint64_t	nr_classified_refcnt = 0, nr_classified_refcnt_r = 0, nr_rx_entries = 0;
	uint64_t	nr_missed_refcnt = 0, nr_error_refcnt = 0;
	uint64_t	nr_unclassified_refcnt = 0;
	struct oryx_fmt_buff_t fb = FMT_BUFF_INITIALIZATION;

	char fmtstring[128] = {0};
	char pps_str[20], bps_str[20];
	struct oryx_lq_ctx_t *lq;
	vlib_threadvar_ctx_t *vtc;
	vlib_mme_t *mme;
	vlib_main_t *vm = &vlib_main;
	static oryx_file_t *fp;
	const char *lq_runtime_file = "/var/run/classify_runtime_summary.txt";
	char cat_null[128] = "cat /dev/null > ";

	strcat(cat_null, lq_runtime_file);
	system(cat_null);

	if(!fp) {
		fp = fopen(lq_runtime_file, "a+");
		if(!fp) fp = stdout;
	}

	gettimeofday(&end, NULL);
	/* cost before last time. */
	nr_cost_us = tm_elapsed_us(&start, &end);
	gettimeofday(&start, NULL);

	duration += (nr_cost_us / 1000000);

	/** Statistics for each QUEUE */
	for (i = 0; i < vm->nr_threads; i ++) {
		vtc = &vlib_threadvar_main[i];
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
			oryx_fmt_speed(fmt_pps(nr_cost_us, nr_eq_swap_elements_cur[i]), pps_str, 0, 0),
			oryx_fmt_speed(fmt_bps(nr_cost_us, nr_eq_swap_bytes_cur[i]), bps_str, 0, 0));
		fprintf (fp, "\t\t dequeue %.2f%% %lu (pps %s, bps %s)\n",
			ratio_of(dq[i], nr_eq_total_refcnt),
			dq[i],
			oryx_fmt_speed(fmt_pps(nr_cost_us, nr_dq_swap_elements_cur[i]), pps_str, 0, 0),
			oryx_fmt_speed(fmt_bps(nr_cost_us, nr_dq_swap_bytes_cur[i]), bps_str, 0, 0));
	}
	fflush(fp);
	

	fprintf (fp, "\n\n");
	fprintf (fp, "Statistics for %d MME(s)\n", vm->nr_mmes);
	fprintf (fp, "%24s%64s%32s%32s\n", " ", "Rx(Rx%Rx_TOTAL), -IMSI(-IMSI%Rx)", "+WR(+WR%Rx)", "+W(+W%Rx) Delta");
	fprintf (fp, "%24s%64s%32s%32s\n", " ", "---------------------------------", "-----------", "--------------");
	fflush(fp);

	/** Statistics for each MME */
	for (i = 0; i < vm->nr_mmes; i ++) {
		oryx_format_reset(&fb);
		mme = &nr_global_mmes[i];

		nr_missed_refcnt += mme->nr_refcnt_miss;
		nr_error_refcnt  += mme->nr_refcnt_error;
		
		/* format MME name */	
		oryx_format(&fb, "%24s", mme->name);

		/* format MME refcnt */
		sprintf (fmtstring, "%lu (%.2f%%), %lu (%.2f%%)",
				mme->nr_rx_entries, ratio_of(mme->nr_rx_entries, vm->nr_rx_entries),
				mme->nr_rx_entries_noimsi, ratio_of(mme->nr_rx_entries_noimsi, mme->nr_rx_entries));
		oryx_format(&fb, "%64s", fmtstring);

		sprintf (fmtstring, "%lu (%.2f%%)",
				mme->nr_refcnt_r, ratio_of(mme->nr_refcnt_r, mme->nr_rx_entries));
		oryx_format(&fb, "%32s", fmtstring);

		int delta = ((mme->nr_rx_entries - mme->nr_rx_entries_noimsi) - mme->nr_refcnt);
		sprintf (fmtstring, "%lu (%.2f%%) %u",
				mme->nr_refcnt, ratio_of(mme->nr_refcnt, mme->nr_rx_entries), delta);
		oryx_format(&fb, "%32s", fmtstring);

		/* flush to file */
		oryx_format(&fb, "%s", "\n");
		fprintf(fp, FMT_DATA(fb), FMT_DATA_LENGTH(fb));
		fflush(fp);

		nr_rx_entries += mme->nr_rx_entries;
		nr_classified_refcnt += mme->nr_refcnt;
		nr_classified_refcnt_r += mme->nr_refcnt_r;
	}
	oryx_format_free(&fb);
	fprintf (fp, "\n\n");
	fflush(fp);
	
	/* Overall classified ratio. */
	fprintf (fp, "Statistics Summary\n");
	fprintf (fp, "\trx_files %lu, +classified_files %lu\n",
		vm->nr_rx_files, nr_classified_files);
	fprintf (fp, "\tRx %lu, +dispatched %lu (%.2f%%), -imsi %lu (%.2f%%)\n",
			vm->nr_rx_entries,
			vm->nr_rx_entries_dispatched, ratio_of(vm->nr_rx_entries_dispatched, vm->nr_rx_entries),
			vm->nr_rx_entries_without_imsi, ratio_of(vm->nr_rx_entries_without_imsi, vm->nr_rx_entries));
	fprintf (fp, "\t+WR %lu (%.2f%%), +W %lu (%.2f%%), -classified: %lu (%.2f%%)\n",
			nr_classified_refcnt_r, ratio_of(nr_classified_refcnt_r, vm->nr_rx_entries),
			nr_classified_refcnt,	ratio_of(nr_classified_refcnt, vm->nr_rx_entries),
			nr_unclassified_refcnt, ratio_of(nr_unclassified_refcnt, vm->nr_rx_entries));
	fprintf (fp, "\tDebug\n");
	fprintf (fp, "\t\t%lu opened handler(s), fopen times (f %lu, fr %lu, fe %lu)\n",
		nr_handlers, nr_fopen_times, nr_fopen_times_r, nr_fopen_times_error);
	fprintf (fp, "\t\t-dispatched %lu, +w miss %lu, +w error %lu\n",
		vm->nr_rx_entries_undispatched, nr_missed_refcnt, nr_error_refcnt);
	fprintf (fp, "\t\teq_ticks %lu, dq_ticks %lu\n", vm->nr_thread_eq_ticks, vm->nr_thread_dq_ticks);
	fprintf (fp, "\t\tinotify (%lu/%lu) %s\n", lq_nr_dq(fmgr_q), lq_nr_eq(fmgr_q), inotify_file);
	fprintf (fp, "\n");
	fflush(fp);
	
}

static __oryx_always_inline__
void do_dispatch(const char *value, size_t vlen)
{
	char *p;
	int sep_refcnt = 0;
	const char sep = ',';
	size_t step;
	time_t tv_usec;
	struct lq_element_t *lqe;
	struct oryx_lq_ctx_t *lq;
	char time[32] = {0};
	bool keep_cpy = 1, event_start_time_cpy = 0, find_imsi = 0;
	int tlen = 0;
	vlib_main_t *vm = &vlib_main;

	vm->nr_rx_entries ++;
	
	lqe = lqe_alloc();
	if (!lqe) {
		vm->nr_rx_entries_undispatched ++;
		return;
	}

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
	free(lqe);
	return;

dispatch:

	sscanf(time, "%lu", &tv_usec);	

	lqe->value[lqe->valen - 1] = '\n';
	lqe->value[lqe->valen] = '\0';	
	/* soft copy. */
	lqe->ul_flags	= find_imsi ? LQE_HAVE_IMSI : 0;
	lqe->rawlen		= vlen;
	calc_tm_grid(&lqe->vtg, vm->threshold, tv_usec/1000);
	fmt_mme_ip(lqe->mme_ip, p);
	lqe->mme = mme_find_ip_h(vm->mme_htable, lqe->mme_ip, strlen(lqe->mme_ip));

	fetch_lq(lqe->mme->lq_id, &lq);
	oryx_lq_enqueue(lq, lqe);
	vm->nr_rx_entries_dispatched ++;
}

static __oryx_always_inline__
void do_classify(vlib_threadvar_ctx_t *vtc, struct lq_element_t *lqe)
{
	/* 45 is the number of ',' in a CDR. */
	if (lqe->valen <= 45) {
		fprintf (stdout, "Invalid CDR with length %lu from \"%s\" \n", lqe->valen, (const char *)&lqe->mme_ip[0]);
		vtc->nr_unclassified_refcnt ++;
		goto finish;
	}

	write_lqe(lqe->mme, lqe);
finish:
	free((const void *)lqe);
}

void classify_tmr_handler(struct oryx_timer_t *tmr, int __oryx_unused_param__ argc, 
                char __oryx_unused_param__**argv)
{
	int i;
	vlib_mme_t *mme;
	vlib_file_t *f, *fr;
	vlib_main_t *vm = &vlib_main;
	static uint64_t nr_sec;

	if ((nr_sec ++ % 5) == 0) {
		for (i = 0; i < vm->nr_mmes; i ++) {
			mme = &nr_global_mmes[i];

			MME_LOCK(mme);

			f  = &mme->file;
			fr = &mme->filer;
			if (f->ul_flags & VLIB_MME_VALID)
				fflush (f->fp);
			if (fr->ul_flags & VLIB_MME_VALID)
				fflush (fr->fp);
			
			MME_UNLOCK(mme);
		}
	}

	classify_runtime();
}

static
void * dequeue_handler (void __oryx_unused_param__ *r)
{
		struct lq_element_t *lqe;
		vlib_threadvar_ctx_t *vtc = (vlib_threadvar_ctx_t *)r;
		struct oryx_lq_ctx_t *lq = vtc->lq;
		vlib_main_t *vm = &vlib_main;
		
		/* wait for enqueue handler start. */
		while (vm->ul_flags & VLIB_ENQUEUE_HANDLER_STARTED)
			break;

		fprintf(stdout, "Starting dequeue handler(%lu): vtc=%p, lq=%p\n", pthread_self(), vtc, lq);		
		FOREVER {
			lqe = NULL;
			
			if (!running) {
				/* wait for enqueue handler exit. */
				while (vm->ul_flags & VLIB_ENQUEUE_HANDLER_EXITED)
					break;
				fprintf (stdout, "dequeue exiting ... ");
				/* drunk out all elements before thread quit */					
				while (NULL != (lqe = oryx_lq_dequeue(lq))){
					do_classify(vtc, lqe);
				}
				fprintf (stdout, " exited!\n");
				break;
			}
			
			lqe = oryx_lq_dequeue(lq);
			if (lqe != NULL) {
				do_classify(vtc, lqe);
			}
			vm->nr_thread_dq_ticks ++;
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

void classify_env_init(vlib_main_t *vm)
{
	int i;
	uint32_t	lq_cfg = 0;
	vlib_threadvar_ctx_t *vtc;

	epoch_time_sec = time(NULL);
	inotify_dir = vm->inotifydir;
	
	vm->mme_htable = oryx_htable_init(DEFAULT_HASH_CHAIN_SIZE, 
								mmekey_hval, mmekey_cmp, mmekey_free, 0);	

	for (i = 0; i < vm->nr_threads; i ++) {
		vtc = &vlib_threadvar_main[i];
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

	oryx_lq_new("A FMGR queue", 0, (void **)&fmgr_q);
	oryx_lq_dump(fmgr_q);

	oryx_task_registry(&inotify);
	
	load_dictionary(vm);
	
	struct oryx_timer_t *tmr = oryx_tmr_create (1, "Classify Runtime TMR", TMR_OPTIONS_PERIODIC | TMR_OPTIONS_ADVANCED,
											  classify_tmr_handler, 0, NULL, 1000);
	oryx_tmr_start(tmr);

}

static __oryx_always_inline__
void classify_one_file(const char *oldpath)
{
	FILE				*fp = NULL;
	uint64_t			nr_local_lines = 0;
	size_t				llen = 0;
	vlib_main_t			*vm = &vlib_main;
	char				line[lqe_valen] = {0},
						pps_str[20];
	struct timeval		start,
						end;

	gettimeofday(&start, NULL);
	
	memset(inotify_file, 0, BUFSIZ);
	sprintf(inotify_file, "%s", oldpath);
	
	if (!fp) {
		fp = fopen(oldpath, "r");
		if(!fp) {
			fprintf (stdout, "Cannot open %s \n", oldpath);
			return;
		}
	}
	vm->nr_rx_files ++;

	fprintf (stdout, "reading %s\n", oldpath);
	
	/* skip first lines, this line is a CSV header. */
	fgets(line, lqe_valen, fp);
	memset(line, 0, lqe_valen);

	/* A loop read the part of lines. */
	while (fgets (line, lqe_valen, fp)) {
		nr_local_lines ++;	
		llen = strlen(line);
		//do_dispatch(line, llen);
		do_classify_online(line, llen);
		memset (line, 0, lqe_valen);
	}
	fclose(fp);
	
	gettimeofday(&end, NULL);
	fprintf (stdout, "Finish read %s, %lu/%lu entry(ies) , cost %lu usec, (pps %s)\n",
			oldpath, nr_local_lines, vm->nr_rx_entries, tm_elapsed_us(&start, &end),
			oryx_fmt_speed(fmt_pps(tm_elapsed_us(&start, &end), nr_local_lines), pps_str, 0, 0));

	inotify_remove_file(oldpath);

}

static
void * enqueue_handler (void __oryx_unused_param__ *r)
{
		vlib_main_t *vm = &vlib_main;
		struct fq_element_t *fqe;
		
		vm->ul_flags |= VLIB_ENQUEUE_HANDLER_STARTED;
		FOREVER {
			if (!running) {
				fprintf (stdout, "Thread(%lu): enqueue exited!\n", pthread_self());
				vm->ul_flags |= VLIB_ENQUEUE_HANDLER_EXITED;

				/* drunk out all elements before thread quit */					
				while (NULL != (fqe = oryx_lq_dequeue(fmgr_q))){
					classify_one_file(fqe->name);
					free(fqe);
				}
				fprintf (stdout, " exited!\n");
				break;
			}
			
			fqe = oryx_lq_dequeue(fmgr_q);
			if (fqe != NULL) {
				classify_one_file(fqe->name);
				free(fqe);
			}

			vm->nr_thread_eq_ticks ++;
		}
		oryx_task_deregistry_id(pthread_self());
		return NULL;
}

struct oryx_task_t enqueue = {
		.module 		= THIS,
		.sc_alias		= "Enqueue Task",
		.fn_handler 	= enqueue_handler,
		.lcore_mask		= INVALID_CORE,
		.ul_prio		= KERNEL_SCHED,
		.argc			= 0,
		.argv			= NULL,
		.ul_flags		= 0,	/** Can not be recyclable. */
};

int classify_init (int nr_threads, int nr_threshold,
			const char *dictionary,
			const char *classdir,
			const char *rawdir)

{
	vlib_main_t *vm = &vlib_main;
	oryx_initialize();

	fprintf(stdout, "Trying init classify module\n");
	vm->nr_threads	= nr_threads;
	vm->dictionary	= dictionary;
	vm->classdir	= classdir;
	vm->savdir		= rawdir;
	vm->threshold	= nr_threshold;
	vm->dispatch_mode		 = HASH;
	vm->max_entries_per_file = 100000;
	fprintf(stdout, "	nr_threads %d, nr_threshold %d, flush_entries %d, mme_dict %s, classdir %s, savdir %s\n",
		vm->nr_threads, vm->threshold, 
		vm->max_entries_per_file, vm->dictionary, vm->classdir, vm->savdir);

	classify_env_init(vm);
	oryx_task_registry(&enqueue);
	oryx_task_launch();
	FOREVER {
		sleep (3);
		if (!running) {
			/* wait for handlers of enqueue and dequeue finish. */
			sleep (3);
			classify_runtime();
			break;
		}
	}
	classify_terminal();
	return 0;
}
