#include "oryx.h"
#include "config.h"

ATOMIC_DECLARE(int, nr_sub_netperf_proc);
static vlib_main_t *vlib_main;
static const char *shortopt = "vi:l:t:";

/* Parameter from args. */
static char *progname;
static char *ethdev;
static size_t mtu = 1024;
/* how long does this progress run (in seconds). 0 means forever. */
static time_t timeout = 0;

#define RXTX_BUF_MAX	1024
static char rx_buf[RXTX_BUF_MAX], tx_buf[RXTX_BUF_MAX] ="\
	AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\
	AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\
	AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\
	AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\
	AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\
	AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\
	AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\
	AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";

/* payload length */
static size_t tx_buf_len = 0;

/*
 * print a usage message
 */
static void
usage(void)
{
	fprintf (stdout,
		"%s -i ETHDEV -l MTU -t TIMEDOUT\n"
		" -v            : Show version\n"
		" -i ETHDEV     : Which netdev should be used to send packets\n"
		" -l MTU        : Maximum Transform Unit, %lu (in bytes)\n"
		" -t TIMEDOUT   : Time for netperf in runtime, 0 means never stop\n"
		, progname, mtu);
}

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
	fprintf (stdout, "%s, signal %d ...\n", progname, sig);
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

static vlib_ndev_t *ndev_open (const char *ethname)
{
	int err;
	int sock;
	struct ifreq ifr;
	vlib_ndev_t *ndev;
	vlib_main_t *vm = vlib_main;
	
	//sock = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		fprintf(stdout, "socket: %s\n",
			oryx_safe_strerror(errno));
		exit(0);
	}

	/* bind to specified interface */
	memset (&ifr, 0, sizeof (ifr));
	snprintf (ifr.ifr_name, sizeof (ifr.ifr_name), "%s", ethname);

	err = setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE,
		(char *)&ifr, sizeof(ifr));
    if (err  < 0) {
		fprintf(stdout, "setsockopt: %s\n",
			oryx_safe_strerror(errno));
		exit(0);
    }

	err = ioctl (sock, SIOCGIFHWADDR, &ifr);
	if (err < 0) {
		fprintf(stdout, "ioctl: %s\n",
			oryx_safe_strerror(errno));
		exit(0);
	}

	/* Nonblock */
	int flags = 0;
	if ((flags = fcntl(sock, F_GETFL, 0)) < 0) {
		fprintf(stdout, "%s\n",
			oryx_safe_strerror(errno));
		exit(0);
	}

	err = fcntl(sock, F_SETFL, flags | O_NONBLOCK);
	if (err) {
		fprintf(stdout, "%s\n",
			oryx_safe_strerror(errno));
		exit(0);
	}
	
	ndev = &vm->ndev[vm->nr_ndevs ++];
	ndev->sock = sock;
	memcpy (&ndev->ethname[0], ethname, strlen(ethname));

	return ndev;
	
}

static int netperf_init(int argc, char **argv)
{
	char c;
	vlib_main_t *vm = vlib_main;

	vm->start_time = time(NULL);
	progname = argv[0];
	
	while ((c = getopt(argc, argv, shortopt)) != -1) {
		switch ((char)c) {
			case 'v':
				{
					fprintf(stdout,
						"%s Verson %s. BUILD %s\n", progname, "1", "aaa");
					exit(0);
				}
			
			case 'i':
				{
					ethdev = optarg;
					break;
				}

			case 'l':
				{
					char *chkptr;
					mtu = strtol(optarg, &chkptr, 10);
					if ((chkptr != NULL && *chkptr == 0)) {
						tx_buf_len = (mtu - 14 - 20 - 20);
						break;
					}
					return -1;

				}
				break;

			case 't':
				{
					char *chkptr;
					int tmoff;
					if (is_number(optarg, strlen(optarg))) {
						tmoff = strtol(optarg, &chkptr, 10);
						if ((chkptr != NULL && *chkptr == 0)) {
							timeout = (vm->start_time + tmoff);
							break;
						}
					}
					return -1;
			
				}
				break;

			default:
				fprintf(stdout, "error\n");
				return -1;
		}
	}

	if (tx_buf_len > RXTX_BUF_MAX) {
		fprintf(stdout, "tx_buf_len out of range (max %d)\n",
			RXTX_BUF_MAX);
		return -1;
	}
	
	fprintf(stdout, "eth %s, tx_buf_len %lu, timeout %lu\n",
		ethdev, tx_buf_len, timeout);
	
	return 0;
}

static void netperf_rx_handler(vlib_ndev_t *ndev)
{
	/* Rx rountine */
	ssize_t bytes;
	struct sockaddr_in peer;
	socklen_t len = sizeof(peer);

	bytes = recvfrom(ndev->sock, rx_buf, RXTX_BUF_MAX,
			0, (struct sockaddr *)&peer, &len);
	if (bytes > 0) {
		ndev->nr_rx_bytes += (bytes + 14 + 20 + 20);
		ndev->nr_rx_pkts ++;
	} else {
		if (bytes < 0) {
			if (errno != EAGAIN) {
				fprintf(stdout, "rx: %s(%d)\n",
					oryx_safe_strerror(errno), errno);
			}
		}
	}
}

static void netperf_tx_handler(vlib_ndev_t *ndev)
{
	/* Tx rountine */
	ssize_t bytes;
	struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(12345);
    addr.sin_addr.s_addr = inet_addr("1.1.1.1");
	
	bytes = sendto(
			ndev->sock, tx_buf, tx_buf_len,
			0, (struct sockaddr *)&addr, sizeof(addr));
	if (bytes > 0) {
		ndev->nr_tx_bytes += (bytes + 14 + 20 + 20);
		ndev->nr_tx_pkts ++;
	} else {
		if (bytes < 0) {
			fprintf(stdout, "tx: %s\n",
				oryx_safe_strerror(errno));
		}
	}
}

static void netperf_handler (vlib_ndev_t *ndev)
{
	vlib_main_t *vm = vlib_main;

	if (!ndev) return;

	atomic_inc(nr_sub_netperf_proc);
	FOREVER {	
		/* Rx rountine */
		netperf_rx_handler(ndev);
		/* Tx rountine */
		netperf_tx_handler(ndev);

		if (netperf_is_quit(vm))
			break;	
	}
	
	atomic_dec(nr_sub_netperf_proc);
}

int main (
        int     __oryx_unused__   argc,
        char    __oryx_unused__   ** argv
)
{
	int err;	
	int i;
	vlib_shm_t shm = {
		.shmid = 0,
		.addr = 0,
			
	};
	vlib_ndev_t *ndev = NULL;

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

	err = netperf_init(argc, argv);
	if (err) {
		usage();
		exit(0);
	}
	
	memset(tx_buf, 'A', tx_buf_len);

	if (ethdev)
		ndev = ndev_open(ethdev);
	else {
		char lanx[32] = {0};
		for (i = 0; i < 8; i ++) {
			sprintf(lanx, "lan%d", (i+1));
			ndev_open(lanx);
		}
	}
	
	oryx_task_launch();

	pid_t p;
	if (ethdev) {
		p = fork();
		if (p < 0) {
			fprintf(stdout, "fork: %s\n",
				oryx_safe_strerror(errno));
		} else if (p == 0) {
			fprintf(stdout, "Children %d -> %s\n",
				getpid(), ndev->ethname);
			netperf_handler(ndev);
		}
	} else {
		for (i = 0; i < vm->nr_ndevs; i ++) {
			ndev = &vm->ndev[i];
			p = fork();
			if (p < 0) {
				fprintf(stdout, "fork: %s\n",
					oryx_safe_strerror(errno));
				continue;
			} else if (p == 0) {
				fprintf(stdout, "Children %d -> %s\n",
					getpid(), ndev->ethname);
				netperf_handler(ndev);
			}
		}
	}

	FOREVER {
		for (i = 0; i < vm->nr_ndevs; i ++) {
			ndev = &vm->ndev[i];
			fprintf(stdout, "%s, rx_pkts %lu, tx_pkts %lu\n",
				ndev->ethname, ndev->nr_rx_pkts, ndev->nr_tx_pkts);
		}
		//logprgname();	
		sleep(1);

		if (timeout) {
			if (timeout < (time(NULL))) {
				vlib_main->ul_flags |= VLIB_QUIT;
				fprintf(stdout, "Timedout ... quit\n");
			}
		}
		
		if (netperf_is_quit(vm)) {
			/* wait all subprocess quit finished. */
			while (atomic_read(nr_sub_netperf_proc))
				continue;
			break;
		}
	};

	return 0;
}

