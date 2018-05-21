#ifndef MARVELL_DSA_H_H
#define MARVELL_DSA_H_H

typedef union MarvellDSAHdr_ {
	struct {
		uint32_t cmd		: 2;	/* [31:30] */
		uint32_t resv0		: 1;	/* [29] */
		uint32_t dev_src 	: 5;	/* [28:24] */
		uint32_t port_src	: 5;	/* [23:19] */
		uint32_t resv1		: 3;	/* [18:16] */
		uint32_t usr_prio	: 3;	/* [15:13] */
		uint32_t extend		: 1;	/* [12] */
		uint32_t vlan_id	: 12;	/* [11:00] */
		uint8_t	 eh[];				/* extend headr, 4 bytes. */
	};
	uint32_t dsa;
}__attribute__((__packed__)) MarvellDSAHdr;

#define DSA_CMD(dsa)	  (((dsa) >> 30) & 0x3)
#define DSA_DEV_SRC(dsa)  (((dsa) >> 24) & 0x1ff)
#define DSA_PORT_SRC(dsa) (((dsa) >> 19) & 0x1ff)
#define DSA_USR_PRIO(dsa) (((dsa) >> 13) & 0x7)
#define DSA_EXTEND(dsa)   (((dsa) >> 12) & 0x1)
#define DSA_VLAN_ID(dsa)  (((dsa) >> 0)  & 0xfff)


/** DSA header length */
#define DSA_HEADER_LEN (4 + 2 /** eth_type*/)

/** DSA extend header length */
#define DSA_EXTEND_HEADER_LEN 8


#endif

