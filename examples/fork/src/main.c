#include "oryx.h"

int running = 1;
#define	VLIB_FIFO_NAME			"/tmp/FIFO.domain"
#define VLIB_BUFSIZE			1024

static void sigint_handler(int sig) {
	fprintf (stdout, "signal %d ...\n", sig);
	running = 0;
}

void parent_read_fifo (void)
{
	int fifo;
	char buf[VLIB_BUFSIZE] = {0};
	size_t rb;
	

	fifo = open(VLIB_FIFO_NAME, O_RDONLY | O_NONBLOCK, 0);
	if(fifo < 0) {
		fprintf(stdout, "%s\n", oryx_safe_strerror(errno));
		exit(0);
	}

	FOREVER {
		memset (buf, 0, VLIB_BUFSIZE);
		rb = read(fifo, buf, VLIB_BUFSIZE);
		if(rb <= 0) {
			if(rb == EAGAIN) {
				fprintf(stdout, "No data avilable, try again\n");
				continue;
			}

			if(rb == 0) {
				//fprintf(stdout, "Timedout\n");
				continue;
			}
			exit (0);
		}

		fprintf(stdout, "Read FIFO: %s\n", buf);		
		/* aging entries in table. */
		sleep(1);
	}

	unlink(VLIB_FIFO_NAME);
	oryx_task_deregistry_id(pthread_self());
	return NULL;
}

struct oryx_task_t FIFOMonitor = {
		.module 		= THIS,
		.sc_alias		= "FIFO Monitor Task",
		.fn_handler 	= fifo_monitor,
		.lcore_mask		= INVALID_CORE,
		.ul_prio		= KERNEL_SCHED,
		.argc			= 0,
		.argv			= NULL,
		.ul_flags		= 0,	/** Can not be recyclable. */
};

static uint64_t count = 10;
int main(
	int 	__oryx_unused_param__	argc,
	char	__oryx_unused_param__	** argv
)
{
	int		i,
			len,
			status = 0;
	pid_t	pid,
			pid0 = -1;
		
	oryx_initialize();
	oryx_register_sighandler(SIGINT,	sigint_handler);
	oryx_register_sighandler(SIGTERM,	sigint_handler);

	int fifo;
	if(oryx_path_exsit(VLIB_FIFO_NAME)) {
		unlink(VLIB_FIFO_NAME);
	}
	
	if(mkfifo(VLIB_FIFO_NAME, 0644)) {
		fprintf(stdout, "%s\n", oryx_safe_strerror(errno));
		exit(0);
	}
	fprintf(stdout, "FIFO created\n");
	
	oryx_task_registry(&FIFOMonitor);
	
	for (i = 0; i < 3; i ++) {
		pid0 = fork();
		if (pid0 == 0 || pid0 == -1)
			break;
		else if (pid0 < 0) {
			fprintf(stdout, "%s\n", oryx_safe_strerror(errno));
			exit(-1);
		}
	}

	count = 100;
	if (pid0 == 0) {
		char say[VLIB_BUFSIZE] = {0};
		int f0 = open(VLIB_FIFO_NAME, O_WRONLY | O_NONBLOCK, 0);
		count ++;
		sprintf(say, "Children %lu(%lu), %lu\n", getpid(), getppid(), count);
		write(f0, say, strlen(say));
		sleep(1);
	}
	if (pid0 > 0) {
		count += 10;
		parent_read_fifo();
		sleep(10);
		fprintf(stdout, "Parent %lu, %lu\n", getpid(), count);
	}

	return 0;
}

