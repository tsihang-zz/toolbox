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
	void (*pcap_handler)(u_char *user, const struct pcap_pkthdr *h,
                                   const u_char *bytes);

	void *private;
	atomic_declare(uint64_t, rank);

	uint32_t ul_flags;
};

static __oryx_always_inline__
const char *ethtool_speed(uint32_t speed){
	switch (speed) {
		case SPEED_10:
			return "10";
			break;
		case SPEED_100:
			return "100";
			break;
		case SPEED_1000:
			return "1000";
			break;
		case SPEED_10000:
			return "10000";
			break;
		default:
			return "Unknown";
			break;
	}
}

static __oryx_always_inline__
const char *ethtool_duplex(uint32_t duplex) {
	switch (duplex) {
		case DUPLEX_HALF:
			return "Half-Duplex";
			break;
		case DUPLEX_FULL:
			return "Full-Duplex";
			break;
		default:
			return "Unknown!";
			break;
	}
}

ORYX_DECLARE(int netdev_up(const char *iface));
ORYX_DECLARE(int netdev_exist(const char *iface));
ORYX_DECLARE(int netdev_is_running(const char *iface, struct ethtool_cmd *ethtool));
ORYX_DECLARE(int netdev_is_up(const char *iface));
ORYX_DECLARE(int netdev_open(struct netdev_t *netdev));
ORYX_DECLARE(int netdev_down(const char *iface));
ORYX_DECLARE(void *netdev_cap(void *argv));


#endif

