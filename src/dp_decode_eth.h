#ifndef DP_DECODE_ETHERNET_H
#define DP_DECODE_ETHERNET_H

static __oryx_always_inline__
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

   oryx_logd("[%llu] p %p pkt %p ether type %04x",
	   oryx_counter_get(&tv->perf_private_ctx0, dtv->counter_eth), p, pkt, ntoh16(ethh->eth_type));

   switch (ntoh16(ethh->eth_type)) {
		   case ETHERNET_TYPE_IP:
			   DecodeIPv40(tv, dtv, p, pkt + ETHERNET_HEADER_LEN,
						  len - ETHERNET_HEADER_LEN, pq);
			   break;
		   case ETHERNET_TYPE_IPV6:
			   DecodeIPv60(tv, dtv, p, pkt + ETHERNET_HEADER_LEN,
						  len - ETHERNET_HEADER_LEN, pq);
			   break;
		   case ETHERNET_TYPE_PPPOE_SESS:
			   DecodePPPoESession0(tv, dtv, p, pkt + ETHERNET_HEADER_LEN,
								  len - ETHERNET_HEADER_LEN, pq);
			   break;
		   case ETHERNET_TYPE_PPPOE_DISC:
			   DecodePPPoEDiscovery0(tv, dtv, p, pkt + ETHERNET_HEADER_LEN,
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
		#if 0
		   case ETHERNET_TYPE_DCE:
			   if (unlikely(len < ETHERNET_DCE_HEADER_LEN)) {
				   ENGINE_SET_INVALID_EVENT(p, DCE_PKT_TOO_SMALL);
			   } else {
				   DecodeEthernet0(tv, dtv, p, pkt + ETHERNET_DCE_HEADER_LEN,
					   len - ETHERNET_DCE_HEADER_LEN, pq);
			   }
			   break;
		#endif
		   case ETHERNET_TYPE_ARP:
			   DecodeARP0(tv, dtv, p, pkt + ETHERNET_HEADER_LEN,
					len - ETHERNET_HEADER_LEN, pq);
			   break;
		   default:    /** trying to decode Marvell DSA frame. */
			   DecodeMarvellDSA0(tv, dtv, p, pkt + 12, len - 12, pq);
   }

   return 0;
}

#endif
