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

static void sigchld_handler(int num) {   
    int status;   
    pid_t pid = waitpid(-1, &status, 0);   
    if (WIFEXITED(status)) {
		if(WEXITSTATUS(status) != 0)
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

static void create_new_file (const char *file_template)
{
    FILE            *fp0,
                     *fp1;
    char            line[lqe_valen] = {0},
                            fn[256] = {0};
    uint64_t        nr_rb = 0,
                    nr_wb = 0;
    time_t          t = time(NULL);
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

}

#define linesize 256

static int get_prgname(pid_t pid, char *task_name) {
	 char pidpath[1024];
	 char buf[linesize];

	 sprintf(pidpath, "/proc/%d/status", pid);
	 FILE* fp = fopen(pidpath, "r");
	 if(fp == NULL) {
	 	fprintf(stdout, "fopen: %s\n", oryx_safe_strerror(errno));
		return -1;
	 } else {
	     if (fgets(buf, linesize - 1, fp) != NULL)
		     sscanf(buf, "%*s %s", task_name);
	     fclose(fp);
		 return 0;
	 }
}

int main (
        int     __oryx_unused_param__   argc,
        char    __oryx_unused_param__   ** argv
)
{
	vlib_main_t *vm = &vlib_main;
	struct fq_element_t *fqe = NULL;	
	char logzomb[1024] = {0};
	char prgname[32] = {0};
	const char *zombie = "./zombie.txt";

	oryx_initialize();
	oryx_register_sighandler(SIGINT,	sigint_handler);
	oryx_register_sighandler(SIGTERM,	sigint_handler);
	//oryx_register_sighandler(SIGCHLD,	sigchld_handler);
	signal(SIGCHLD,	SIG_IGN);

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
	classify_initialization(vm);
	oryx_task_launch();

	FOREVER {
		sleep (3);
		
		if (!running) {
			break;
		}
		
		if (oryx_path_exsit(zombie))
			remove(zombie);

		get_prgname(getpid(), prgname);
		sprintf(logzomb, "ps -ef | grep %s | grep -v grep | grep defunct | awk '{print$2}' >> %s", prgname, zombie);
		do_system(logzomb);
		fqe = oryx_lq_dequeue(fmgr_q);
		if (fqe == NULL)
			continue;
		if(!oryx_path_exsit(fqe->name)){
			free(fqe);
			continue;
		}

		vlib_fkey_t fkey = {
			.name = " ",
			.ul_flags = 0,
			.nr_size = 0,
			.nr_entries = 0,
		};
		
		pid_t p = fork();
		if (p < 0) {
			fprintf(stdout, "fork: %s\n", oryx_safe_strerror(errno));
			free(fqe);
			continue;
		} else if (p == 0) {
			fprintf(stdout, "Children %d -> %s\n", getpid(), fqe->name);
			classify_offline(fqe->name, &fkey); 			
			fprintf(stdout, "Children %d -> %s, %lu entries, exit\n", getpid(), fqe->name, fkey.nr_entries);
			exit(0);
		}
		
		free(fqe);
	}
		
	classify_terminal();

	return 0;
}

