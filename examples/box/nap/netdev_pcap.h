#ifndef NETDEV_PCAP_H
#define NETDEV_PCAP_H

ORYX_DECLARE (
	int netdev_pcap_open (
		OUT dev_handler_t **handler,
		IN char *devname,
		IN int flags
	)
);


#endif

