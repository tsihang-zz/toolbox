#ifndef NETDEV_PCAP_H
#define NETDEV_PCAP_H

ORYX_DECLARE (
	int ndev_pcap_open (
		IO ndev_handler_t **handler,
		IN char *name,
		IN int flags
	)
);

ORYX_DECLARE (
	int ndev_pcap_close (
		IO ndev_handler_t *handler
	)
);


#endif

