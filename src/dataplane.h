#ifndef __DATAPLANE_H__
#define __DATAPLANE_H__

/** How to define a priv size within a packet before rte_pktmbuf_pool_create. */
#define ET1500_BUFFER_HDR_SIZE  (sizeof(struct Packet_) - ET1500_BUFFER_PRE_DATA_SIZE)

#if !defined(HAVE_SURICATA)
/** libpcap shows us the way to linktype codes
 * \todo we need more & maybe put them in a separate file? */
#define LINKTYPE_NULL       DLT_NULL
#define LINKTYPE_ETHERNET   DLT_EN10MB
#define LINKTYPE_LINUX_SLL  113
#define LINKTYPE_PPP        9
#define LINKTYPE_RAW        DLT_RAW
/* http://www.tcpdump.org/linktypes.html defines DLT_RAW as 101, yet others don't.
 * Libpcap on at least OpenBSD returns 101 as datalink type for RAW pcaps though. */
#define LINKTYPE_RAW2       101
#define PPP_OVER_GRE        11
#define VLAN_OVER_GRE       13
#endif

void dataplane_init (vlib_main_t *vm);


#endif
