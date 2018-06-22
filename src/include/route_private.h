#ifndef ROUTE_PRIVATE_H
#define ROUTE_PRIVATE_H

/** THIS IS a FIXED structure for EM  LPM and ACL. DO NOT CHANGE. */
/* BIG ENDIAN */
struct ipv4_5tuple {
	uint32_t ip_dst;
	uint32_t ip_src;
	uint16_t port_dst;
	uint16_t port_src;
	uint8_t  proto;
	union {
		uint16_t  rx_port_id: 4;
		uint16_t  vlan: 12;
		uint16_t  pad0;
	}u;
} __attribute__((__packed__));

#define IPv6_ADDR_LEN 16
struct ipv6_5tuple {
	uint8_t  ip_dst[IPv6_ADDR_LEN];
	uint8_t  ip_src[IPv6_ADDR_LEN];
	uint16_t port_dst;
	uint16_t port_src;
	uint8_t  proto;
	union {
		uint16_t  rx_port_id: 4;
		uint16_t  vlan: 12;
		uint16_t  pad0;
	}u;
} __attribute__((__packed__));


#endif
