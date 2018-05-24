#ifndef DP_DECODE_MARVELL_DSA_H
#define DP_DECODE_MARVELL_DSA_H

struct MarvellDSAMap {
	u8 p;
	const char *comment;
};

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


int DecodeMarvellDSA0(ThreadVars *tv, DecodeThreadVars *dtv, Packet *p, uint8_t *pkt, uint16_t len, PacketQueue *pq);

#endif

