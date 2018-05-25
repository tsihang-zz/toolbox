#ifndef DP_DECODE_MARVELL_DSA_H
#define DP_DECODE_MARVELL_DSA_H

extern struct MarvellDSAMap dsa_to_phy_map_list[];
extern struct MarvellDSAMap phy_to_dsa_map_list[];

static inline void PrintDSA(const char *comment, uint32_t dsa, u8 rx_tx)
{
	printf ("=================== %s ===================\n", comment);
	printf ("%12s%4d\n", "cmd:",	DSA_CMD(dsa));
	printf ("%12s%4d\n", "dev:",	DSA_SRC_DEV(dsa));
	printf ("%12s%4d\n", "port:",	DSA_SRC_PORT(dsa));
	printf ("%12s%4d\n", "pri:",	DSA_PRI(dsa));
	printf ("%12s%4d\n", "extend:",	DSA_EXTEND(dsa));
	printf ("%12s%4d\n", "R1:",		DSA_R1(dsa));
	printf ("%12s%4d\n", "R2:",		DSA_R2(dsa));
	printf ("%12s%4d\n", "vlan:",	DSA_VLAN(dsa));
	printf ("%12s%4d\n", rx_tx == QUA_RX ? "fm_phy" : "to_phy",
									dsa_to_phy_map_list[DSA_SRC_PORT(dsa)].p);
}

typedef struct MarvellDSAEthernetHdr_ {
    MarvellDSAHdr dsah;
    uint16_t eth_type;
} __attribute__((__packed__)) MarvellDSAEthernetHdr;


/**
 * \internal
 * \brief this function is used to decode Marvell DSA packets
 *
 * \param tv pointer to the thread vars
 * \param dtv pointer code thread vars
 * \param p pointer to the packet struct
 * \param pkt pointer to the raw packet
 * \param len packet len
 * \param pq pointer to the packet queue
 *
 */
static inline int __oryx_hot__
DecodeMarvellDSA0(ThreadVars *tv, DecodeThreadVars *dtv, Packet *p, uint8_t *pkt, uint16_t len, PacketQueue *pq)
{
	MarvellDSAHdr *dsah;
	MarvellDSAEthernetHdr *dsaeth;

	oryx_logd("Marvell DSA ...");

	oryx_counter_inc(&tv->perf_private_ctx0, dtv->counter_dsa);
	if (unlikely(len < DSA_HEADER_LEN)) {
		ENGINE_SET_INVALID_EVENT(p, ETHERNET_PKT_TOO_SMALL);
		return TM_ECODE_FAILED;
	}

	dsaeth = (MarvellDSAEthernetHdr *)&pkt[0];
	dsah = p->dsah = (MarvellDSAHdr *)&pkt[0];
	if (unlikely(p->dsah == NULL))
		return TM_ECODE_FAILED;

	p->dsa = ntoh32(p->dsah->dsa);

	//PrintDSA("RX", p->dsa, QUA_RX);
	//SET_PKT_SRC_PHY(p, dsa_to_phy_map_list[DSA_SRC_PORT(p->dsa) % DIM(dsa_to_phy_map_list)].p);

	oryx_logd("ether type %04x", ntoh16(dsaeth->eth_type));

	switch (ntoh16(dsaeth->eth_type)) {
			case ETHERNET_TYPE_IP:
				DecodeIPv40(tv, dtv, p, pkt + DSA_HEADER_LEN,
						   len - DSA_HEADER_LEN, pq);
				break;
			case ETHERNET_TYPE_IPV6:
				DecodeIPv60(tv, dtv, p, pkt + DSA_HEADER_LEN,
						   len - DSA_HEADER_LEN, pq);
				break;
			case ETHERNET_TYPE_PPPOE_SESS:
				DecodePPPoESession0(tv, dtv, p, pkt + DSA_HEADER_LEN,
								   len - DSA_HEADER_LEN, pq);
				break;
			case ETHERNET_TYPE_PPPOE_DISC:
				DecodePPPoEDiscovery0(tv, dtv, p, pkt + DSA_HEADER_LEN,
									 len - DSA_HEADER_LEN, pq);
				break;
			case ETHERNET_TYPE_VLAN:
			case ETHERNET_TYPE_8021QINQ:
				DecodeVLAN0(tv, dtv, p, pkt + DSA_HEADER_LEN,
									 len - DSA_HEADER_LEN, pq);
				break;
			case ETHERNET_TYPE_MPLS_UNICAST:
			case ETHERNET_TYPE_MPLS_MULTICAST:
				DecodeMPLS0(tv, dtv, p, pkt + DSA_HEADER_LEN,
						   len - DSA_HEADER_LEN, pq);
				break;
			case ETHERNET_TYPE_ARP:
				DecodeARP0(tv, dtv, p, pkt + DSA_HEADER_LEN,
					 len - DSA_HEADER_LEN, pq);
				break;
			default:
				oryx_loge(-1, "p %p pkt %p ether_type %04x not supported", p,
						   pkt, ntohs(dsaeth->eth_type));
				ENGINE_SET_INVALID_EVENT(p, ETHERNET_PKT_NOT_SUPPORTED);
				dump_pkt(GET_PKT(p), GET_PKT_LEN(p));
	}

	return 0;
}

#endif

