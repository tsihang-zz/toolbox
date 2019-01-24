#include "oryx.h"
#include "netdev.h"

__oryx_always_extern__
int ndev_pcap_open
(
	IO ndev_handler_t **handler,
	IN char *name,
	IN int flags
)
{
    char errbuf[PCAP_ERRBUF_SIZE];
    struct bpf_program bpf;
    uint32_t ip, mask;
    int xret = 0;
    char filter[300] = {0};

	if(flags & NETDEV_OPEN_LIVE) {
		;	/** nothing to do. */
	}
	
    /** search the ethernet device */
    xret = pcap_lookupnet(name, &ip, &mask, errbuf);
    if (likely(xret < 0)){
		fprintf(stderr,
			"pcap_lookupnet error %s\n", name);
		exit(0);
    }

	(*handler) = pcap_open_live(name, 65535, 1, 500, errbuf);
	if(unlikely(!(*handler))){
		fprintf(stderr,
				"pcap_open_live error %s -> %s\n", name, errbuf);
		exit(0);
	 }

    xret = pcap_compile((*handler), &bpf, &filter[0], 1, mask);
    if (likely(xret < 0)){
		fprintf(stderr,
				"pcap_compile error\n");
		exit(0);
    }

    xret = pcap_setfilter((*handler), &bpf);
    if (likely(xret < 0)){
		fprintf(stderr,
				"pcap_setfilter error\n");
		exit(0);
    }

	fprintf(stdout, "Pcap open %s okay\n", name);
	return 0;
}

__oryx_always_extern__
int ndev_pcap_close(ndev_handler_t *handler)
{
	pcap_close(handler);
	return 0;
}

