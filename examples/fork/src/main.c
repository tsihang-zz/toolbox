#include "oryx.h"

#define VLIB_BUFSIZE	1024
#define VLIB_FIFO_NAME		"/tmp/fifo"
#define VLIB_MSG_SOURCE		"/tmp/msg.log"

#define VLIB_SHM_BASE_KEY	0x464F524B	/* FORK */

int running = 1;

typedef struct vlib_main_t {
	uint64_t	rx,
				tx;
} vlib_main_t;

vlib_main_t *vlib_main;

static void sigint_handler(int sig) {
	fprintf (stdout, "signal %d ...\n", sig);
	running = 0;
}

static void fifo_msg_depart(const char *msg, size_t msglen)
{
	char *p;
	char elem[128] = {0};
	int pos = 0;

	fprintf(stdout, "msg: %s\n", msg);
	for (p = (const char *)&msg[0];
				*p != '\0' && *p != '\n'; ++ p) {	
		if (*p == '#') {
			fprintf(stdout, "elem: %s\n", elem);
			pos = 0;
		} else {
			elem[pos ++] = *p;
		}
	}
}

int main(
	int 	__oryx_unused__	argc,
	char	__oryx_unused__	** argv
)
{
	char	value[VLIB_BUFSIZE];
	size_t	valen;
	FILE	*fp;
	int 	err;
	int 	fd;
	char msg[VLIB_BUFSIZE] = {0};
	size_t bytes;
	
	oryx_initialize();
	oryx_register_sighandler(SIGINT,	sigint_handler);
	oryx_register_sighandler(SIGTERM,	sigint_handler);
	//oryx_register_sighandler(SIGCHLD,	sigchld_handler);
	signal(SIGCHLD,	SIG_IGN);

	unlink(VLIB_FIFO_NAME);
	unlink(VLIB_MSG_SOURCE);
	
	err = mkfifo(VLIB_FIFO_NAME, 0777);
	if (err) {
		fprintf(stdout, "mkfifo: %s\n", oryx_safe_strerror(errno));
	} else {
		fd = open(VLIB_FIFO_NAME, O_RDONLY | O_NONBLOCK);
		if (fd < 0) {
			fprintf(stdout, "open: %s\n", oryx_safe_strerror(errno));
		}
	}

	vlib_shm_t shm = {
		.shmid = 0,
		.addr = 0,
	};
	
	err = oryx_shm_get(VLIB_SHM_BASE_KEY, sizeof(vlib_main_t), &shm);
	if (err) {
		exit (0);
	}
	
	vlib_main = (vlib_main_t *)shm.addr;
	vlib_main_t *vm = vlib_main;
	
	fp = fopen(VLIB_MSG_SOURCE, "a+");
	if (!fp) {
		fprintf(stdout, "fopen: %s\n", oryx_safe_strerror(errno));
		return 0;
	}

	/* creat msg */
	int entries = 10;
	uint32_t rand = 13567;
	while (entries --) {
		rand = oryx_next_rand(&rand);
		fprintf(fp, "%lu#", time(NULL) + rand % 0xFFFF);
	}	
	fflush(fp);
	
	/* A loop read the part of lines. */
	FOREVER {

		if(!running)
			break;
		
		sleep (3);
		fprintf(stdout, "rx %lu, tx %lu\n", vm->rx, vm->tx);

		bytes = read(fd, msg, VLIB_BUFSIZE);
		if (bytes > 0) {
			vm->rx ++;
			fifo_msg_depart(msg, bytes);
		}

		pid_t p = fork();
		if (p < 0) {
			fprintf(stdout, "fork: %s\n", oryx_safe_strerror(errno));
			continue;
		} else if (p == 0) {
			fprintf(stdout, "Children %d -> started\n", getpid());
			fd = open(VLIB_FIFO_NAME, O_WRONLY);
			if (fd < 0) {
				fprintf(stdout, "open: %s\n", oryx_safe_strerror(errno));
			} else {
				fp = fopen(VLIB_MSG_SOURCE, "r");
				if (!fp) {
					fprintf(stdout, "fopen: %s\n", oryx_safe_strerror(errno));					
					close(fd);
					exit(0);
				}				
				memset (value, 0, VLIB_BUFSIZE);
				while(fgets (value, VLIB_BUFSIZE, fp) && running) {
					valen = strlen(value);
					bytes = write(fd, value, valen);
					if(bytes < 0) {
						fprintf(stdout, "write: %s\n", oryx_safe_strerror(errno));
					} else {
						/* child process have inherited vlib_main */
						vm->tx ++;
					}
					usleep(1000);
				}
			}
			
			fprintf(stdout, "Children %d -> exitted\n", getpid());
			close(fd);
			fclose(fp);
			exit(0);
		}
	}

	fprintf(stdout, "exiting ... \n");
	oryx_shm_destroy(&shm);
	fclose(fp);
	return 0;
}

