#ifndef DP_DECODE_MARVELL_DSA_H
#define DP_DECODE_MARVELL_DSA_H

extern struct MarvellDSAMap dsa_to_phy_map_list[];
extern struct MarvellDSAMap phy_to_dsa_map_list[];

#define DSA_TO_PANEL_GE_ID(dsa)\
	dsa_to_phy_map_list[DSA_PORT(dsa)].p

#define DSA_TO_GLOBAL_PORT_ID(dsa)\
	(DSA_TO_PANEL_GE_ID((dsa)) + SW_PORT_OFFSET)

#define PANEL_GE_ID_TO_DSA(pid)\
	phy_to_dsa_map_list[pid].p

static __oryx_always_inline__
void PrintDSA(const char *comment, uint32_t cpu_dsa, uint8_t rx_tx)
{
	fprintf (stdout, "=================== %s ===================\n", comment);
	fprintf (stdout, "%12s%8x\n", "dsa:",    cpu_dsa);
	fprintf (stdout, "%12s%4d\n", "cmd:",	DSA_CMD(cpu_dsa));
	fprintf (stdout, "%12s%4d\n", "dev:",	DSA_DEV(cpu_dsa));
	fprintf (stdout, "%12s%4d\n", "port:",	DSA_PORT(cpu_dsa));
	fprintf (stdout, "%12s%4d\n", "pri:",	DSA_PRI(cpu_dsa));
	fprintf (stdout, "%12s%4d\n", "extend:",	DSA_EXTEND(cpu_dsa));
	fprintf (stdout, "%12s%4d\n", "R1:",		DSA_R1(cpu_dsa));
	fprintf (stdout, "%12s%4d\n", "R2:",		DSA_R2(cpu_dsa));
	fprintf (stdout, "%12s%4d\n", "vlan:",	DSA_VLAN(cpu_dsa));
	fprintf (stdout, "%12s%4d\n", rx_tx == QUA_RX ? "fm_ge" : "to_ge",
									DSA_TO_PANEL_GE_ID(cpu_dsa));
}

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
static __oryx_always_inline__
int DecodeMarvellDSA0(threadvar_ctx_t *tv, decode_threadvar_ctx_t *dtv, packet_t *p, uint8_t *pkt, uint16_t len, pq_t *pq)
{
	MarvellDSAHdr *dsah;
	MarvellDSAEthernetHdr *dsaeth;

	oryx_logd("Marvell DSA ...");

	oryx_counter_inc(&tv->perf_private_ctx0, dtv->counter_dsa);
	if (unlikely(len < ETHERNET_DSA_HEADER_LEN)) {
		ENGINE_SET_INVALID_EVENT(p, ETHERNET_PKT_TOO_SMALL);
		return TM_ECODE_FAILED;
	}

	dsaeth = (MarvellDSAEthernetHdr *)&pkt[0];
	dsah = p->dsah = (MarvellDSAHdr *)&pkt[12];
	if (unlikely(p->dsah == NULL))
		return TM_ECODE_FAILED;

	p->dsa = __ntoh32__(dsah->dsa);

	//PrintDSA("RX", p->dsa, QUA_RX);
	//SET_PKT_RX_PORT(p, dsa_to_phy_map_list[DSA_PORT(p->dsa) % DIM(dsa_to_phy_map_list)].p);

	oryx_logd("dsa %08x, ether_type %04x", p->dsa, __ntoh16__(dsaeth->eth_type));

	switch (__ntoh16__(dsaeth->eth_type)) {
			case ETHERNET_TYPE_IP:
				DecodeIPv40(tv, dtv, p, pkt + ETHERNET_DSA_HEADER_LEN,
						   len - ETHERNET_DSA_HEADER_LEN, pq);
				break;
			case ETHERNET_TYPE_IPv6:
				DecodeIPv60(tv, dtv, p, pkt + ETHERNET_DSA_HEADER_LEN,
						   len - ETHERNET_DSA_HEADER_LEN, pq);
				break;
			case ETHERNET_TYPE_PPPOE_SESS:
				DecodePPPoESession0(tv, dtv, p, pkt + ETHERNET_DSA_HEADER_LEN,
								   len - ETHERNET_DSA_HEADER_LEN, pq);
				break;
			case ETHERNET_TYPE_PPPOE_DISC:
				DecodePPPoEDiscovery0(tv, dtv, p, pkt + ETHERNET_DSA_HEADER_LEN,
									 len - ETHERNET_DSA_HEADER_LEN, pq);
				break;
			case ETHERNET_TYPE_VLAN:
			case ETHERNET_TYPE_8021QINQ:
				DecodeVLAN0(tv, dtv, p, pkt + ETHERNET_DSA_HEADER_LEN,
									 len - ETHERNET_DSA_HEADER_LEN, pq);
				break;
			case ETHERNET_TYPE_MPLS_UNICAST:
			case ETHERNET_TYPE_MPLS_MULTICAST:
				DecodeMPLS0(tv, dtv, p, pkt + ETHERNET_DSA_HEADER_LEN,
						   len - ETHERNET_DSA_HEADER_LEN, pq);
				break;
			case ETHERNET_TYPE_ARP:
				DecodeARP0(tv, dtv, p, pkt + ETHERNET_DSA_HEADER_LEN,
					 len - ETHERNET_DSA_HEADER_LEN, pq);
				break;
			default:
		#if defined(BUILD_DEBUG)
				oryx_loge(-1, "p %p pkt %p dsa %08x ether_type %04x not supported", p,
						   pkt, p->dsa, __ntoh16__(dsaeth->eth_type));
				dump_pkt(GET_PKT(p), GET_PKT_LEN(p));
		#endif
				ENGINE_SET_INVALID_EVENT(p, ETHERNET_PKT_NOT_SUPPORTED);
				break;
	}

	return 0;
}

#endif

