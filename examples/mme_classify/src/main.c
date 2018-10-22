#include "oryx.h"
#include "main.h"
#include "cfg.h"


#define MME_CSV_FILE \
	"/home/tsihang/vbx_share/class/DataExport.s1mmeSAMPLEMME_1540090678.csv"

int running = 1;

extern struct oryx_task_t enqueue;

static void lq_sigint(int sig)
{
	fprintf (stdout, "signal %d ...\n", sig);
	running = 0;
}

static
void * new_file_handler (void __oryx_unused_param__ *r)
{
		FILE		*fp0,
					*fp1;
		char		line[lqe_valen] = {0},
					fn[256] = {0};
		uint64_t	nr_rb = 0,
					nr_wb = 0;
		time_t		t = time(NULL);
		static int	times = 0;
		vlib_main_t	*vm = &vlib_main;

		FOREVER {
			
			nr_rb = nr_wb = 0;
			sprintf (fn, "%s/%s%s_%lu.csv",
				vm->inotifydir, MME_CSV_PREFIX, "SAMPLEMME", t);
			//t += 300;

			fp0 = fopen(MME_CSV_FILE, "r");
			if(!fp0) {
				fprintf (stdout, "Cannot open %s \n", MME_CSV_FILE);
				break;
			}

			fp1 = fopen(fn, "a+");
			if(!fp1) {
				fprintf (stdout, "Cannot open %s \n", fn);
				break;
			}

			fprintf (stdout, "writing %s ......", fn);
			while (fgets (line, lqe_valen, fp0)) {
				nr_rb += strlen(line);
				nr_wb += fwrite(line, 1, strlen(line), fp1);
			}

			fclose(fp0);
			fclose(fp1);
			
			if (nr_rb == nr_wb) {
				fprintf (stdout, "done!\n");
			}

			if (times ++ >= 10)
			break;
		}
		
		oryx_task_deregistry_id(pthread_self());
		return NULL;
}

struct oryx_task_t newfile = {
		.module 		= THIS,
		.sc_alias		= "New File Task",
		.fn_handler 	= new_file_handler,
		.lcore_mask		= INVALID_CORE,//(1 << ENQUEUE_LCORE_ID),
		.ul_prio		= KERNEL_SCHED,
		.argc			= 0,
		.argv			= NULL,
		.ul_flags		= 0,	/** Can not be recyclable. */
};

static int do_timeout_check(void *argv, char *pathname, char *filename)
{
	int 		err,
				n = 0,
				timeout_sec = *(int *)argv;
	struct stat buf;
	char		*p,
				move[256] = {0},
				newpath[256] = {0},
				mmename[32] = {0};
	time_t		now = time(NULL);	
	vlib_main_t *vm = &vlib_main;

	/* Not a CSV file */
	if(!strstr(filename, ".csv"))
		return 0;

	err = stat(pathname, &buf);
	if(err) {
		fprintf(stdout, "stat %s\n", oryx_safe_strerror(errno));
		return 0;
	}
	/* file without any modifications in 60 seconds will be timeout and removed. */
	if(now < (buf.st_mtime + timeout_sec)) {
		fprintf(stdout, ".");
		return 0;
	}

	/* find MME name by the given string */
	for(p = (filename + strlen(MME_CSV_PREFIX)); *p != '_'; ++ p) {
		mmename[n ++] = *p;
	}

#if 0
	if(!mme_find(mmename, n)) {
		fprintf(stdout, "Cannot find mme named \"%s\"\n", mmename);
		return 0;
	}
#endif
	
	sprintf (newpath, "%s/%s/%s", vm->classdir, mmename, filename);

	sprintf(move, "mv %s %s", pathname, newpath);
	err = do_system(move);
	if(err) {
		fprintf (stdout, "mv %s\n", oryx_safe_strerror(errno));
	} else {
		fprintf (stdout, "\n(*)timeout %s\n", move);
	}

#if defined(HAVE_LOCAL_TEST)
	//remove(newpath);
	//fprintf (stdout, "* remove %s\n", newpath);
#endif

	return 0;
}

int main (
        int     __oryx_unused_param__   argc,
        char    __oryx_unused_param__   ** argv
)

{
	oryx_register_sighandler(SIGINT, lq_sigint);
	oryx_register_sighandler(SIGTERM, lq_sigint);
	
	pid_t pid;
	pid = fork();
	if (pid < 0)
		return -1;
	
	if (pid == 0) {
		
		fprintf (stdout, "Children\n");
		vlib_main_t *vm = &vlib_main;
		oryx_initialize();
		
		vm->argc = argc;
		vm->argv = argv;

		oryx_task_registry(&enqueue);
		//oryx_task_registry(&lserver);
		//oryx_task_registry(&lclient);
#if defined(HAVE_LOCAL_TEST)
		//oryx_task_registry(&newfile);
#endif
		classify_env_init(vm);
		oryx_task_launch();
		
		FOREVER {
			sleep (1);
			if (!running) {
				/* wait for handlers of enqueue and dequeue finish. */
				sleep (3);
				classify_runtime();
				break;
			}
		};
			
		classify_terminal();
	}else {
		FOREVER {
			if (!running) {
				int status;
				kill(pid, SIGKILL);
				waitpid(pid, &status, WNOHANG);
				fprintf (stdout, "the state is %d\n",status);
				break;
			}
			int timeout_sec = 300;
			foreach_directory_file (classify_home,
						do_timeout_check, (void *)&timeout_sec, 0);
			sleep (10);
		}
	}
	
	return 0;
}
