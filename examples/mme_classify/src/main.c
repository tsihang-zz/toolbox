#include "oryx.h"
#include "mme_htable.h"

#define HAVE_CDR
#define MAX_LQ_NUM	4
#define ENQUEUE_LCORE_ID 0
#define LINE_LENGTH	256

static int quit = 0;

typedef struct vlib_mme_classify_main_t {
	struct oryx_lq_ctx_t *lq;
	uint32_t unique_id;
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
	memset(lqe, lq_element_size, 0);	
	
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
void lq_read_csv(void)
{
	static FILE *fp = NULL;
	const char *file = "/home/tsihang/vbx_share/class/DataExport.s1mmeSAMPLEMME_1538102100.csv";
	static char line[LINE_LENGTH] = {0};
	char *p;
	int sep_refcnt = 0;
	char sep = ',';
	size_t line_size = 0, step;
	uint64_t dec = 0;
	uint8_t a,b,c,d;
	
	if(!fp) {
		fp = fopen(file, "r");
		if(!fp) {
            fprintf (stdout, "Cannot open %s \n", file);
            exit(0);
        }
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
				sscanf(p, "%d.%d.%d.%d", &a, &b, &c, &d);
				fprintf (stdout, "%d bytes, mmeip %d.%d.%d.%d\n", line_size, a, b, c, d);
				break;
			}
		}
	}
}

static __oryx_always_inline__
void do_csv_flush(vlib_mme_t *mme, const struct lq_element_t *lqe)
{
	char name[128] = {0};
	
	if (mme->fp == NULL) {
		sprintf (name, "%s/%s/%s", MME_CSV_HOME, mme->name, "DataExport.csv");
		mme->fp = fopen (name, "a+");
		if (mme->fp == NULL) {
			fprintf (stdout, "Cannot fopen file %s\n", name);
			exit (0);
		}
	}

	if (mme->fp) {
		fprintf(mme->fp, "%s", lqe->value);
		fflush(mme->fp);
	}
}

static __oryx_always_inline__
void do_classify(vlib_mme_classify_main_t *vmc, const struct lq_element_t *lqe)
{
	vlib_main_t *vm = &vlib_main;
	void *s;
	vlib_mme_key_t *mmekey;
	vlib_mme_t		*mme;

	/* 45 is the number of ',' in a CDR. */
	if (lqe->valen <= 45)
		goto finish;
	
	s = oryx_htable_lookup(vm->mme_htable, lqe->mme_ip, strlen(lqe->mme_ip));
	if (s) {
		mmekey = (vlib_mme_key_t *) container_of (s, vlib_mme_key_t, ip);
		mme = mmekey->mme;
		mme->nr_refcnt ++;
		//fprintf (stdout, "(2)%s", lqe->value);
		do_csv_flush(mme, lqe);
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
		const char *file = "/home/tsihang/vbx_share/class/DataExport.s1mmeSAMPLEMME_1538102100.csv";
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
		//lq_read_csv();

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

static void mme_print(ht_value_t  v,
				uint32_t __oryx_unused_param__ s,
				void __oryx_unused_param__*opaque,
				int __oryx_unused_param__ opaque_size) {
	vlib_mme_t *mme;
	vlib_mme_key_t *mmekey = (vlib_mme_key_t *)container_of (v, vlib_mme_key_t, ip);
	FILE *fp = (FILE *)opaque;

	if (mmekey) {
		mme = mmekey->mme;
		fprintf (fp, "%16s%16s\n", "MME_NAME: ", mme->name);
		fprintf (fp, "%16s%16lu\n", "REFCNT: ",   mme->nr_refcnt);
	}

	fflush(fp);
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
	fprintf (fp, "\n");
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

#if 0
	oryx_htable_foreach_elem(vm->mme_htable,
								mme_print, fp, sizeof(FILE));
#else
	/** Statistics for each MME */
	fprintf (fp, "\n\n");
	fprintf (fp, "Statistics for each MME\n");
	fprintf (fp, "%32s%24s\n", "MME_NAME", "REFCNT");
	fprintf (fp, "%32s%24s\n", "--------", "------");
	fflush(fp);
	for (i = 0; i < vm->nr_mmes; i ++) {
		mme = &nr_global_mmes[i];
		nr_total_refcnt += mme->nr_refcnt;
		fprintf (fp, "%32s%24lu\n", mme->name, mme->nr_refcnt);
		fflush (fp);
	}
	fprintf (fp, "Total: %lu, Unclassified: %lu\n", nr_total_refcnt, nr_unclassified_refcnt);
	fflush(fp);
#endif
	
}

static vlib_mme_key_t *mmekey_alloc(void)
{
	vlib_mme_key_t *v = malloc(sizeof (vlib_mme_key_t));
	BUG_ON(v == NULL);
	memset (v, 0, sizeof (vlib_mme_key_t));
	return v;
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

/* find and alloc MME */
static vlib_mme_t *mme_find(const char *name, size_t nlen)
{
	int i;
	vlib_mme_t *mme;
	vlib_main_t *vm = &vlib_main;
	size_t s1, s2;
	
	BUG_ON(name == NULL || nlen != strlen(name));
	s1 = nlen;
	for (i = 0, s2 = 0; i < vm->nr_mmes; i ++) {
		mme = &nr_global_mmes[i];
		s2 = strlen(mme->name);
		if (s1 == s2 &&
			!strcmp(name, mme->name)) {
			fprintf (stdout, "mme_find, %s\n", name);
			goto finish;
		}
		
		mme = NULL;
	}

finish:
	return mme;
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

