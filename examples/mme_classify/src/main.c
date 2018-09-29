#include "oryx.h"
#include "mme.h"

#define HAVE_CDR
#define MAX_LQ_NUM	4
#define ENQUEUE_LCORE_ID 0
#define LINE_LENGTH	256

static int quit = 0;

typedef struct vlib_mme_classify_main_t {
	struct oryx_lq_ctx_t *lq;
	uint32_t			unique_id;
}vlib_mme_classify_main_t;
vlib_mme_classify_main_t vlib_mme_classify_main[MAX_LQ_NUM];

struct lq_element_t {
	struct lq_prefix_t lp;
	
#if defined(HAVE_CDR)
	/* used to find an unique MME. */
	char mme_ip[32];
	size_t valen;
	char value[LINE_LENGTH];
#else
	char value;
#endif
};
#define lq_element_size	(sizeof(struct lq_element_t))

#define VLIB_ENQUEUE_HANDLER_EXITED		(1 << 0)
#define VLIB_ENQUEUE_HANDLER_STARTED	(1 << 1)
typedef struct vlib_main_t {
	int						argc;
	char					**argv;
	const char				*prgname;			/* Name for e.g. syslog. */
	volatile uint32_t		ul_flags;
	int						nr_mmes;
	uint64_t				nr_unclassified_refcnt[MAX_LQ_NUM];	
	struct oryx_htable_t	*mme_htable;
} vlib_main_t;

vlib_main_t vlib_main = {
	.argc		=	0,
	.argv		=	NULL,
	.prgname	=	"mme_classify",
	.nr_mmes	=	0,
	.mme_htable	=	NULL,
	.nr_unclassified_refcnt	=	{0},
};

static __oryx_always_inline__
void fmt_mme_ip(char *out, const char *in)
{
	uint8_t a,b,c,d;

	sscanf(in, "%d.%d.%d.%d", &a, &b, &c, &d);
	sprintf (out, "%d.%d.%d.%d", a, b, c, d);
}

static __oryx_always_inline__
struct oryx_lq_ctx_t * fetch_lq(uint64_t lq_id, struct oryx_lq_ctx_t **lq) {
	/* fetch an LQ */
	vlib_mme_classify_main_t *vmc = &vlib_mme_classify_main[lq_id % MAX_LQ_NUM];
	(*lq) = vmc->lq;
}

static __oryx_always_inline__
void lq_cdr_equeue(const char *value, size_t size, const char *mme_ip)
{
	uint8_t a,b,c,d;
	static uint32_t lq_id;
	struct lq_element_t *lqe;
	struct oryx_lq_ctx_t *lq;

	lqe = malloc(lq_element_size);
	BUG_ON(lqe == NULL);
	memset(lqe, 0, lq_element_size);
	
#if defined(HAVE_CDR)
	/* soft copy. */
	memcpy((void *)&lqe->value[0], value, size);
	lqe->valen = size;
	fmt_mme_ip(lqe->mme_ip, mme_ip);
#endif

	lq_id = oryx_js_hash(lqe->mme_ip, strlen(lqe->mme_ip));
	/* When round robbin used, CSV file must be locked while writing. */
	//lq_id ++;
	
	fetch_lq(lq_id, &lq);
	oryx_lq_enqueue(lq, lqe);
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
void flush_line(const FILE *fp, const char *val)
{
	fprintf(fp, "%s", val);
	fflush(fp);
}

static __oryx_always_inline__
void do_flush(vlib_mme_t *mme, struct lq_element_t *lqe)
{
	if (!mme->fp) {
		mme->nr_miss ++;
		return;
	}
	
	flush_line(mme->fp, lqe->value);
	mme->nr_refcnt ++;
}

static __oryx_always_inline__
void do_csv_post(vlib_mme_t *mme, time_t new_ts)
{
	/* flush file before close it. */
	fflush(mme->fp);
	/* overtime or overdisk, close this file. */
	fclose(mme->fp);
	/* update local time. */
	mme->local_time = new_ts;
	/* reset csv file handler. */
	mme->fp = NULL;
}

static __oryx_always_inline__
void format_csv_file(char *name, time_t start, char *csv_file)
{
	sprintf (csv_file, "%s/%s/DataExport.s1mme%s_%d_%d.csv",
		MME_CSV_HOME, name, name, start, start + (MME_CSV_THRESHOLD * 60));
}

static __oryx_always_inline__
void do_csv_flush(vlib_mme_classify_main_t *vmc,
			vlib_mme_t *mme, const struct lq_element_t *lqe)
{
	char file[128] = {0};
	time_t ts = time(NULL);

	/* lock MME */
	MME_LOCK(mme);

	/* Prepare a new CSV file. */	
	if (mme->fp == NULL) {
		format_csv_file(mme->name, ts, file);
		mme->fp = fopen (file, "a+");
		if (mme->fp == NULL) {
			mme->nr_miss ++;
			fprintf (stdout, "Cannot fopen file %s\n", file);
			goto finish;
		} else {
			flush_line(mme->fp, MME_CSV_HEADER);
		}
	}

	/* write CDR to disk ASAP */
	do_flush (mme, lqe);
	
	if (is_overtime(ts, mme->local_time))
		do_csv_post(mme, ts);
	
	if (is_overdisk(0, 100))
		do_csv_post(mme, ts);
	
finish:
	MME_UNLOCK(mme);
}

static __oryx_always_inline__
void do_classify(vlib_mme_classify_main_t *vmc, const struct lq_element_t *lqe)
{
	vlib_main_t *vm = &vlib_main;
	void *s;
	vlib_mme_key_t *mmekey;
	vlib_mme_t		*mme;

	/* 45 is the number of ',' in a CDR. */
	if (lqe->valen <= 45) {
		fprintf (stdout, "Invalid CDR with length %d from \"%s\" \n", lqe->valen, lqe->mme_ip);
		goto finish;
	}
	
	s = oryx_htable_lookup(vm->mme_htable, lqe->mme_ip, strlen(lqe->mme_ip));
	if (s) {
		mmekey = (vlib_mme_key_t *) container_of (s, vlib_mme_key_t, ip);
		mme = mmekey->mme;
		do_csv_flush(vmc, mme, lqe);
	} else {
		fprintf (stdout, "Cannot find a defined mmekey via IP \"%s\"\n", lqe->mme_ip);
		vm->nr_unclassified_refcnt[vmc->unique_id] ++;
	}

finish:
	free(lqe);
}

static
void * dequeue_handler (void __oryx_unused_param__ *r)
{
		struct lq_element_t *lqe;
		vlib_mme_classify_main_t *vmc = (vlib_mme_classify_main_t *)r;
		struct oryx_lq_ctx_t *lq = vmc->lq;
		vlib_main_t *vm = &vlib_main;
		
		/* wait for enqueue handler start. */
		while (vm->ul_flags & VLIB_ENQUEUE_HANDLER_STARTED)
			break;

		fprintf(stdout, "Starting dequeue handler(%lu): vmc=%p, lq=%p\n", pthread_self(), vmc, lq);		
		FOREVER {
			lqe = NULL;
			
			if (quit) {
				/* wait for enqueue handler exit. */
				while (vm->ul_flags & VLIB_ENQUEUE_HANDLER_EXITED)
					break;
				fprintf (stdout, "dequeue exiting ... ");
				/* drunk out all elements before thread quit */					
				while (NULL != (lqe = oryx_lq_dequeue(lq))){
					do_classify(vmc, lqe);
				}
				fprintf (stdout, " exited!\n");
				break;
			}
			
			lqe = oryx_lq_dequeue(lq);
			if (lqe != NULL) {
				do_classify(vmc, lqe);
			}
		}

		oryx_task_deregistry_id(pthread_self());
		return NULL;
}

static
void * enqueue_handler (void __oryx_unused_param__ *r)
{
		struct lq_element_t *lqe;
		vlib_main_t *vm = &vlib_main;

		static FILE *fp = NULL;
		const char *file = MME_CSV_FILE;
		static char line[LINE_LENGTH] = {0};
		char *p;
		int sep_refcnt = 0;
		char sep = ',';
		size_t line_size = 0, step;
		uint64_t dec = 0;
		
		if(!fp) {
			fp = fopen(file, "r");
			if(!fp) {
	            fprintf (stdout, "Cannot open %s \n", file);
	            exit(0);
        	}
		}
		
		vm->ul_flags |= VLIB_ENQUEUE_HANDLER_STARTED;
		FOREVER {
			if (quit) {
				fprintf (stdout, "enqueue exited!\n");
				vm->ul_flags |= VLIB_ENQUEUE_HANDLER_EXITED;
				break;
			}
			
			while (fgets (line, LINE_LENGTH, fp)) {
				line_size = strlen(line);
				for (p = &line[0], step = 0, sep_refcnt = 0;
							*p != '\0' && *p != '\n'; ++ p, step ++) {
					if (*p == sep)
						sep_refcnt ++;
					if (sep_refcnt == 45) {
						/* skip the last sep ',' */
						++ p;
						lq_cdr_equeue(line, line_size, p);
                        usleep(10000);
						break;
					}
				}
				memset (line, 0, LINE_LENGTH);
			}
			/* break after end of file. */
			fprintf (stdout, "Finish read %s, break down!\n", file);
			break;
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

static struct oryx_task_t enqueue = {
		.module 		= THIS,
		.sc_alias		= "Enqueue Task",
		.fn_handler 	= enqueue_handler,
		.lcore_mask		= (1 << ENQUEUE_LCORE_ID),
		.ul_prio		= KERNEL_SCHED,
		.argc			= 0,
		.argv			= NULL,
		.ul_flags		= 0,	/** Can not be recyclable. */
};

static void lq_sigint(int sig)
{
	fprintf (stdout, "signal %d ...\n", sig);
	quit = 1;
}

static void lq_terminal(void)
{
	int i;
	struct oryx_lq_ctx_t *lq;
	vlib_mme_classify_main_t *vmc;

	for (i = 0; i < MAX_LQ_NUM; i ++) {
		vmc = &vlib_mme_classify_main[i];
		lq = vmc->lq;
		fprintf (stdout, "LQ[%d], nr_eq_refcnt %ld, nr_dq_refcnt %ld, buffered %lu(%d)\n",
			i, lq->nr_eq_refcnt, lq->nr_dq_refcnt, (lq->nr_eq_refcnt - lq->nr_dq_refcnt), lq->len);
		oryx_lq_destroy(lq);
	}
}

static void lq_runtime(void)
{
	int i;
	static size_t lqe_size = lq_element_size;
	static struct timeval start, end;
	static uint64_t nr_eq_swap_bytes_prev[MAX_LQ_NUM] = {0}, nr_eq_swap_elements_prev[MAX_LQ_NUM] = {0};
	static uint64_t nr_dq_swap_bytes_prev[MAX_LQ_NUM] = {0}, nr_dq_swap_elements_prev[MAX_LQ_NUM] = {0};
	uint64_t	nr_eq_swap_bytes_cur = 0, nr_eq_swap_elements_cur = 0;
	uint64_t	nr_dq_swap_bytes_cur = 0, nr_dq_swap_elements_cur = 0;	
	uint64_t	nr_cost_us;
	uint64_t	eq, dq;
	uint64_t	nr_total_refcnt = 0;
	uint64_t	nr_unclassified_refcnt = 0;
	char pps_str[20], bps_str[20];
	struct oryx_lq_ctx_t *lq;
	vlib_mme_classify_main_t *vmc;
	vlib_mme_t *mme;
	vlib_main_t *vm = &vlib_main;
	static oryx_file_t *fp;
	const char *lq_runtime_file = "./lq_runtime_summary.txt";
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

	fprintf (fp, "Cost %lu us \n", nr_cost_us);
	fprintf (fp, "\n");
	fprintf (fp, "Statistics for each QUEUE\n");
	fflush(fp);
	
	for (i = 0; i < MAX_LQ_NUM; i ++) {
		vmc = &vlib_mme_classify_main[i];
		lq = vmc->lq;

		nr_unclassified_refcnt += vm->nr_unclassified_refcnt[i];
		
		dq = lq->nr_dq_refcnt;
		eq = lq->nr_eq_refcnt;
		
		nr_eq_swap_bytes_cur		=	(eq * lqe_size) - nr_eq_swap_bytes_prev[i];
		nr_eq_swap_elements_cur 	=	eq - nr_eq_swap_elements_prev[i];
		
		nr_dq_swap_bytes_cur		=	(dq * lqe_size) - nr_dq_swap_bytes_prev[i];
		nr_dq_swap_elements_cur 	=	dq - nr_dq_swap_elements_prev[i];
		
		nr_eq_swap_bytes_prev[i]	=	(eq * lqe_size);
		nr_eq_swap_elements_prev[i]	=	eq;
		
		nr_dq_swap_bytes_prev[i]	=	(dq * lqe_size);
		nr_dq_swap_elements_prev[i]	=	dq;

		fprintf (fp, "LQ[%d], eq %ld (pps %s, bps %s), dq %ld (pps %s, bps %s), buffered %lu(%d)\n",
			i,
			eq, oryx_fmt_speed(fmt_pps(nr_cost_us, nr_eq_swap_elements_cur), pps_str, 0, 0), oryx_fmt_speed(fmt_bps(nr_cost_us, nr_eq_swap_bytes_cur), bps_str, 0, 0),
			dq, oryx_fmt_speed(fmt_pps(nr_cost_us, nr_dq_swap_elements_cur), pps_str, 0, 0), oryx_fmt_speed(fmt_bps(nr_cost_us, nr_dq_swap_bytes_cur), bps_str, 0, 0),
			(lq->nr_eq_refcnt - lq->nr_dq_refcnt), lq->len);
		fflush(fp);

	}

	/** Statistics for each MME */
	fprintf (fp, "\n\n");
	fprintf (fp, "Statistics for each MME\n");
	fprintf (fp, "%32s%24s%24s\n", "MME_NAME", "REFCNT", "MISS");
	fprintf (fp, "%32s%24s%24s\n", "--------", "------", "----");
	fflush(fp);
	for (i = 0; i < vm->nr_mmes; i ++) {
		mme = &nr_global_mmes[i];
		nr_total_refcnt += mme->nr_refcnt;
		fprintf (fp, "%32s%24lu%24lu\n", mme->name, mme->nr_refcnt, mme->nr_miss);
		fflush (fp);
	}
	fprintf (fp, "Total: %lu, Unclassified: %lu\n", nr_total_refcnt, nr_unclassified_refcnt);
	fflush(fp);

}

static void mme_warehouse(const char *path)
{
	int i;
	vlib_mme_t *mme;
	vlib_main_t *vm = &vlib_main;
	
	char dir[256] = {0};
	for (i = 0; i < vm->nr_mmes; i ++) {
		mme = &nr_global_mmes[i];
		sprintf (dir, "%s/%s", path, mme->name);
		mkdir(dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	}
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
				memcpy (&name[0], &line[0], (p - line));
				/* skip the last sep ',' */
				++ p;				
				/* Parse MME IP */
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
					/* alloc MMEKEY */
					mmekey = mmekey_alloc();

					/* find and alloc MME */
					mme = mme_find(name, nlen);
					
					/* no such mme, alloc a new one */
					if (mme == NULL) {
						mme = &nr_global_mmes[vm->nr_mmes ++];
						mme->local_time = time(NULL);
						MME_LOCK_INIT(mme);
						memcpy(&mme->name[0], &name[0], nlen);
						mmekey->mme = mme;						
					}
					
					memcpy(&mmekey->ip[0], &ip[0], iplen);
					/* add mmekey dictionary to hash table.
					 * Actually, there are more than 1 IP for one MME.
					 * So, use IP of which MME as index to find unique MME> */
					BUG_ON(oryx_htable_add(vm->mme_htable, mmekey->ip, strlen(mmekey->ip)) != 0);
					fprintf (stdout, "%32s\"%16s\"%32s\n", "HTABLE ADD MME:", mme->name, mmekey->ip);
				}
				break;
			}
		}
		memset (line, 0, 256);
	}

	fclose (fp);

	mme_warehouse("./test");

	s = oryx_htable_lookup(vm->mme_htable, "10.110.16.216", strlen("10.110.16.216"));
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

static void lq_env_init(vlib_main_t *vm)
{
	int i;
	uint32_t	lq_cfg = 0;
	vlib_mme_classify_main_t *vmc;

	epoch_time_sec = time(NULL);
	
	vm->mme_htable = oryx_htable_init(DEFAULT_HASH_CHAIN_SIZE, 
							ht_mme_key_hval, ht_mme_key_cmp, ht_mme_key_free, 0);	

	for (i = 0; i < MAX_LQ_NUM; i ++) {
		vmc = &vlib_mme_classify_main[i];
		vmc->unique_id = i;
		/* new queue */
		oryx_lq_new("A new list queue", lq_cfg, (void **)&vmc->lq);
		oryx_lq_dump(vmc->lq);

		char name[64] = {0};
		sprintf (name, "Dequeue Task%d", i);
		struct oryx_task_t *t = malloc (sizeof(struct oryx_task_t));
		BUG_ON(t == NULL);
		memset(t, sizeof (struct oryx_task_t), 0);
		memcpy(t, &dequeue, sizeof (struct oryx_task_t));
		t->lcore_mask = INVALID_CORE;
		//t->lcore_mask = (1 << (i + ENQUEUE_LCORE_ID));
        t->ul_prio = KERNEL_SCHED;
		t->argc = 1;
		t->argv = vmc;
		t->sc_alias = strdup(name);
		oryx_task_registry(t);
	}

	load_dictionary(vm);
	
}

void tmr_default_handler(struct oryx_timer_t *tmr, int __oryx_unused_param__ argc, 
                char __oryx_unused_param__**argv)
{
	int i;
	vlib_main_t *vm = &vlib_main;
	vlib_mme_t *mme;

	time_t ts = time(NULL);
	
	for (i = 0; i < vm->nr_mmes; i ++) {
		mme = &nr_global_mmes[i];
		MME_LOCK(mme);
		if (ts > (mme->local_time + MME_CSV_THRESHOLD * 60)) {
			//do_csv_post(mme, ts);
		}

		MME_UNLOCK(mme);

	}
	epoch_time_sec = time(NULL);
}

int main (
        int     __oryx_unused_param__   argc,
        char    __oryx_unused_param__   ** argv
)

{
	vlib_main_t *vm = &vlib_main;
	
	oryx_initialize();
	oryx_register_sighandler(SIGINT, lq_sigint);
	oryx_register_sighandler(SIGTERM, lq_sigint);

	lq_env_init(vm);
	oryx_task_registry(&enqueue);


	struct oryx_timer_t *tmr = oryx_tmr_create (1, "CSV Prepare TMR", TMR_OPTIONS_PERIODIC | TMR_OPTIONS_ADVANCED,
											  tmr_default_handler, 0, NULL, 60000);
	oryx_tmr_start(tmr);
	
	oryx_task_launch();
	FOREVER {
		if (quit) {
			/* wait for handlers of enqueue and dequeue finish. */
			sleep (3);
			/* last runtime logging. */
			lq_runtime();
			lq_terminal();
			break;
		} else {
			lq_runtime();
			sleep (3);
		}
	};
	
	return 0;
}

