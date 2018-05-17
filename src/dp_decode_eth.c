#include "oryx.h"
#include "dp_decode.h"

int DecodeEthernet0 (ThreadVars *tv, DecodeThreadVars *dtv, Packet *p,
                   uint8_t *pkt, uint16_t len, PacketQueue *pq)
{
	EthernetHdr *ethh;

	oryx_logd("Ethernet ...");

    oryx_counter_inc(&tv->perf_private_ctx0, dtv->counter_eth);
    if (unlikely(len < ETHERNET_HEADER_LEN)) {
        ENGINE_SET_INVALID_EVENT(p, ETHERNET_PKT_TOO_SMALL);
        return TM_ECODE_FAILED;
    }

    ethh = p->ethh = (EthernetHdr *)pkt;
    if (unlikely(p->ethh == NULL))
        return TM_ECODE_FAILED;

	oryx_logd("p %p pkt %p ether type %04x", p, pkt, ntohs(ethh->eth_type));

	switch (ntohs(ethh->eth_type)) {
			case ETHERNET_TYPE_IP:
				DecodeIPv40(tv, dtv, p, pkt + ETHERNET_HEADER_LEN,
						   len - ETHERNET_HEADER_LEN, pq);
				break;
			case ETHERNET_TYPE_IPV6:
				DecodeIPv60(tv, dtv, p, pkt + ETHERNET_HEADER_LEN,
						   len - ETHERNET_HEADER_LEN, pq);
				break;
			case ETHERNET_TYPE_PPPOE_SESS:
				DecodePPPOESession0(tv, dtv, p, pkt + ETHERNET_HEADER_LEN,
								   len - ETHERNET_HEADER_LEN, pq);
				break;
			case ETHERNET_TYPE_PPPOE_DISC:
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
			case ETHERNET_TYPE_ARP:
				DecodeARP0(tv, dtv, p, pkt + ETHERNET_HEADER_LEN,
					 len - ETHERNET_HEADER_LEN, pq);
				break;
			default:
				oryx_logn("p %p pkt %p ether type %04x not supported", p,
						   pkt, ntohs(ethh->eth_type));
	}

	
	return 0;
}

