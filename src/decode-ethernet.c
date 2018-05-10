#include "oryx.h"

#include "suricata-common.h"
#include "decode.h"
#include "decode-events.h"
#include "util-unittest.h"
#include "util-debug.h"

#include "decode-ethernet.h"
#include "decode-vlan.h"
#include "decode-mpls.h"
#include "decode-ipv4.h"
#include "decode-ipv6.h"
#include "decode-pppoe.h"

int DecodeEthernet0 (ThreadVars *tv, DecodeThreadVars *dtv, Packet *p,
                   uint8_t *pkt, uint16_t len, PacketQueue *pq)
{
#if defined(HAVE_STATS_COUNTERS)
    StatsIncr(tv, dtv->counter_eth);
#endif

    if (unlikely(len < ETHERNET_HEADER_LEN)) {
        ENGINE_SET_INVALID_EVENT(p, ETHERNET_PKT_TOO_SMALL);
        return TM_ECODE_FAILED;
    }

    p->ethh = (EthernetHdr *)pkt;
    if (unlikely(p->ethh == NULL))
        return TM_ECODE_FAILED;

	SCLogDebug("p %p pkt %p ether type %04x", p, pkt, ntohs(p->ethh->eth_type));

	switch (ntohs(p->ethh->eth_type)) {
			case ETHERNET_TYPE_IP:
				SCLogDebug("DecodeEthernet ip4");
				DecodeIPV40(tv, dtv, p, pkt + ETHERNET_HEADER_LEN,
						   len - ETHERNET_HEADER_LEN, pq);
				break;
			case ETHERNET_TYPE_IPV6:
				SCLogDebug("DecodeEthernet ip6");

				DecodeIPV60(tv, dtv, p, pkt + ETHERNET_HEADER_LEN,
						   len - ETHERNET_HEADER_LEN, pq);
				break;
			case ETHERNET_TYPE_PPPOE_SESS:
				SCLogDebug("DecodeEthernet PPPOE Session\n");
				DecodePPPOESession0(tv, dtv, p, pkt + ETHERNET_HEADER_LEN,
								   len - ETHERNET_HEADER_LEN, pq);
				break;
			case ETHERNET_TYPE_PPPOE_DISC:
				SCLogDebug("DecodeEthernet PPPOE Discovery\n");
				DecodePPPOEDiscovery0(tv, dtv, p, pkt + ETHERNET_HEADER_LEN,
									 len - ETHERNET_HEADER_LEN, pq);
				break;
			case ETHERNET_TYPE_VLAN:
			case ETHERNET_TYPE_8021QINQ:
				DecodeVLAN0(tv, dtv, p, pkt + ETHERNET_HEADER_LEN,
									 len - ETHERNET_HEADER_LEN, pq);
				break;
			case ETHERNET_TYPE_MPLS_UNICAST:
			case ETHERNET_TYPE_MPLS_MULTICAST:
				DecodeMPLS0(tv, dtv, p, pkt + ETHERNET_HEADER_LEN,
						   len - ETHERNET_HEADER_LEN, pq);
				break;
			case ETHERNET_TYPE_DCE:
				if (unlikely(len < ETHERNET_DCE_HEADER_LEN)) {
					ENGINE_SET_INVALID_EVENT(p, DCE_PKT_TOO_SMALL);
				} else {
					DecodeEthernet0(tv, dtv, p, pkt + ETHERNET_DCE_HEADER_LEN,
						len - ETHERNET_DCE_HEADER_LEN, pq);
				}
				break;

			default:
				SCLogNotice("p %p pkt %p ether type %04x not supported", p,
						   pkt, ntohs(p->ethh->eth_type));
	}

	
	return 0;
}

