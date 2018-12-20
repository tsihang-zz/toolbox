#include "oryx.h"
#include "config.h"

vlib_main_t *vlib_main;

static __oryx_always_inline__
void logprgname(void)
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

static __oryx_always_inline__
void sigint_handler(int sig) {
	fprintf (stdout, "signal %d ...\n", sig);
	vlib_main->ul_flags |= VLIB_QUIT;
}

static __oryx_always_inline__
void sigchld_handler(int num) {
	num = num;
    int status;   
    pid_t pid = waitpid(-1, &status, 0);   
    if (WIFEXITED(status)) {
		if(WEXITSTATUS(status) != 0)
        	fprintf(stdout,
        		"The child %d exit with code %d\n", pid, WEXITSTATUS(status));   
    }   
}

#if 0
static int server_handler
(
	IN void *instance,
	IN void  __oryx_unused__ *opaque
)
{
        struct oryx_server_t *s = (struct oryx_server_t *)instance;
        int nb_rx_length, nb_tx_length;

        while (1) {
                /* Wait for data from client */
                nb_rx_length = read(s->sock, s->rx_buf, 64);
                if (nb_rx_length == -1) {
                        fprintf(stdout, "read: %s\n", oryx_safe_strerror(errno));
                        close(s->sock);
                        return -1;
                }

                /* Send response to client */
                nb_tx_length = write(s->sock, s->rx_buf, nb_rx_length);
                if (nb_tx_length == -1) {
                        fprintf(stdout, "write: %s\n", oryx_safe_strerror(errno));
                        close(s->sock);
                        return -1;
                }
        }
}

static struct oryx_task_t client_task =
{
        .module         = THIS,
        .sc_alias       = "Client handler Task",
        .fn_handler     = client_thread,
        .lcore_mask     = INVALID_CORE,
        .ul_prio        = KERNEL_SCHED,
        .argc           = 1,
        .argv           = &client,
        .ul_flags       = 0,    /** Can not be recyclable. */
};


static struct oryx_task_t server_task =
{
        .module         = THIS,
        .sc_alias       = "Server handler Task",
        .fn_handler     = server_thread,
        .lcore_mask     = INVALID_CORE,
        .ul_prio        = KERNEL_SCHED,
        .argc           = 1,
        .argv           = &server,
        .ul_flags       = 0,    /** Can not be recyclable. */
};
#endif

struct vlib_ethdev_t {
	char *name;
};

typedef struct vlib_sockpair_t {
	int sock[2];
	struct vlib_ethdev_t dev[2];
} vlib_sockpair_t;

#define VLIB_GRP_NUM 5
vlib_sockpair_t vlib_sockpair[VLIB_GRP_NUM];

static void netperf_sockpair (const char *ethname1, const char *ethname2)
{
	static int i = 0;
	int sock;
	vlib_sockpair_t *skpair;
	struct ifreq ifr;
	
	skpair = &vlib_sockpair[i ++];

	sock = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (sock < 0) {
		fprintf(stdout, "socket: %s\n",
			oryx_safe_strerror(errno));
		exit(0);
	}

	/* bind to specified interface */
	memset (&ifr, 0, sizeof (ifr));
	snprintf (ifr.ifr_name, sizeof (ifr.ifr_name), "%s", ethname1);
    if (setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE,
		(char *)&ifr, sizeof(ifr))  < 0) {
		fprintf(stdout, "setsockopt: %s\n",
			oryx_safe_strerror(errno));
		exit(0);
    }

	if (ioctl (sock, SIOCGIFHWADDR, &ifr) < 0) {
		fprintf(stdout, "ioctl: %s\n",
			oryx_safe_strerror(errno));
		exit(0);
	}
	
	skpair->sock[0] = sock;

	sock = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (sock < 0) {
		fprintf(stdout, "socket: %s\n",
			oryx_safe_strerror(errno));
		exit(0);
	}

	/* bind to specified interface */
	memset (&ifr, 0, sizeof (ifr));
	snprintf (ifr.ifr_name, sizeof (ifr.ifr_name), "%s", ethname2);
    if (setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE,
		(char *)&ifr, sizeof(ifr))  < 0) {
		fprintf(stdout, "setsockopt: %s\n",
			oryx_safe_strerror(errno));
		exit(0);
    }

	if (ioctl (sock, SIOCGIFHWADDR, &ifr) < 0) {
		fprintf(stdout, "ioctl: %s\n",
			oryx_safe_strerror(errno));
		exit(0);
	}
	
	skpair->sock[1] = sock;


}

int main (
        int     __oryx_unused__   argc,
        char    __oryx_unused__   ** argv
)
{
	int err;

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
	memset (vm, 0, sizeof(vlib_main_t));
	vm->start_time = time(NULL);

	oryx_initialize();
	oryx_register_sighandler(SIGINT,	sigint_handler);
	oryx_register_sighandler(SIGTERM,	sigint_handler);
	//oryx_register_sighandler(SIGCHLD,	sigchld_handler);
	signal(SIGCHLD,	SIG_IGN);

	/* lan1 <-> lan2 */
	netperf_sockpair("enp0s3", "enp0s3");

	oryx_task_launch();
	FOREVER {
		if (netperf_is_quit(vm))
			break;
		sleep(1);
		logprgname();
	};

	return 0;
}

