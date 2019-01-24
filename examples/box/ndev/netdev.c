/*!
 * @file netdev.c
 * @date 2019/01/05
 *
 * TSIHANG (haechime@gmail.com)
 */

#include "oryx.h"
#include "netdev.h"

static void ndev_pkt_handler 
(
	IN u_char *user,
	IN const struct pcap_pkthdr *h,
	IN const u_char *bytes
)
{
	user = user;
	h = h;
	bytes = bytes;
	
	fprintf (stdout, "defualt pkt_handler\n");
}

static int is_there_an_eth_iface_named
(
	IN struct ifreq *ifr
)
{
	int skfd = socket(AF_INET, SOCK_DGRAM, 0);
	if(skfd < 0) {
		oryx_loge(errno,
				"%s", oryx_safe_strerror(errno));
		return -1;
	}

	if(ioctl(skfd, SIOCGIFFLAGS, ifr) < 0) {
		oryx_loge(errno,
				"%s-%d(\"%s\")", oryx_safe_strerror(errno), errno, ifr->ifr_name);
		if (errno == ENODEV) {
			close(skfd);
			skfd = -ENODEV;
	    	return skfd;
		}
		return -1;
	}

	return skfd;
}

int ndev_exist
(
	IN const char *iface
)
{
	struct ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, iface);
	int skfd = is_there_an_eth_iface_named(&ifr);
	if(skfd > 0) {
		close (skfd);
		return 1;
	} else {
		if (skfd == 0/** no such device */)
			return 0;
		else
			return 0;
	}
}

int ndev_is_running
(
	IN const char *iface,
	IN struct ethtool_cmd *ethtool
)
{
	struct ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, iface);
	int skfd = is_there_an_eth_iface_named(&ifr);
	
	if(skfd <= 0)
		return skfd;

	if(ifr.ifr_flags & IFF_RUNNING) {
		ethtool->cmd = ETHTOOL_GSET;
		ifr.ifr_data = (caddr_t)ethtool;
		ioctl(skfd, SIOCETHTOOL, &ifr);
		close (skfd);
	    return 1;
	} else {
		close (skfd);
	    return 0;
	}
}


int ndev_is_up
(
	IN const char *iface
)
{
	struct ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, iface);
	int skfd = is_there_an_eth_iface_named(&ifr);

	if(skfd <= 0)
		return skfd;

	close (skfd);
	
	if(ifr.ifr_flags & IFF_UP) {
	    return 1;
	} else {
	    return 0;
	}
}

int ndev_up
(
	IN const char *iface
)
{
	struct ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, iface);
	int skfd = is_there_an_eth_iface_named(&ifr);

	if(skfd < 0)
		return skfd;

	ifr.ifr_flags |= IFF_UP;

	if(ioctl(skfd, SIOCSIFFLAGS, &ifr) < 0) {
		oryx_loge(errno,
				"%s(\"%s\")", oryx_safe_strerror(errno), ifr.ifr_name);
	    close(skfd);
	    return -1;
	}

	close (skfd);
	return 0;
}

int ndev_down
(
	IN const char *iface
)
{
	struct ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, iface);
	int skfd = is_there_an_eth_iface_named(&ifr);

	if(skfd < 0)
		return skfd;

	ifr.ifr_flags &= ~IFF_UP;

	if(ioctl(skfd, SIOCSIFFLAGS, &ifr) < 0) {
		oryx_loge(errno,
				"%s(\"%s\")", oryx_safe_strerror(errno), ifr.ifr_name);
	    close(skfd);
	    return -1;
	}

	close (skfd);
	return 0;
}

int
ndev_open
(
	IN vlib_ndev_t *ndev,
	IN void (*ndev_pkt_handler)(u_char *user,
					const struct pcap_pkthdr *h, const u_char *bytes),
	IN int (*ndev_close_fn)(ndev_handler_t *)
)
{
	int err;
	
	ndev->ul_flags = NETDEV_OPEN_LIVE;
	err = ndev_pcap_open ((ndev_handler_t **)&ndev->handler, 
				ndev->name, ndev->ul_flags);
	if (!err) {
		ndev->ndev_pkt_handler = \
			(ndev_pkt_handler ? ndev_pkt_handler : ndev_pkt_handler);
		ndev->ndev_close_fn = \
			(ndev_close_fn ? ndev_close_fn : ndev_pcap_close);
	}

	return err;
}

void *ndev_capture
(
	IN void *argv
)
{
	int64_t rank_acc;
	vlib_ndev_t *netdev = (vlib_ndev_t *)argv;
	
	atomic_set(netdev->rank, 0);

	FOREVER {
		if (!netdev->handler)
			continue;
		
		rank_acc = pcap_dispatch(netdev->handler,
			1024, 
			netdev->ndev_pkt_handler, 
			(u_char *)netdev);

		if (rank_acc >= 0) {
			atomic_add (netdev->rank, rank_acc);
		} else {
			netdev->ndev_close_fn(netdev->handler);
			netdev->handler = NULL;
			switch (rank_acc) {
				case -1:
				case -2:
				case -3:
				default:
					fprintf(stderr,
						"pcap_dispatch=%ld\n", rank_acc);
					break;
			}
		}
	}

	oryx_task_deregistry_id(pthread_self());
}

