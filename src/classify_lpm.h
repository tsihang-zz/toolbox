#ifndef LPM_H
#define LPM_H

/** longest prefix match. */


static __oryx_always_inline__
uint8_t lpm_get_ipv4_dst_port(void *ipv4_hdr,  void *dpdk_port, void *lookup_struct)
{
	uint32_t next_hop;
	struct rte_lpm *ipv4_l3fwd_lookup_struct =
		(struct rte_lpm *)lookup_struct;

	return (uint8_t) ((rte_lpm_lookup(ipv4_l3fwd_lookup_struct,
		rte_be_to_cpu_32(((struct ipv4_hdr *)ipv4_hdr)->dst_addr),
		&next_hop) == 0) ? next_hop : RTE_MAX_ETHPORTS);
}


static __oryx_always_inline__
uint8_t lpm_get_ipv6_dst_port(void *ipv6_hdr,  void *dpdk_port, void *lookup_struct)
{
	uint32_t next_hop;
	struct rte_lpm6 *ipv6_l3fwd_lookup_struct =
		(struct rte_lpm6 *)lookup_struct;

	return (uint8_t) ((rte_lpm6_lookup(ipv6_l3fwd_lookup_struct,
			((struct ipv6_hdr *)ipv6_hdr)->dst_addr,
			&next_hop) == 0) ?  next_hop : RTE_MAX_ETHPORTS);
}

void classify_setup_lpm(const int socketid);

#endif
