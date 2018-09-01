#ifndef MARVELL_DSA_H_H
#define MARVELL_DSA_H_H

enum {
	DSA_TAG_TO_CPU,
	DSA_TAG_FROM_CPU,
	DSA_TAG_TO_SNIFFER,
	DSA_TAG_FORWARD
};


#define DSA_CMD_EGRESS      (0X1 /* DSA_TAG_FROM_CPU */)
#define DSA_CMD_INGRESS     (0X3 /* DSA_TAG_FORWARD  */)

typedef union MarvellDSAHdr_ {
	struct {
		uint32_t cmd		: 2;	/* [31:30]
									 * 0: TO_CPU_TAG
									 * 1: FROM_CPU_TAG
									 * 2: TO_SNIFFER_TAG
									 * 3: FORWARD_TAG */
		uint32_t T			: 1;	/* [29]
									 * 0: frame recieved(or to egress) untag
									 * 1: frame recieved(or to egress) tagged */	 
		uint32_t dev 		: 5;	/* [28:24] */
		uint32_t port		: 5;	/* [23:19] */
		uint32_t R2			: 1;	/* [18]
									 * R: 0 or 1.
									 * W: must be 0. */
		uint32_t R1			: 1;	/* [17] */			
		uint32_t C			: 1;	/* [16] Frame's CFI */
		
		uint32_t prio		: 3;	/* [15:13] */
		uint32_t R0			: 1;	/* [12] code=[R2:R1:R0] while cmd=TO_CPU_TAG */
		uint32_t vlan		: 12;	/* [11:00] */
		uint8_t	 eh[];				/* extend headr, 4 bytes. */
	};
	uint32_t dsa;
}__attribute__((__packed__)) MarvellDSAHdr;

#define u32_offset(uint32_t,off)	((uint32_t) >> off)
#define DSA_CMD(dsa)	  		(((dsa) >> 30) & 0x003)
#define DSA_TAG(dsa)  			(((dsa) >> 29) & 0x001)
#define DSA_DEV(dsa)			(((dsa) >> 24) & 0x01f)
#define DSA_PORT(dsa)			(((dsa) >> 19) & 0x01f)
#define DSA_R2(dsa)				(((dsa) >> 18) & 0x001)
#define DSA_R1(dsa)				(((dsa) >> 17) & 0x001)
#define DSA_CFI(dsa)			(((dsa) >> 16) & 0x001)
#define DSA_PRI(dsa) 			(((dsa) >> 13) & 0x007)
#define DSA_R0(dsa)		   		(((dsa) >> 12) & 0x001)
#define DSA_VLAN(dsa)  			(((dsa) >> 00) & 0xfff)

#define SET_DSA_CMD(dsa,cmd)	((dsa) |= (((cmd) & 0x003) << 30))
#define SET_DSA_TAG(dsa,tag)	((dsa) |= (((tag) & 0x001) << 29))
#define SET_DSA_DEV(dsa,dev)	((dsa) |= (((dev) & 0x01f) << 24))
#define SET_DSA_PORT(dsa, p)	((dsa) |= (((p)   & 0x01f) << 19))
#define SET_DSA_R2(dsa,   r)	((dsa) |= (((r)    & 0x001) << 18))
#define SET_DSA_R1(dsa,   r)	((dsa) |= (((r)    & 0x001) << 17))
#define SET_DSA_CFI(dsa,cfi)	((dsa) |= (((cfi) & 0x001) << 16))
#define SET_DSA_PRI(dsa,pri)	((dsa) |= (((pri) & 0x007) << 13))
#define SET_DSA_R0(dsa,   r)	((dsa) |= (((r)    & 0x001) << 12))
#define SET_DSA_VLAN(dsa, v)	((dsa) |= (((v)   & 0xfff) << 0))

#define DSA_EXTEND(dsa)			DSA_R0(dsa)

#define DSA_IS_TO_CPU(dsa)		(DSA_CMD((dsa)) == DSA_TAG_TO_CPU)
#define DSA_IS_FROM_CPU(dsa)	(DSA_CMD((dsa)) == DSA_TAG_FROM_CPU)
#define DSA_IS_TO_SNIFFER(dsa)	(DSA_CMD((dsa)) == DSA_TAG_TO_SNIFFER)
#define DSA_IS_FORWARD(dsa)		(DSA_CMD((dsa)) == DSA_TAG_FORWARD)
#define FRAME_IS_TAGGED(dsa)	(DSA_TAG((dsa)) == 1)
#define FRAME_IS_UNTAGGED(dsa)	(DSA_TAG((dsa)) == 0)

#define DSA_IS_INGRESS(dsa)		(DSA_CMD(dsa) == DSA_CMD_INGRESS)
#define DSA_IS_EGRESS(dsa)		(DSA_CMD(dsa) == DSA_CMD_EGRESS)

/** DSA extend header length */
#define DSA_EXTEND_HEADER_LEN 8

/** sizeof MarvellDSAHdr */
#define DSA_HEADER_LEN	4

/** DSA header length */
#define ETHERNET_DSA_HEADER_LEN (ETHERNET_HEADER_LEN + DSA_HEADER_LEN)

typedef struct MarvellDSAEthernetHdr_ {
    uint8_t eth_dst[6];
    uint8_t eth_src[6];
    MarvellDSAHdr dsah;
    uint16_t eth_type;
} __attribute__((__packed__)) MarvellDSAEthernetHdr;

#endif

