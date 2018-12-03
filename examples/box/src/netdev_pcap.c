#include "oryx.h"
#include "netdev.h"

int netdev_pcap_open(dev_handler_t **handler, char *devname, int flags)
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
    xret = pcap_lookupnet(devname, &ip, &mask, errbuf);
    if (likely(xret < 0)){
		oryx_panic(-1,
			"pcap_lookupnet error %s", devname);
    }

	(*handler) = pcap_open_live(devname, 65535, 1, 500, errbuf);
	if(unlikely(!(*handler))){
		oryx_panic(-1,
				"pcap_open_live error %s -> %s\n", devname, errbuf);
	 }

    xret = pcap_compile((*handler), &bpf, &filter[0], 1, mask);
    if (likely(xret < 0)){
		oryx_panic(-1,
				"pcap_compile error\n");
    }

    xret = pcap_setfilter((*handler), &bpf);
    if (likely(xret < 0)){
		oryx_panic(-1,
				"pcap_setfilter error\n");
    }

	fprintf (stdout, "Pcap open %s okay\n", devname);
	return 0;
}

