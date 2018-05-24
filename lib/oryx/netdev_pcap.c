#include "oryx.h"
#include "netdev.h"

int netdev_pcap_open(dev_handler_t **handler, char *devname, int flags)
{
    char errbuf[PCAP_ERRBUF_SIZE];
    struct bpf_program bpf;
    bpf_u_int32 ip, mask;
    int xret = 0;
    char filter[300] = {0};

	if(flags & NETDEV_OPEN_LIVE) {
		;	/** nothing to do. */
	}
	
    /** search the ethernet device */
    xret = pcap_lookupnet(devname, &ip, &mask, errbuf);
    if (likely(xret < 0)){
        oryx_loge(-1,
				"pcap_lookupnet error %s\n", devname);
		exit(0);
    }

	(*handler) = pcap_open_live(devname, 65535, 1, 500, errbuf);
	if(unlikely(!(*handler))){
		oryx_loge (-1,
				"pcap_open_live error %s -> %s\n", devname, errbuf);
		exit(0);
	 }

    xret = pcap_compile((*handler), &bpf, &filter[0], 1, mask);
    if (likely(xret < 0)){
        oryx_loge (-1,
				"pcap_compile error\n");
		exit(0);
    }

    xret = pcap_setfilter((*handler), &bpf);
    if (likely(xret < 0)){
        oryx_loge (-1,
				"pcap_setfilter error\n");
		exit(0);
    }

	printf ("Pcap open %s okay\n", devname);
	return 0;
}

