#include "oryx.h"
#include "config.h"

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

int running = 1;

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

static void logprgname(void)
{
	char logzomb[1024] = {0};
	char prgname[32] = {0};
	const char *zombie = "./zombie.txt";
		
	if (oryx_path_exsit(zombie))
		remove(zombie);
	
	oryx_get_prgname(getpid(), prgname);
	fprintf(stdout, "$$> hello, it's %s\n", prgname);
	sprintf(logzomb, "ps -ef | grep %s | grep -v grep | grep defunct | awk '{print$2}' >> %s", prgname, zombie);
	do_system(logzomb);

}

int main (
        int     __oryx_unused__   argc,
        char    __oryx_unused__   ** argv
)
{
	vlib_main_t *vm = &vlib_main;
	struct fq_element_t *fqe = NULL;	
	int err;

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
		err = oryx_mkdir(vm->inotifydir, NULL);
		if (err) {
			fprintf(stdout, "mkdir: %s (%s)\n",
				oryx_safe_strerror(errno), vm->inotifydir);
		}

		oryx_lq_new("A FMGR queue", 0, (void **)&fmgr_q);
		oryx_lq_dump(fmgr_q);
		oryx_task_registry(&inotify);
	}

	if(vm->classdir /* dir exist ? */) {
		err = oryx_mkdir(vm->classdir, NULL);
		if (err) {
			fprintf(stdout, "mkdir: %s (%s)\n",
				oryx_safe_strerror(errno), vm->classdir);
		}
	}

	if(vm->savdir /* dir exist ? */) {
		err = oryx_mkdir(vm->savdir, NULL);
		if (err) {
			fprintf(stdout, "mkdir: %s (%s)\n",
				oryx_safe_strerror(errno), vm->savdir);
		}
	}	

	vm->mme_htable = oryx_hashtab_new("HASHTABLE",
						0,
						DEFAULT_HASH_CHAIN_SIZE,
						mmekey_hval,
						mmekey_cmp,
						mmekey_free,
						0/* HTABLE_SYNCHRONIZED is unused,
						  * because the table is no need to update.*/);

	classify_initialization(vm);
	oryx_task_launch();

	FOREVER {

		if (!running)
			break;
		
		sleep (3);

		logprgname();
		
		fqe = oryx_lq_dequeue(fmgr_q);
		if (fqe == NULL)
			continue;
		if(!oryx_path_exsit(fqe->name)){
			free(fqe);
			continue;
		}
		
		pid_t p = fork();
		if (p < 0) {
			fprintf(stdout, "fork: %s\n", oryx_safe_strerror(errno));
			free(fqe);
			continue;
		} else if (p == 0) {
			fprintf(stdout, "Children %d -> %s\n", getpid(), fqe->name);
			vlib_conf_t conf = {
				.ul_flags = (VLIB_CONF_MV | VLIB_CONF_LOG)
			};
			classify_offline(fqe->name, &conf);
			classify_result(fqe->name, &conf);
			fprintf(stdout, "Children %d -> %s, exit\n", getpid(), fqe->name);
			exit(0);
		}
		
		free(fqe);
	}
		
	classify_terminal();
	return 0;
}

