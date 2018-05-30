#ifndef ROUTE_PRIVATE_H
#define ROUTE_PRIVATE_H

struct ipv4_5tuple {
	uint32_t ip_dst;
	uint32_t ip_src;
	uint16_t port_dst;
	uint16_t port_src;
	uint8_t  proto;
} __attribute__((__packed__));

#define IPV6_ADDR_LEN 16
struct ipv6_5tuple {
	uint8_t  ip_dst[IPV6_ADDR_LEN];
	uint8_t  ip_src[IPV6_ADDR_LEN];
	uint16_t port_dst;
	uint16_t port_src;
	uint8_t  proto;
} __attribute__((__packed__));

struct em_route_ipv4 {
	struct ipv4_5tuple key;
	void *map_entry;	/** map entry is a traffic direction
							which has a set of in_ports and out_ports. */
};

#endif
