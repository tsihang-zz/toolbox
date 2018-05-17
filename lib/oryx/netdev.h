#ifndef NETDEV_H
#define NETDEV_H

#if defined(HAVE_PCAP)
typedef pcap_t  dev_handler_t;
#else
typedef int		dev_handler_t;
/** pfring */
#endif

#include "netdev_pcap.h"
#include "netdev_pfring.h"

#define	NETDEV_OPEN_LIVE	(1 << 0)
#define NETDEV_OPEN_FAIL	(1 << 1)

struct netdev_t {
	dev_handler_t	*handler;
	char		devname[16];
	int (*netdev_capture_fn)(dev_handler_t, char *, size_t);
	int (*netdev_close_fn)(dev_handler_t);
	void (*dispatch)(u_char *argument,
						const struct pcap_pkthdr *pkthdr, const u_char *packet);

	void *private;
	atomic64_t         rank;

	u32 ul_flags;
};

int netdev_exist(const char *iface);
int netdev_is_running(const char* iface);
int netdev_open(struct netdev_t *netdev);
void *netdev_cap(void *argv);

#endif

