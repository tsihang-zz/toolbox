#ifndef EM_H
#define EM_H

/** exact match */

extern struct rte_hash *ipv4_l3fwd_em_lookup_struct[];
extern struct rte_hash *ipv6_l3fwd_em_lookup_struct[];
extern uint32_t g_em_appl_to_map_lookup_table[];

extern rte_xmm_t mask0;
extern rte_xmm_t mask1;
extern rte_xmm_t mask2;


/* Hash parameters. */
#ifdef RTE_ARCH_64
/* default to 4 million hash entries (approx) */
#define EM_HASH_ENTRIES		(1024*1024*4)
#else
/* 32-bit has less address-space for hugepage memory, limit to 1M entries */
#define EM_HASH_ENTRIES		(1024*1024*1)
#endif

union ipv4_5tuple_host {
	struct {
		uint8_t  pad0;
		uint8_t  proto;
		uint16_t pad1;
		uint32_t ip_src;
		uint32_t ip_dst;
		uint16_t port_src;
		uint16_t port_dst;
	};
	xmm_t xmm;
};

#define XMM_NUM_IN_IPv6_5TUPLE 3

union ipv6_5tuple_host {
	struct {
		uint16_t pad0;
		uint8_t  proto;
		uint8_t  pad1;
		uint8_t  ip_src[IPv6_ADDR_LEN];
		uint8_t  ip_dst[IPv6_ADDR_LEN];
		uint16_t port_src;
		uint16_t port_dst;
		uint64_t reserve;
	};
	xmm_t xmm[XMM_NUM_IN_IPv6_5TUPLE];
};

struct em_route {
	union {
		struct ipv4_5tuple k4;
		struct ipv6_5tuple k6;
	}u;
	uint32_t id;/** map entry id is a traffic direction
					which has a set of in_ports and out_ports. */
};

#if defined(__aarch64__)
static __oryx_always_inline__
xmm_t em_mask_key(void *key, xmm_t mask)
{
	int32x4_t data = vld1q_s32((int32_t *)key);

	return vandq_s32(data, mask);
}
#endif

#if defined(__x86_64__)
static __oryx_always_inline__
xmm_t em_mask_key(void *key, xmm_t mask)
{
	__m128i data = _mm_loadu_si128((__m128i *)(key));
	return _mm_and_si128(data, mask);
}
#endif

static __oryx_always_inline__
uint32_t em_get_ipv4_dst_port(void *ipv4h)
{
	int ret = 0;
	union ipv4_5tuple_host key;
	struct rte_hash *ipv4_l3fwd_lookup_struct = (struct rte_hash *)ipv4_l3fwd_em_lookup_struct[0];

#if defined(BUILD_DEBUG)
	BUG_ON(ipv4_l3fwd_lookup_struct == NULL);
#endif

	ipv4h = (uint8_t *)ipv4h + offsetof(struct ipv4_hdr, time_to_live);

	/*
	 * Get 5 tuple: dst port, src port, dst IP address,
	 * src IP address and protocol.
	 */
	key.xmm = em_mask_key(ipv4h, mask0.x);

	/* Find destination port */
	ret = rte_hash_lookup(ipv4_l3fwd_lookup_struct, (const void *)&key);

#if defined(BUILD_DEBUG)
	oryx_logn("em[%d], sip=%08x dip=%08x proto=%u sp=%u dp=%u",
			ret,
			key.ip_src, key.ip_dst, key.proto, key.port_src, key.port_dst);
#endif

	return (uint32_t)((ret < 0) ? EM_HASH_ENTRIES : g_em_appl_to_map_lookup_table[ret]);
}


void classify_setup_em(const int socketid);

int em_add_hash_key(struct em_route *entry);

#endif

