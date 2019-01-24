/*!
 * @file netdev.h
 * @date 2019/01/05
 *
 * TSIHANG (haechime@gmail.com)
 */

#ifndef NETDEV_H
#define NETDEV_H

#include <linux/if_ether.h>
#include <linux/if_packet.h>

#include "ethtool.h"

#if defined(HAVE_PCAP)
#include <pcap.h>
typedef pcap_t  ndev_handler_t;
#endif

#if !defined(HAVE_PCAP)
struct pcap_pkthdr {
	struct timeval ts;
	bpf_u_int32 caplen;
	bpf_u_int32 len;
};
#endif

typedef int		ndev_handler_t;
/** pfring */
#endif

#include "netdev_pcap.h"
//#include "netdev_pfring.h"

#define	NETDEV_OPEN_LIVE	(1 << 0)
#define NETDEV_OPEN_FAIL	(1 << 1)

typedef struct vlib_ndev_t {
	ndev_handler_t	*handler;
	char			*name;
	char			ethmac[6];
	int				sock;

	int				type;	/**< console ? netdev ? disk ? etc... */

	struct sockaddr_ll dev;
	char tx_buf[54 + 1024];

	int         (*ndev_close_fn)(ndev_handler_t *);
	void        (*ndev_pkt_handler)(u_char *user, const struct pcap_pkthdr *h,
                                   const u_char *bytes);

	ATOMIC_DECLARE(uint64_t, rank);

	uint32_t ul_flags;	
	uint64_t	nr_rx_pkts;
	uint64_t	nr_tx_pkts;
	uint64_t	nr_rx_bytes;
	uint64_t	nr_tx_bytes;
} vlib_ndev_t;

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

ORYX_DECLARE (
	int ndev_up (
		IN const char *iface
	)
);

ORYX_DECLARE (
	int ndev_exist (
		IN const char *iface
	)
);

ORYX_DECLARE (
	int ndev_is_running (
		IN const char *iface,
		IN struct ethtool_cmd *ethtool
	)
);

ORYX_DECLARE (
	int ndev_is_up (
		IN const char *iface
	)
);

ORYX_DECLARE (
	int ndev_open (
		IN vlib_ndev_t *ndev,
		IN void (*ndev_pkt_handler)(u_char *user,
						const struct pcap_pkthdr *h, const u_char *bytes),
		IN int (*ndev_close_fn)(ndev_handler_t *)
	)
);

ORYX_DECLARE (
	int ndev_down (
		IN const char *iface
	)
);

ORYX_DECLARE (
	void *ndev_capture (
		IN void *argv
	)
);


#endif

