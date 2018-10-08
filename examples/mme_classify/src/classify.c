#include "oryx.h"
#include "mme.h"
#include "classify.h"

extern struct oryx_task_t dequeue;

#if defined(HAVE_CLASSIFY_DEBUG)
static FILE * fp_unclassified_csv_entries = NULL;
static FILE * fp_raw_csv_entries = NULL;
#endif

vlib_threadvar_ctx_t vlib_threadvar_main[MAX_LQ_NUM];

static __oryx_always_inline__
void fmt_mme_ip(char *out, const char *in)
{
	uint32_t a,b,c,d;

	sscanf(in, "%d.%d.%d.%d", &a, &b, &c, &d);
	sprintf (out, "%d.%d.%d.%d", a, b, c, d);
}

static __oryx_always_inline__
struct oryx_lq_ctx_t * fetch_lq(uint64_t lq_id, struct oryx_lq_ctx_t **lq) {
	vlib_main_t *vm = &vlib_main;
	/* fetch an LQ */
	vlib_threadvar_ctx_t *vtc = &vlib_threadvar_main[lq_id % vm->nr_threads];
	(*lq) = vtc->lq;
}

static __oryx_always_inline__
int is_overtime(time_t now, time_t start)
{
	return (now > (start +  MME_CSV_THRESHOLD * 60)) ? 1 : 0;
}

static __oryx_always_inline__
int is_overdisk(time_t now, time_t start)
{
	return 0;
}

static __oryx_always_inline__
void flush_line(FILE *fp, const char *val)
{
	fprintf(fp, "%s", val);
	fflush(fp);
}

static __oryx_always_inline__
void do_flush(vlib_mme_t *mme, const struct lq_element_t *lqe)
{
	if (!mme->fp) {
		mme->nr_miss ++;
		return;
	}
	
	flush_line(mme->fp, lqe->value);
	mme->nr_refcnt ++;
}

static __oryx_always_inline__
void do_csv_post(vlib_mme_t *mme, int reason)
{
	if(mme->fp == NULL)
		return;
	
	fprintf (stdout, "Finish flush disk for file \"%s\" reason %s\n",
			mme->fp_name, reason == OVERTIME ? "overtime" : "overdisk");
	/* flush file before close it. */
	fflush(mme->fp);
	/* overtime or overdisk, close this file. */
	fclose(mme->fp);
	/* reset csv file handler. */
	mme->fp = NULL;
}

static __oryx_always_inline__
int do_csv_open(vlib_mme_t *mme, time_t start)
{
	vlib_main_t *vm = &vlib_main;
	
	memset (mme->fp_name, 0, 128);
	sprintf (mme->fp_name, "%s/%s/DataExport.s1mme%s_%lu_%lu.csv",
		vm->classify_warehouse, mme->name, mme->name, start, start + (MME_CSV_THRESHOLD * 60));

	/* update local time. */
	mme->local_time = start;
	mme->fp = fopen (mme->fp_name, "a+");
	if (mme->fp == NULL) {
		fprintf (stdout, "Cannot fopen file %s\n", mme->fp_name);
	} else {
		flush_line(mme->fp, MME_CSV_HEADER);
	}
	
	return mme->fp ? 1 : 0;
}

static __oryx_always_inline__
void do_csv_flush(vlib_mme_t *mme, const struct lq_element_t *lqe)
{
	/* lock MME */
	MME_LOCK(mme);

	/* To make sure that there is a valid file handler
	 * before writing CSV to disk. */
	if (mme->fp == NULL)
		do_csv_open(mme, time(NULL));

	/* write CDR to disk ASAP */
	do_flush (mme, lqe);

	/* unlock MME */
	MME_UNLOCK(mme);
}

static __oryx_always_inline__
void do_csv_flush0(vlib_mme_t *mme, const struct lq_element_t *lqe)
{
	time_t ts = time(NULL);

	/* lock MME */
	MME_LOCK(mme);

	/* Prepare a new CSV file. */	
	if (mme->fp == NULL &&
		!do_csv_open(mme->name, ts)) {
		goto finish;
	}

	/* write CDR to disk ASAP */
	do_flush (mme, lqe);
	
	if (is_overtime(ts, mme->local_time))
		do_csv_post(mme, OVERTIME);
	
	if (is_overdisk(0, 100))
		do_csv_post(mme, OVERDISK);
	
finish:
	MME_UNLOCK(mme);
}

static void load_dictionary(vlib_main_t *vm)
{
	/* load dictionary */
	FILE *fp;
	const char *file = "src/cfg/dictionary";
	char line[256] = {0};
	char *p;
	int sep_refcnt = 0;
	char sep = ',';
	size_t line_size = 0, step;
	uint64_t dec = 0;
	vlib_mme_key_t	*mmekey;
	vlib_mme_t		*mme;
	void *s;
	char ip[32] = {0};
	char name[32] = {0};
	size_t nlen, iplen;
	int i;
	
	/* Overwrite MME dictionary. */
	if (vm->mme_dictionary)
		file = vm->mme_dictionary;
	
	fp = fopen(file, "r");
	if(!fp) {
		fprintf (stdout, "Cannot open %s \n", file);
		exit(0);
	}

	while (fgets (line, LINE_LENGTH, fp)) {

		line_size = strlen(line);		
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
					mmekey = (vlib_mme_key_t *) container_of (s, vlib_mme_key_t, ip);
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
						if(mme)
							vm->nr_mmes ++;
						
					}
					/* alloc MMEKEY */
					mmekey = mmekey_alloc();
					if(!mmekey)
						break;

					mmekey->mme = mme;
					memcpy(&mmekey->ip[0], &ip[0], iplen);
					
					/* add mmekey dictionary to hash table.
					 * Actually, there are more than 1 IP for one MME.
					 * So, use IP of which MME as index to find unique MME> */
					BUG_ON(oryx_htable_add(vm->mme_htable, mmekey->ip, strlen(mmekey->ip)) != 0);
					fprintf (stdout, "%32s\"%16s\"%32s(%p)\n", "HTABLE ADD MME:", mme->name, mmekey->ip, mme);
				}
				break;
			}
		}
		memset (line, 0, 256);
	}
	
	fclose (fp);

	const char *pdir = vm->classify_warehouse;
	char subdir[256] = {0};
	
	mkdir(pdir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

	/* Prepare Classify CSV files for each MME. */
	time_t ts = time(NULL);
	for (i = 0; i < vm->nr_mmes; i ++) {	
		memset((void *)&subdir[0], 0, 256);
		mme = &nr_global_mmes[i];
		MME_LOCK(mme);
		sprintf (subdir, "%s/%s", pdir, mme->name);
		fprintf (stdout, "mkdir %s (%s, %s)\n", subdir, pdir, mme->name);
		mkdir(subdir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
		/* Prepare a new CSV file. */	
		BUG_ON(mme->fp != NULL);
		do_csv_open(mme->name, ts);
		MME_UNLOCK(mme);
	}

#if defined(HAVE_CLASSIFY_DEBUG)
	sprintf (subdir, "%s/DataExport.unclassified", pdir);
	if (!fp_unclassified_csv_entries) {
		fp_unclassified_csv_entries = fopen(subdir, "a+");
		if (!fp_unclassified_csv_entries)
			exit(0);
	}
	
	sprintf (subdir, "%s/DataExport.raw", pdir);
	if (!fp_raw_csv_entries) {
		fp_raw_csv_entries = fopen(subdir, "a+");
		if (!fp_raw_csv_entries)
			exit(0);
	}
#endif

	s = oryx_htable_lookup(vm->mme_htable, "10.110.16.25", strlen("10.110.16.25"));
	if (s) {
		mmekey = (vlib_mme_key_t *) container_of (s, vlib_mme_key_t, ip);
		mme = mmekey->mme;
		fprintf (stdout, "find mmekey %s by %s\n", mme->name, mmekey->ip);
	}
	
	s = oryx_htable_lookup(vm->mme_htable, "10.110.18.153", strlen("10.110.18.153"));
	if (s) {
		mmekey = (vlib_mme_key_t *) container_of (s, vlib_mme_key_t, ip);
		mme = mmekey->mme;
		fprintf (stdout, "find mmekey %s by %s\n", mme->name, mmekey->ip);
	}
	
	s = oryx_htable_lookup(vm->mme_htable, "10.110.18.152", strlen("10.110.18.152"));
	if (s) {
		mmekey = (vlib_mme_key_t *) container_of (s, vlib_mme_key_t, ip);
		mme = mmekey->mme;
		fprintf (stdout, "find mmekey %s by %s\n", mme->name, mmekey->ip);
	}

}

static
void * csv_file_prepare_handler (void __oryx_unused_param__ *r)
{
		int i;
		char file[128] = {0};
		vlib_main_t *vm = &vlib_main;
		vlib_mme_t *mme;
		int try_new_file = 0;
	
		FOREVER {
			if (!running) {
				fprintf (stdout, "Thread(%p): csv file prepare handler exited!\n", pthread_self());
				break;
			}
			
			time_t ts = time(NULL);
			
			for (i = 0; i < vm->nr_mmes; i ++) {
				try_new_file = 0;
				mme = &nr_global_mmes[i];

				MME_LOCK(mme);
				if (is_overtime(ts, mme->local_time)) {
					do_csv_post(mme, OVERTIME);
					try_new_file = 1;
				}
				if (is_overdisk(0, ts)) {
					do_csv_post(mme, OVERDISK);
					try_new_file = 1;
				}
		
				/* Prepare a new CSV file. */	
				if (try_new_file) {
					/* To make sure that the file has been closed successfully. */
					BUG_ON(mme->fp != NULL);
					do_csv_open(mme->name, ts);
				}

				MME_UNLOCK(mme);
			}

			sleep (30);
		}

		oryx_task_deregistry_id(pthread_self());
		return NULL;
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
	static size_t lqe_size = lq_element_size;
	static struct timeval start, end;
	static uint64_t nr_eq_swap_bytes_prev[MAX_LQ_NUM] = {0}, nr_eq_swap_elements_prev[MAX_LQ_NUM] = {0};
	static uint64_t nr_dq_swap_bytes_prev[MAX_LQ_NUM] = {0}, nr_dq_swap_elements_prev[MAX_LQ_NUM] = {0};
	uint64_t	nr_eq_swap_bytes_cur[MAX_LQ_NUM] = {0}, nr_eq_swap_elements_cur[MAX_LQ_NUM] = {0};
	uint64_t	nr_dq_swap_bytes_cur[MAX_LQ_NUM] = {0}, nr_dq_swap_elements_cur[MAX_LQ_NUM] = {0};	
	uint64_t	nr_cost_us;
	uint64_t	eq[MAX_LQ_NUM] = {0}, dq[MAX_LQ_NUM] = {0};
	uint64_t	nr_eq_total_refcnt = 0, nr_dq_total_refcnt = 0;
	uint64_t	nr_classified_refcnt = 0;
	uint64_t	nr_unclassified_refcnt = 0;
	uint64_t	nr_mme_refcnt[MAX_MME_NUM] = {0};
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

	for (i = 0; i < vm->nr_threads; i ++) {
		vtc = &vlib_threadvar_main[i];
		lq = vtc->lq;

		nr_unclassified_refcnt += vtc->nr_unclassified_refcnt;

		dq[i] = lq->nr_dq_refcnt;
		eq[i] = lq->nr_eq_refcnt;

		nr_eq_total_refcnt += eq[i];
		nr_dq_total_refcnt += dq[i];
		
		nr_eq_swap_bytes_cur[i]		=	(eq[i] * lqe_size) - nr_eq_swap_bytes_prev[i];
		nr_eq_swap_elements_cur[i] 	=	eq[i] - nr_eq_swap_elements_prev[i];
		
		nr_dq_swap_bytes_cur[i]		=	(dq[i] * lqe_size) - nr_dq_swap_bytes_prev[i];
		nr_dq_swap_elements_cur[i] 	=	dq[i] - nr_dq_swap_elements_prev[i];
		
		nr_eq_swap_bytes_prev[i]	=	(eq[i] * lqe_size);
		nr_eq_swap_elements_prev[i]	=	eq[i];
		
		nr_dq_swap_bytes_prev[i]	=	(dq[i] * lqe_size);
		nr_dq_swap_elements_prev[i]	=	dq[i];
	}

	fprintf (fp, "Cost %lu us \n", nr_cost_us);
	fprintf (fp, "\n");
	fprintf (fp, "Statistics for %d QUEUE(s)\n", vm->nr_threads);
	fflush(fp);

	for (i = 0; i < vm->nr_threads; i ++) {
		fprintf (fp, "\tLQ[%d], blocked %lu element(s)\n", i, eq[i] - dq[i]);
		fprintf (fp, "\t\t enqueue %.2f%% %lu (pps %s, bps %s)\n",
			ratio_of(eq[i], nr_eq_total_refcnt),
			eq[i], oryx_fmt_speed(fmt_pps(nr_cost_us, nr_eq_swap_elements_cur[i]), pps_str, 0, 0), oryx_fmt_speed(fmt_bps(nr_cost_us, nr_eq_swap_bytes_cur[i]), bps_str, 0, 0));
		fprintf (fp, "\t\t dequeue %.2f%% %lu (pps %s, bps %s)\n",
			ratio_of(dq[i], nr_eq_total_refcnt),
			dq[i], oryx_fmt_speed(fmt_pps(nr_cost_us, nr_dq_swap_elements_cur[i]), pps_str, 0, 0), oryx_fmt_speed(fmt_bps(nr_cost_us, nr_dq_swap_bytes_cur[i]), bps_str, 0, 0));
	}
	fflush(fp);
	

	/** Statistics for each MME */
	for (i = 0; i < vm->nr_mmes; i ++) {
		mme = &nr_global_mmes[i];
		nr_classified_refcnt += mme->nr_refcnt;
	}
	fprintf (fp, "\n\n");
	fprintf (fp, "Statistics for %d MME(s)\n", vm->nr_mmes);

	fprintf (fp, "%32s%24s%24s\n", " ", "REFCNT", "MISS");
	fprintf (fp, "%32s%24s%24s\n", " ", "------", "----");
	fflush(fp);

	for (i = 0; i < vm->nr_mmes; i ++) {
		oryx_format_reset(&fb);
		mme = &nr_global_mmes[i];

		/* format MME name */	
		oryx_format(&fb, "%32s", mme->name);

		/* format MME refcnt */
		sprintf (fmtstring, "%lu(%.2f%%)",
				mme->nr_refcnt, ratio_of(mme->nr_refcnt, nr_eq_total_refcnt));
		oryx_format(&fb, "%24s", fmtstring);

		/* format MME miss refcnt */
		sprintf (fmtstring, "%lu(%.2f%%)",
				mme->nr_miss, ratio_of(mme->nr_miss, nr_eq_total_refcnt));
		oryx_format(&fb, "%24s", fmtstring);

		/* flush to file */
		oryx_format(&fb, "%s", "\n");
		flush_line(fp, FMT_DATA(fb));

	}

	/* Overall classified ratio. */
	fprintf (fp, "\nClassified: %lu (%.2f%%), Unclassified: %lu (%.2f%%)\n",
			nr_classified_refcnt, ratio_of(nr_classified_refcnt, nr_eq_total_refcnt),
			nr_unclassified_refcnt, ratio_of(nr_unclassified_refcnt, nr_eq_total_refcnt));
	fflush(fp);

	oryx_format_free(&fb);
}

void do_dispatch(const char *value, size_t vlen)
{
	char *p;
	int sep_refcnt = 0;
	char sep = ',';
	size_t llen = 0, step;

	static uint32_t lq_id = 0;
	struct lq_element_t *lqe;
	struct oryx_lq_ctx_t *lq;

#if defined(HAVE_CLASSIFY_DEBUG)
	flush_line(fp_raw_csv_entries, value);
#endif

	for (p = &value[0], step = 0, sep_refcnt = 0;
				*p != '\0' && *p != '\n'; ++ p, step ++) {
		if (*p == sep)
			sep_refcnt ++;
		if (sep_refcnt == 45) {
			/* skip the last sep ',' */
			++ p;
			/** fprintf (stdout, "vm->nr_threads %d, ip %s ===> %s",
						vm->nr_threads, p, line); */
			goto dispatch;
		}
	}
	return;

dispatch:
	lqe = lqe_alloc();
	if (!lqe)
		return;
	
	/* soft copy. */
	memcpy((void *)&lqe->value[0], value, vlen);
	lqe->valen = vlen;
	fmt_mme_ip(lqe->mme_ip, p);
	lq_id = oryx_js_hash(lqe->mme_ip, strlen(lqe->mme_ip));
	/* When round robbin used, CSV file must be locked while writing. */
	//lq_id ++;
	fetch_lq(lq_id, &lq);
	oryx_lq_enqueue(lq, lqe);
}

void do_classify(vlib_threadvar_ctx_t *vtc, const struct lq_element_t *lqe)
{
	vlib_main_t *vm = &vlib_main;
	void *s;
	vlib_mme_key_t *mmekey;
	vlib_mme_t		*mme;

	/* 45 is the number of ',' in a CDR. */
	if (lqe->valen <= 45) {
		fprintf (stdout, "Invalid CDR with length %d from \"%s\" \n", lqe->valen, lqe->mme_ip);
#if defined(HAVE_CLASSIFY_DEBUG)
		/* debug ONLY */
		flush_line(fp_unclassified_csv_entries, lqe->value);
#endif
		vtc->nr_unclassified_refcnt ++;
		goto finish;
	}
	
	s = oryx_htable_lookup(vm->mme_htable, lqe->mme_ip, strlen(lqe->mme_ip));
	if (s) {
		mmekey = (vlib_mme_key_t *) container_of (s, vlib_mme_key_t, ip);
		mme = mmekey->mme;
		do_csv_flush(mme, lqe);
	} else {
		fprintf (stdout, "Cannot find a defined mmekey via IP \"%s\"\n", lqe->mme_ip);
		vtc->nr_unclassified_refcnt ++;
#if defined(HAVE_CLASSIFY_DEBUG)
		/* debug ONLY */
		flush_line(fp_unclassified_csv_entries, lqe->value);
#endif
	}

finish:
	free(lqe);
}

void classify_tmr_handler(struct oryx_timer_t *tmr, int __oryx_unused_param__ argc, 
                char __oryx_unused_param__**argv)
{
	fprintf (stdout, "%s running ...\n", tmr->sc_alias);
	classify_runtime();
}

struct oryx_task_t prepare = {
		.module 		= THIS,
		.sc_alias		= "CSV File Prepare Task",
		.fn_handler 	= csv_file_prepare_handler,
		.lcore_mask 	= INVALID_CORE,
		.ul_prio		= KERNEL_SCHED,
		.argc			= 0,
		.argv			= NULL,
		.ul_flags		= 0,	/** Can not be recyclable. */
};

void classify_env_init(vlib_main_t *vm)
{
	int i;
	char c;
	uint32_t	lq_cfg = 0;
	vlib_threadvar_ctx_t *vtc;

	epoch_time_sec = time(NULL);
	
	vm->mme_htable = oryx_htable_init(DEFAULT_HASH_CHAIN_SIZE, 
			ht_mme_key_hval, ht_mme_key_cmp, ht_mme_key_free, 0);	

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
		//t->lcore_mask = (1 << (i + ENQUEUE_LCORE_ID));
		t->ul_prio = KERNEL_SCHED;
		t->argc = 1;
		t->argv = vtc;
		t->sc_alias = strdup(name);
		oryx_task_registry(t);
	}

	oryx_task_registry(&prepare);
	
	load_dictionary(vm);

	struct oryx_timer_t *tmr = oryx_tmr_create (1, "Classify Runtime TMR", TMR_OPTIONS_PERIODIC | TMR_OPTIONS_ADVANCED,
											  classify_tmr_handler, 0, NULL, 1000);
	oryx_tmr_start(tmr);

}

