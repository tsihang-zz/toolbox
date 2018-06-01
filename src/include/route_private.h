#ifndef ROUTE_PRIVATE_H
#define ROUTE_PRIVATE_H

/** THIS IS a FIXED structure for EM  LPM and ACL. DO NOT CHANGE. */

struct ipv4_5tuple {
	uint32_t ip_dst;
	uint32_t ip_src;
	uint16_t port_dst;
	uint16_t port_src;
	uint8_t  proto;
	//uint8_t  rx_port_id;
	//uint16_t vlan_id;
} __attribute__((__packed__));

#define IPV6_ADDR_LEN 16
struct ipv6_5tuple {
	uint8_t  ip_dst[IPV6_ADDR_LEN];
	uint8_t  ip_src[IPV6_ADDR_LEN];
	uint16_t port_dst;
	uint16_t port_src;
	uint8_t  proto;
	//uint8_t  rx_port_id;
	//uint16_t vlan_id;
} __attribute__((__packed__));


#endif
