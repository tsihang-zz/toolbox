#include "oryx.h"
#include "config.h"
#include "netdev.h"

static vlib_main_t *vlib_main;
static const char *shortopt = "vi:l:t:s";

/* Parameter from args. */
static char *progname;
static char *ethdev;
static size_t mtu = 64;
/* how long does this progress run (in seconds). 0 means forever. */
static time_t timeout = 0;

static char rx_buf[RXTX_BUF_MAX], tx_buf[RXTX_BUF_MAX] = {0};

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

static void pf_packet(vlib_ndev_t *ndev)
{
	size_t l = 0;
	char *p = &ndev->tx_buf[0];
	char src_mac[6] = {0};
	char dst_mac[6] = {0};

    dst_mac[0] = 0x10;
    dst_mac[1] = 0x78;
    dst_mac[2] = 0xd2;
    dst_mac[3] = 0xc6;
    dst_mac[4] = 0x2f;
    dst_mac[5] = 0x89;


	l += sprintf (p + l, "%s", src_mac);
	l += sprintf (p + l, "%s", dst_mac);
	l += sprintf (p + l, "%x", ETH_P_DEAN);

	/* */
	memset(&ndev->dev, 0, sizeof (ndev->dev));
	ndev->dev.sll_ifindex = if_nametoindex (ndev->name);
	ndev->dev.sll_family = AF_PACKET;	
	ndev->dev.sll_halen = htons (6);
	memcpy(&ndev->dev.sll_addr, ndev->ethmac, 6);

	memset (p + l, 'A', tx_buf_len);

}

static vlib_ndev_t *ndev_register (const char *name)
{
	int err;
	int sock;
	uint8_t ethmac[6] = {0};
	struct ifreq ifr;
	vlib_ndev_t *ndev;
	vlib_main_t *vm = vlib_main;
	
	sock = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	//sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		fprintf(stdout, "socket: %s\n",
			oryx_safe_strerror(errno));
		exit(0);
	}

	memset (&ifr, 0, sizeof (ifr));
	snprintf (ifr.ifr_name, sizeof (ifr.ifr_name), "%s", name);
	err = ioctl (sock, SIOCGIFHWADDR, &ifr);
	if (err < 0) {
		fprintf(stdout, "ioctl: %s\n",
			oryx_safe_strerror(errno));
		exit(0);
	}
	
	/** Copy source MAC address. */
	memcpy(ethmac, ifr.ifr_hwaddr.sa_data, 6);

	/* Nonblock */
	int flags = 0;
	if ((flags = fcntl(sock, F_GETFL, 0)) < 0) {
		fprintf(stdout, "%s\n",
			oryx_safe_strerror(errno));
		exit(0);
	}

	ndev = &vm->ndev[vm->nr_ndevs ++];
	ndev->name = strdup(name);
	pf_packet(ndev);
	memcpy(ndev->ethmac, ethmac, 6);

	close(sock);

	return ndev;	
}


static void ndev_open_promisc (vlib_ndev_t *ndev)
{
	int err;
	int sock;
	struct ifreq ifr;
	vlib_main_t *vm = vlib_main;
	
	sock = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	//sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		fprintf(stdout, "socket: %s\n",
			oryx_safe_strerror(errno));
		exit(0);
	}

	/* bind to specified interface */
	memset (&ifr, 0, sizeof (ifr));
	snprintf (ifr.ifr_name, sizeof (ifr.ifr_name), "%s", ndev->name);

	err = setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE,
		(char *)&ifr, sizeof(ifr));
    if (err  < 0) {
		fprintf(stdout, "setsockopt: %s\n",
			oryx_safe_strerror(errno));
		exit(0);
    }

#if 0
	/* setup promisc */
	ifr.ifr_flags |= IFF_PROMISC;
	err = ioctl (sock, SIOCGIFFLAGS, &ifr);
	if (err < 0) {
		fprintf(stdout, "ioctl: %s\n",
			oryx_safe_strerror(errno));
		exit(0);
	}
#endif

	/* setup non-block */
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

	ndev->sock  = sock;
}

/**
 * tx_buf_len = (mtu - 14 - 20 - 20);
 * timeout = (vm->start_time + tmoff);
 */
static int netperf_init(int argc, char **argv)
{
	int opt;
	vlib_main_t *vm = vlib_main;

	vm->start_time = time(NULL);
	progname = argv[0];
	
	while ((opt = getopt(argc, argv, shortopt)) != -1) {
		switch (opt) {
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
			case 's':
				{
					//fprintf(stdout, "+++++++\n");
					break;
				}
			
			case 'l':
				{
					//fprintf(stdout, "optind %c\n", 'l');
					char *chkptr;
					mtu = strtol(optarg, &chkptr, 10);
					if ((chkptr != NULL && *chkptr == 0)) {
						tx_buf_len = (mtu - 14 - 20 - 20);
						break;
					}					
					return -1;
				}
				
			case 't':
				{
					//fprintf(stdout, "optind %c\n", 't');
					char *chkptr;
					long int tmoff = strtol(optarg, &chkptr, 10);
					if ((chkptr != NULL && *chkptr == 0)) {
						timeout = (vm->start_time + tmoff);
						break;
					}					
					return -1;
				}

			default:
				fprintf(stdout, "netperf_init error\n");
				return -1;
		}
	}

	if (tx_buf_len > RXTX_BUF_MAX) {
		fprintf(stdout, "tx_buf_len out of range (max %d)\n",
			RXTX_BUF_MAX);
		return -1;
	}
	
	fprintf(stdout, "eth %s, mtu %lu (tx_buf_len %lu), timeout %lu\n",
		ethdev, mtu, tx_buf_len, timeout);
	sleep(1);
	
	return 0;
}

static __oryx_always_inline__
void dump_pkt(uint8_t *pkt, int len)
{
	int i = 0;
	
	for (i = 0; i < len; i ++){
		if (!(i % 16))
			fprintf (stdout, "\n");
		fprintf (stdout, "%02x ", pkt[i]);
	}
	fprintf (stdout, "\n");
}

static void netperf_rx_handler(vlib_ndev_t *ndev)
{
	/* Rx rountine */
	ssize_t bytes;

	/* Recv any. */
	bytes = recvfrom(ndev->sock, rx_buf, RXTX_BUF_MAX,
			0, NULL, NULL);
	if (bytes > 0) {
		ndev->nr_rx_bytes += (bytes + 14 + 20 + 20);
		ndev->nr_rx_pkts ++;
		//dump_pkt(rx_buf, bytes);
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

	struct sockaddr_ll addr;
	// Fill out sockaddr_ll.
	memset (&addr, 0, sizeof (addr));
	memcpy(&addr, &ndev->dev, sizeof(addr));

	bytes = sendto(
			ndev->sock, tx_buf, tx_buf_len,
			0, (struct sockaddr *)&addr, sizeof(addr));
	if (bytes > 0) {
		ndev->nr_tx_bytes += (bytes + 14 + 20 + 20);
		ndev->nr_tx_pkts ++;
	} else {
		if (bytes < 0) {
			if (errno != EAGAIN)
			fprintf(stdout, "tx: %s(%d)\n",
				oryx_safe_strerror(errno), errno);
		}
	}
}

static void netperf_handler (vlib_ndev_t *ndev)
{
	vlib_main_t *vm = vlib_main;

	atomic_inc(vm->nr_sub_netperf_proc);
	FOREVER {

		/* Tx rountine */
		//netperf_tx_handler(ndev);

		/* Rx rountine */
		netperf_rx_handler(ndev);

		if (netperf_is_quit(vm))
			break;
	};
	
	atomic_dec(vm->nr_sub_netperf_proc);
	fprintf(stdout, "vm->nr_sub_netperf_proc %d\n",
		atomic_read(vm->nr_sub_netperf_proc));
	
	exit(0);
}

vlib_ndev_t netdev = {
	.ul_flags = 0,
	.type = 0,
};

static
struct oryx_task_t ndev_capture_templater = {
	.module = THIS,
	.sc_alias = "Ndev Capture Task",
	.fn_handler = ndev_capture,
	.lcore_mask = INVALID_CORE,
	.ul_prio = KERNEL_SCHED,
	.argc = 1,
	.argv = &netdev,
	.ul_flags = 0,	/** Can not be recyclable. */
};

static void ndev_pkt_handler 
(
	IN u_char *user,
	IN const struct pcap_pkthdr *h,
	IN const u_char *bytes
)
{
	vlib_ndev_t *ndev = (vlib_ndev_t *)user;
	
	ndev->nr_rx_pkts ++;
	ndev->nr_rx_bytes += h->caplen;
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

	if (ethdev)
		ndev = ndev_register(ethdev);
	else {
		char ethx[32] = {0};
		for (i = 1; i <= 8; i ++) {
			sprintf(ethx, "lan%d", i);
			ndev_register(ethx);
		}
		for (i = 2; i <= 3; i ++) {
			sprintf(ethx, "enp5s0f%d", i);
			ndev_register(ethx);
		}
	}

#if defined(HAVE_SOCK_CAPTURE)	
	pid_t p;
	if (ethdev) {
		p = fork();
		if (p < 0) {
			fprintf(stdout, "fork: %s\n",
				oryx_safe_strerror(errno));
		} else if (p == 0) {
			fprintf(stdout, "Children %d -> %s\n",
				getpid(), ndev->name);
			ndev_open_promisc(ndev);
			netperf_handler(ndev);
		}
	} else {
		for (i = 0; i < vm->nr_ndevs; i ++) {
			p = fork();
			if (p < 0) {
				fprintf(stdout, "fork: %s\n",
					oryx_safe_strerror(errno));
				continue;
			} else if (p == 0) {
				ndev = &vm->ndev[i];
				fprintf(stdout, "Children %d -> %s\n",
					getpid(), ndev->name);
				ndev_open_promisc(ndev);
				netperf_handler(ndev);
			}
		}
	}
#else
	for (i = 0; i < vm->nr_ndevs; i ++) {
		ndev = &vm->ndev[i];
		struct oryx_task_t *t;
		char desc[32] = {0};
		
		ndev_open(ndev, ndev_pkt_handler, NULL);

		sprintf(desc, "%s%d", "NetDEV Capture", i);
		ASSERT((t = malloc(sizeof(struct oryx_task_t))) != NULL);
		memset(t, 0, sizeof (struct oryx_task_t));
		t->argc = 1;
		t->argv = ndev;
		t->sc_alias = strdup(desc);
		t->fn_handler = ndev_capture;
		t->lcore_mask = INVALID_CORE;
		t->ul_prio = KERNEL_SCHED;
		t->ul_flags = TASK_CAN_BE_RECYCLABLE;
		oryx_task_registry(t);
	}
#endif
	oryx_task_launch();
	FOREVER {
		for (i = 0; i < vm->nr_ndevs; i ++) {
			ndev = &vm->ndev[i];
			fprintf(stdout, "%s, rx_pkts %lu, tx_pkts %lu\n",
				ndev->name, ndev->nr_rx_pkts, ndev->nr_tx_pkts); 
		}
		//logprgname();	
		sleep(1);

		if (timeout) {
			if (timeout < (time(NULL))) {
				vm->ul_flags |= VLIB_QUIT;
				fprintf(stdout, "Timedout ... quit\n");
			}
		}
		
		if (netperf_is_quit(vm) &&
			atomic_read(vm->nr_sub_netperf_proc) == 0) {
			/* wait all subprocess quit finished. */
			break;
		}
	};

	return 0;
}

