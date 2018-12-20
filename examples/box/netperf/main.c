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

struct vlib_ethdev_t {
	char *name;
	uint64_t	nr_rx_pkts;
	uint64_t	nr_tx_pkts;
	uint64_t	nr_rx_bytes;
	uint64_t	nr_tx_bytes;
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

	sock = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
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
	skpair->dev[0].name = strdup(ethname1);
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
	skpair->dev[1].name = strdup(ethname2);
	skpair->sock[1] = sock;

}

int toIpString(const char* ip, uint32_t* retval) {
   struct hostent *hp;
   *retval = 0;

   if (strcmp(ip, "0.0.0.0") == 0) {
      return 0;
   }

   if (ip[0] == 0) {
      return 0;
   }

   // It seems it can't resolve IP addresses, or tries too hard to find a name,
   // or something.  Need to check for a.b.c.d
   *retval = inet_addr(ip);
   if (*retval != INADDR_NONE) {
      *retval = ntohl(*retval);
      return 0;
   }

   // Otherwise, try to resolve it below.
   hp = gethostbyname(ip);
   if (hp == NULL) {
      return -1;
   }//if
   else {
      *retval = ntohl(*((unsigned int*)(hp->h_addr_list[0])));
      if (*retval != 0) {
         return 0;
      }
      else {
         return -1;
      }
   }
   return -1;
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

	netperf_sockpair("lan1", "lan2");
	netperf_sockpair("lan3", "lan4");
	netperf_sockpair("lan5", "lan6");
	netperf_sockpair("lan7", "lan8");
	netperf_sockpair("enp5s0f2", "enp5s0f3");

	int i, j;
	for (i = 0; i < VLIB_GRP_NUM; i ++) {
		vlib_sockpair_t *skpair = &vlib_sockpair[i];
		fprintf(stdout, "=== GROUP[%d]:\n", i);
		fprintf(stdout, "%8s%12d\n", skpair->dev[0].name, skpair->sock[0]);
		fprintf(stdout, "%8s%12d\n", skpair->dev[1].name, skpair->sock[1]);
	}

	struct sockaddr_in dest_addr;
	// set up the destination address
	memset(&dest_addr, '\0', sizeof(dest_addr));
	dest_addr.sin_family = PF_PACKET;

	uint32_t dip = 0;
	toIpString("192.168.1.1", &dip);
	dest_addr.sin_addr.s_addr = htonl(dip);
	dest_addr.sin_port = htons(9);
   	
	oryx_task_launch();
	FOREVER {
		if (netperf_is_quit(vm))
			break;		
		usleep(1000);
		logprgname();
#if 0
		for (i = 0; i < VLIB_GRP_NUM; i ++) {
			vlib_sockpair_t *skpair = &vlib_sockpair[i];
			for (j = 0; j < 2; j ++) {
				char msg[1024] = {0};
				sprintf(msg, "hello the world");
				uint64_t nr_tx_bytes = sendto(skpair->sock[j], msg, strlen(msg),
					0, (struct sockaddr *)&dest_addr, sizeof (dest_addr));
				if (nr_tx_bytes > 0) {
					skpair->dev[j].nr_tx_bytes += nr_tx_bytes;
					fprintf(stdout, "[%s TX]  %s\n", skpair->dev[j].name, msg);
				}
			}
		}
#else
		for (i = 0; i < VLIB_GRP_NUM; i ++) {
			vlib_sockpair_t *skpair = &vlib_sockpair[i];
			for (j = 0; j < 2; j ++) {
				char msg[1024] = {0};
				uint64_t nr_rx_bytes = recvfrom(skpair->sock[j], msg, 1024, 0, NULL, NULL);
				if (nr_rx_bytes > 0) {
					skpair->dev[j].nr_rx_bytes += nr_rx_bytes;
					fprintf(stdout, "[%s RX]  %s\n", skpair->dev[j].name, msg);
				}
			}
		}	
#endif
	};

	return 0;
}

