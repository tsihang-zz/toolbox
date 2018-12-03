#include "oryx.h"
#include "config.h"

#define VLIB_MAX_PID_NUM	4
#define VLIB_FIFO_MSG_END	'#'

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

static void fifo_msg_depart(const char *msg, size_t msglen)
{
	char *p;
	char elem[128] = {0};
	int pos = 0;

	fprintf(stdout, "msg: %s\n", msg);
	for (p = (const char *)&msg[0];
				*p != '\0' && *p != '\n'; ++ p) {	
		if (*p == VLIB_FIFO_MSG_END) {
			//fprintf(stdout, "elem: %s\n", elem);
			fmgr_remove(elem);
			pos = 0;
		} else {
			elem[pos ++] = *p;
		}
	}
}

static int openfifo(const char *fifoname)
{
	int fd = -1;
	int err;
	
	unlink(fifoname);
	err = mkfifo(fifoname, 0777);
	if (err) {
		fprintf(stdout, "mkfifo: %s\n", oryx_safe_strerror(errno));
	} else {
		fd = open(fifoname, O_RDONLY | O_NONBLOCK);
		if (fd < 0) {
			fprintf(stdout, "open: %s\n", oryx_safe_strerror(errno));
		}
	}

	return fd;
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
	int fd;
	char msg[VLIB_BUFSIZE];	
	size_t bytes;

	if ((fd = openfifo(VLIB_FIFO_NAME)) < 0)
		exit(0);
	
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
		if(!oryx_path_exsit(vm->inotifydir)) {
			err = mkdir(vm->inotifydir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
			if(err) {
				fprintf(stdout, "mkdir: %s\n", oryx_safe_strerror(errno));
			}
		}
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

		if (!running)
			break;
		
		sleep (3);
		
		memset (msg, 0, VLIB_BUFSIZE);
		if ((bytes = read (fd, msg, VLIB_BUFSIZE)) > 0)
			fifo_msg_depart(msg, bytes);
		
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
			classify_offline(fqe->name, NULL); 			
			fprintf(stdout, "Children %d -> %s, exit\n", getpid(), fqe->name);
			fd = open(VLIB_FIFO_NAME, O_WRONLY);
			if (fd < 0) {
				fprintf(stdout, "open: %s\n", oryx_safe_strerror(errno));
			} else {
				char msgo[fq_name_length] = {0};
				size_t l;
	
				l = sprintf(msgo, "%s%c", fqe->name, VLIB_FIFO_MSG_END);
				bytes = write(fd, msgo, l);
				if (bytes < 0) {
					fprintf(stdout, "write: %s\n", oryx_safe_strerror(errno));
				}
				close(fd);
			}

			exit(0);
		}
		
		free(fqe);
	}
		
	classify_terminal();
	return 0;
}

