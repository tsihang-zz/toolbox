#include "oryx.h"
#include "dp_decode.h"

typedef struct MarvellDSAEthernetHdr_ {
    MarvellDSAHdr dsah;
    uint16_t eth_type;
} __attribute__((__packed__)) MarvellDSAEthernetHdr;

/** a map for dsa.port_src -> phyical port. */
struct MarvellDSAMap dsa_to_phy_map_list[] = {
	{-1, "unused"},
	{ 5, "et1500 ge5"},
	{ 6, "et1500 ge6"},
	{ 7, "et1500 ge7"},
	{ 8, "et1500 ge8"},
	{ 1, "et1500 ge1"},
	{ 2, "et1500 ge2"},
	{ 3, "et1500 ge3"},
	{ 4, "et1500 ge4"}
};

/** a map for physical port -> dsa.port_src */
struct MarvellDSAMap phy_to_dsa_map_list[] = {
	{-1, "unused"},
	{ 5, "dsa.port_src 5"},
	{ 6, "dsa.port_src 6"},
	{ 7, "dsa.port_src 7"},
	{ 8, "dsa.port_src 8"},
	{ 1, "dsa.port_src 1"},
	{ 2, "dsa.port_src 2"},
	{ 3, "dsa.port_src 3"},
	{ 4, "dsa.port_src 4"}
};

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
int DecodeMarvellDSA0(ThreadVars *tv, DecodeThreadVars *dtv, Packet *p, uint8_t *pkt, uint16_t len, PacketQueue *pq)
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
	SET_PKT_SRC_PHY(p, dsa_to_phy_map_list[DSA_SRC_PORT(p->dsa) % DIM(dsa_to_phy_map_list)].p);

	oryx_logd("ether type %04x", ntoh16(dsaeth->eth_type));

	switch (ntohs(dsaeth->eth_type)) {
			case ETHERNET_TYPE_IP:
				DecodeIPv40(tv, dtv, p, pkt + DSA_HEADER_LEN,
						   len - DSA_HEADER_LEN, pq);
				break;
			case ETHERNET_TYPE_IPV6:
				DecodeIPv60(tv, dtv, p, pkt + DSA_HEADER_LEN,
						   len - DSA_HEADER_LEN, pq);
				break;
			case ETHERNET_TYPE_PPPOE_SESS:
				DecodePPPOESession0(tv, dtv, p, pkt + DSA_HEADER_LEN,
								   len - DSA_HEADER_LEN, pq);
				break;
			case ETHERNET_TYPE_PPPOE_DISC:
				DecodePPPOEDiscovery0(tv, dtv, p, pkt + DSA_HEADER_LEN,
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
			case ETHERNET_TYPE_DCE:
				if (unlikely(len < ETHERNET_DCE_HEADER_LEN)) {
					ENGINE_SET_INVALID_EVENT(p, DCE_PKT_TOO_SMALL);
				} else {
					DecodeEthernet0(tv, dtv, p, pkt + ETHERNET_DCE_HEADER_LEN,
						len - ETHERNET_DCE_HEADER_LEN, pq);
				}
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


