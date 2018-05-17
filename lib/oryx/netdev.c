#include "oryx.h"
#include "netdev.h"

static void
netdev_dispatcher(u_char *argument,
		const struct pcap_pkthdr *pkthdr, const u_char *packet)
{
	printf ("defualt dispatch\n");
}

int netdev_open(struct netdev_t *netdev)
{
	netdev->ul_flags = NETDEV_OPEN_LIVE;
	return netdev_pcap_open ((dev_handler_t **)&netdev->handler, 
				netdev->devname, netdev->ul_flags);
}

void *netdev_cap(void *argv)
{
	int64_t rank_acc;
	struct netdev_t *netdev = (struct netdev_t *)argv;
	
	atomic64_set(&netdev->rank, 0);
	
	FOREVER {
		if (!netdev->handler)
			continue;
		
		rank_acc = pcap_dispatch(netdev->handler,
			1024, netdev->dispatch ? netdev->dispatch : netdev_dispatcher, 
			(u_char *)netdev);

		if (rank_acc >= 0) {
			atomic64_add (&netdev->rank, rank_acc);
		} else {
			pcap_close (netdev->handler);
			netdev->handler = NULL;
			switch (rank_acc) {
				case -1:
				case -2:
				case -3:
				default:
					printf("pcap_dispatch=%ld\n", rank_acc);
					break;
			}
		}
	}

	oryx_task_deregistry_id(pthread_self());
}

