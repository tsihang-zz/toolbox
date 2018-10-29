#include "oryx.h"
#include "config.h"

#define VLIB_MAX_PID_NUM	4

vlib_main_t vlib_main = {
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
	.threshold  =	5,
	.dispatch_mode        = HASH,
	.max_entries_per_file = 80000,
};

pid_t parell_pids[VLIB_MAX_PID_NUM] = {0};

int running = 1;

extern struct oryx_task_t enqueue;

static void sigint_handler(int sig) {
	fprintf (stdout, "signal %d ...\n", sig);
	running = 0;
}

static void waitpid_handler(int num) {   
    int status;   
    pid_t pid = waitpid(-1, &status, WNOHANG);   
    if (WIFEXITED(status)) {   
        printf("The child %d exit with code %d\n", pid, WEXITSTATUS(status));   
    }   
}

#define MME_CSV_FILE \
        "/home/tsihang/vbx_share/class/DataExport.s1mmeSAMPLEMME_1540090678.csv"


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

void create_new_file (const char *file_template)
{
                FILE            *fp0,
                                        *fp1;
                char            line[lqe_valen] = {0},
                                        fn[256] = {0};
                uint64_t        nr_rb = 0,
                                        nr_wb = 0;
                time_t          t = time(NULL);
                static int      times = 0;
                vlib_main_t     *vm = &vlib_main;

                FOREVER {

                        nr_rb = nr_wb = 0;
                        sprintf (fn, "%s/%s%s_%lu.csv",
                                vm->inotifydir, MME_CSV_PREFIX, "SAMPLEMME", t);
                        t += 300;

                        fp0 = fopen(file_template, "r");
                        if(!fp0) {
                                fprintf (stdout, "Cannot open %s \n", file_template);
                                break;
                        }

                        fp1 = fopen(fn, "a+");
                        if(!fp1) {
                                fprintf (stdout, "Cannot open %s \n", fn);
                                break;
                        }

                        fprintf (stdout, "writing %s ......", fn);
                        while (fgets (line, lqe_valen, fp0)) {
								size_t s = strlen(line);
                                nr_rb += s;
								update_event_start_time(line, s);
                                nr_wb += fwrite(line, 1, strlen(line), fp1);
                        }

                        fclose(fp0);
                        fclose(fp1);

                        if (nr_rb == nr_wb) {
                                fprintf (stdout, "done!\n");
                        }
						break;
                }

                return NULL;
}

void classify_tmr_handler(struct oryx_timer_t *tmr, int __oryx_unused_param__ argc, 
                char __oryx_unused_param__**argv)
{
	int i, j;
	vlib_mme_t *mme;
	vlib_file_t *f;
	vlib_main_t *vm = &vlib_main;
	static uint64_t nr_sec;
	const int nr_blocks = (24 * 60) / vm->threshold;
	vlib_tm_grid_t vtg;
	time_t tv_sec = time(NULL);

	calc_tm_grid(&vtg, vm->threshold, tv_sec);

	if ((nr_sec ++ % 5) == 0) {
		for (i = 0; i < vm->nr_mmes; i ++) {
			mme = &nr_global_mmes[i];
			for (j = 0; j < nr_blocks; j ++) {
				f = &mme->farray[j];
				
			}
		}
	}

	classify_runtime();
}

static void shmsync(void)
{
	/* This will synced to do_classify thro shared memory. */
	vlib_main_t *vm = oryx_shm_get(VLIB_MAIN_SHMKEY, sizeof(vlib_main));
	memcpy(vm, &vlib_main, sizeof(vlib_main));
	fprintf(stdout, "%s\n", vm->prgname);
}
static void *classify_start(void *argv)
{
	struct fq_element_t *fqe = (struct fq_element_t *)argv;
	memset(inotify_file, 0, BUFSIZ);
	sprintf(inotify_file, "%s", fqe->name);

	const char *oldpath = fqe->name;
	fprintf(stdout, "Thread(%x) starting classify %s\n", pthread_self(), oldpath);		
	FOREVER {
		classify_offline(oldpath);
		break;
	}

	free(fqe);
	
	return NULL;

}
int main (
        int     __oryx_unused_param__   argc,
        char    __oryx_unused_param__   ** argv
)
{
	vlib_main_t *vm = &vlib_main;
	const char *do_classify_bin = "/usr/local/PreStat/do_classify";
	char do_classify_cmd[256] = " ";
		
	//create_new_file(MME_CSV_FILE);
	//exit(0);

	oryx_initialize();
	oryx_register_sighandler(SIGINT,	sigint_handler);
	oryx_register_sighandler(SIGTERM,	sigint_handler);
	oryx_register_sighandler(SIGTERM,	waitpid_handler);

	vm->argc = argc;
	vm->argv = argv;
	epoch_time_sec = time(NULL);

	if(!strcmp(inotify_home, classify_home)) {
		fprintf(stdout, "inotify home cannot be same with classify home");
		exit(0);
	}

	if(vm->inotifydir /* dir exist ? */) {
		oryx_lq_new("A FMGR queue", 0, (void **)&fmgr_q);
		oryx_lq_dump(fmgr_q);
		oryx_task_registry(&inotify);
	}

	vm->mme_htable = oryx_htable_init(DEFAULT_HASH_CHAIN_SIZE, 
								mmekey_hval, mmekey_cmp, mmekey_free, 0/* HTABLE_SYNCHRONIZED is unused,
																		* because the table is no need to update.*/);
	classify_env_init(vm);

	struct oryx_timer_t *tmr = oryx_tmr_create (1, "Classify Runtime TMR", TMR_OPTIONS_PERIODIC | TMR_OPTIONS_ADVANCED,
											  classify_tmr_handler, 0, NULL, 3000);
	oryx_tmr_start(tmr);
	
	oryx_task_launch();

	struct fq_element_t *fqe = NULL;
	FOREVER {
		if (!running) {
			/* wait for handlers of enqueue and dequeue finish. */
			sleep (3);
			classify_runtime();
			break;
		}
		
		fqe = oryx_lq_dequeue(fmgr_q);
		if (fqe == NULL)
			continue;
		if(!oryx_path_exsit(fqe->name)){
			free(fqe);
			continue;
		}
		
#if 0
		pthread_t pid = pthread_create(&pid, NULL, classify_start, fqe);
		pthread_detach(pid);
#else
		memset(inotify_file, 0, BUFSIZ);
		sprintf(inotify_file, "%s", fqe->name);
		free(fqe);
		classify_offline(inotify_file);
#endif
	}
		
	classify_terminal();

	return 0;
}

