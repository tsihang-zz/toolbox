#include "oryx.h"
#include "dp_decode.h"
#include "dp_flow.h"
#include "dp_decode_tcp.h"
#include "dp_decode_udp.h"
#include "dp_decode_sctp.h"
#include "dp_decode_gre.h"
#include "dp_decode_icmpv4.h"
#include "dp_decode_icmpv6.h"
#include "dp_decode_ipv4.h"
#include "dp_decode_ipv6.h"
#include "dp_decode_pppoe.h"
#include "dp_decode_mpls.h"
#include "dp_decode_arp.h"
#include "dp_decode_vlan.h"
#include "dp_decode_marvell_dsa.h"
#include "dp_decode_eth.h"

#include "iface_private.h"

#include "dp_route.h"

/* Configure how many packets ahead to prefetch, when reading packets */
#define PREFETCH_OFFSET	  3

extern volatile bool force_quit;
extern ThreadVars g_tv[];
extern DecodeThreadVars g_dtv[];
extern PacketQueue g_pq[];

static rte_xmm_t mask0;
static rte_xmm_t mask1;
static rte_xmm_t mask2;

#if defined(RTE_MACHINE_CPUFLAG_SSE4_2) || defined(RTE_MACHINE_CPUFLAG_CRC32)
#define EM_HASH_CRC 1
#endif

struct rte_hash *ipv4_l3fwd_em_lookup_struct[NB_SOCKETS];
struct rte_hash *ipv6_l3fwd_em_lookup_struct[NB_SOCKETS];

static inline uint32_t
ipv4_hash_crc(const void *data, __rte_unused uint32_t data_len,
		uint32_t init_val)
{
	const union ipv4_5tuple_host *k;
	uint32_t t;
	const uint32_t *p;

	k = data;
	t = k->proto;
	p = (const uint32_t *)&k->port_src;

#ifdef EM_HASH_CRC
	init_val = rte_hash_crc_4byte(t, init_val);
	init_val = rte_hash_crc_4byte(k->ip_src, init_val);
	init_val = rte_hash_crc_4byte(k->ip_dst, init_val);
	init_val = rte_hash_crc_4byte(*p, init_val);
#else
	init_val = rte_jhash_1word(t, init_val);
	init_val = rte_jhash_1word(k->ip_src, init_val);
	init_val = rte_jhash_1word(k->ip_dst, init_val);
	init_val = rte_jhash_1word(*p, init_val);
#endif

	return init_val;
}

static inline uint32_t
ipv6_hash_crc(const void *data, __rte_unused uint32_t data_len,
		uint32_t init_val)
{
	const union ipv6_5tuple_host *k;
	uint32_t t;
	const uint32_t *p;
#ifdef EM_HASH_CRC
	const uint32_t	*ip_src0, *ip_src1, *ip_src2, *ip_src3;
	const uint32_t	*ip_dst0, *ip_dst1, *ip_dst2, *ip_dst3;
#endif

	k = data;
	t = k->proto;
	p = (const uint32_t *)&k->port_src;

#ifdef EM_HASH_CRC
	ip_src0 = (const uint32_t *) k->ip_src;
	ip_src1 = (const uint32_t *)(k->ip_src+4);
	ip_src2 = (const uint32_t *)(k->ip_src+8);
	ip_src3 = (const uint32_t *)(k->ip_src+12);
	ip_dst0 = (const uint32_t *) k->ip_dst;
	ip_dst1 = (const uint32_t *)(k->ip_dst+4);
	ip_dst2 = (const uint32_t *)(k->ip_dst+8);
	ip_dst3 = (const uint32_t *)(k->ip_dst+12);
	init_val = rte_hash_crc_4byte(t, init_val);
	init_val = rte_hash_crc_4byte(*ip_src0, init_val);
	init_val = rte_hash_crc_4byte(*ip_src1, init_val);
	init_val = rte_hash_crc_4byte(*ip_src2, init_val);
	init_val = rte_hash_crc_4byte(*ip_src3, init_val);
	init_val = rte_hash_crc_4byte(*ip_dst0, init_val);
	init_val = rte_hash_crc_4byte(*ip_dst1, init_val);
	init_val = rte_hash_crc_4byte(*ip_dst2, init_val);
	init_val = rte_hash_crc_4byte(*ip_dst3, init_val);
	init_val = rte_hash_crc_4byte(*p, init_val);
#else
	init_val = rte_jhash_1word(t, init_val);
	init_val = rte_jhash(k->ip_src,
			sizeof(uint8_t) * IPV6_ADDR_LEN, init_val);
	init_val = rte_jhash(k->ip_dst,
			sizeof(uint8_t) * IPV6_ADDR_LEN, init_val);
	init_val = rte_jhash_1word(*p, init_val);
#endif
	return init_val;
}

#define BYTE_VALUE_MAX 256
#define ALL_32_BITS 0xffffffff
#define BIT_8_TO_15 0x0000ff00
#define BIT_16_TO_23 0x00ff0000
/* Used only in exact match mode. */
int ipv6; /**< ipv6 is false by default. */
uint32_t hash_entry_number = HASH_ENTRY_NUMBER_DEFAULT;

static inline void
populate_ipv4_few_flow_into_table(const struct rte_hash *h)
{
	uint32_t i;
	int32_t ret;

	mask0 = (rte_xmm_t){.u32 = {BIT_8_TO_15, ALL_32_BITS,
				ALL_32_BITS, ALL_32_BITS} };
}

static inline void
populate_ipv6_few_flow_into_table(const struct rte_hash *h)
{
	uint32_t i;
	int32_t ret;

	mask1 = (rte_xmm_t){.u32 = {BIT_16_TO_23, ALL_32_BITS,
				ALL_32_BITS, ALL_32_BITS} };

	mask2 = (rte_xmm_t){.u32 = {ALL_32_BITS, ALL_32_BITS, 0, 0} };
}

static inline void
populate_ipv4_many_flow_into_table(const struct rte_hash *h,
		unsigned int nr_flow)
{
	unsigned i;

	mask0 = (rte_xmm_t){.u32 = {BIT_8_TO_15, ALL_32_BITS,
				ALL_32_BITS, ALL_32_BITS} };
}

static inline void
populate_ipv6_many_flow_into_table(const struct rte_hash *h,
		unsigned int nr_flow)
{
	unsigned i;

	mask1 = (rte_xmm_t){.u32 = {BIT_16_TO_23, ALL_32_BITS,
				ALL_32_BITS, ALL_32_BITS} };
	mask2 = (rte_xmm_t){.u32 = {ALL_32_BITS, ALL_32_BITS, 0, 0} };
}
/*
 * Initialize exact match (hash) parameters.
 */
void
setup_hash(const int socketid)
{
	struct rte_hash_parameters ipv4_l3fwd_hash_params = {
		.name = NULL,
		.entries = L3FWD_HASH_ENTRIES,
		.key_len = sizeof(union ipv4_5tuple_host),
		.hash_func = ipv4_hash_crc,
		.hash_func_init_val = 0,
	};

	struct rte_hash_parameters ipv6_l3fwd_hash_params = {
		.name = NULL,
		.entries = L3FWD_HASH_ENTRIES,
		.key_len = sizeof(union ipv6_5tuple_host),
		.hash_func = ipv6_hash_crc,
		.hash_func_init_val = 0,
	};

	char s[64];

	/* create ipv4 hash */
	snprintf(s, sizeof(s), "ipv4_l3fwd_hash_%d", socketid);
	ipv4_l3fwd_hash_params.name = s;
	ipv4_l3fwd_hash_params.socket_id = socketid;
	ipv4_l3fwd_em_lookup_struct[socketid] =
		rte_hash_create(&ipv4_l3fwd_hash_params);
	if (ipv4_l3fwd_em_lookup_struct[socketid] == NULL)
		rte_exit(EXIT_FAILURE,
			"Unable to create the l3fwd hash on socket %d\n",
			socketid);

	/* create ipv6 hash */
	snprintf(s, sizeof(s), "ipv6_l3fwd_hash_%d", socketid);
	ipv6_l3fwd_hash_params.name = s;
	ipv6_l3fwd_hash_params.socket_id = socketid;
	ipv6_l3fwd_em_lookup_struct[socketid] =
		rte_hash_create(&ipv6_l3fwd_hash_params);
	if (ipv6_l3fwd_em_lookup_struct[socketid] == NULL)
		rte_exit(EXIT_FAILURE,
			"Unable to create the l3fwd hash on socket %d\n",
			socketid);
#if 1
	if (hash_entry_number != HASH_ENTRY_NUMBER_DEFAULT) {
		/* For testing hash matching with a large number of flows we
		 * generate millions of IP 5-tuples with an incremented dst
		 * address to initialize the hash table. */
		if (ipv6 == 0) {
			/* populate the ipv4 hash */
			populate_ipv4_many_flow_into_table(
				ipv4_l3fwd_em_lookup_struct[socketid],
				hash_entry_number);
		} else {
			/* populate the ipv6 hash */
			populate_ipv6_many_flow_into_table(
				ipv6_l3fwd_em_lookup_struct[socketid],
				hash_entry_number);
		}
	} else {
		/*
		 * Use data in ipv4/ipv6 l3fwd lookup table
		 * directly to initialize the hash table.
		 */
		if (ipv6 == 0) {
			/* populate the ipv4 hash */
			populate_ipv4_few_flow_into_table(
				ipv4_l3fwd_em_lookup_struct[socketid]);
		} else {
			/* populate the ipv6 hash */
			populate_ipv6_few_flow_into_table(
				ipv6_l3fwd_em_lookup_struct[socketid]);
		}
	}
#endif
}


#if defined(HAVE_DPDK_BUILT_IN_PARSER)
static __oryx_always_inline__
void rss_parse_ptype0(struct rte_mbuf *m, struct lcore_conf *lconf)
{
	struct ether_hdr *eth_hdr;
	uint32_t packet_type = RTE_PTYPE_UNKNOWN;
	uint16_t ether_type;
	void *l3;
	int hdr_len;
	struct ipv4_hdr *ipv4h;
	struct ipv6_hdr *ipv6h;
	ThreadVars *tv = &g_tv[lconf->lcore_id];
	DecodeThreadVars *dtv = &g_dtv[lconf->lcore_id];

	oryx_counter_inc(&tv->perf_private_ctx0, dtv->counter_pkts);
	oryx_counter_add(&tv->perf_private_ctx0, dtv->counter_bytes, m->pkt_len);
	
	eth_hdr = rte_pktmbuf_mtod(m, struct ether_hdr *);

	oryx_counter_inc(&tv->perf_private_ctx0, dtv->counter_eth);

	ether_type = eth_hdr->ether_type;
	l3 = (uint8_t *)eth_hdr + sizeof(struct ether_hdr);
	if (ether_type == rte_cpu_to_be_16(ETHER_TYPE_IPv4)) {
		ipv4h = (struct ipv4_hdr *)l3;
		hdr_len = (ipv4h->version_ihl & IPV4_HDR_IHL_MASK) *
			  IPV4_IHL_MULTIPLIER;
		oryx_counter_inc(&tv->perf_private_ctx0, dtv->counter_ipv4);
		if (hdr_len == sizeof(struct ipv4_hdr)) {
			packet_type |= RTE_PTYPE_L3_IPV4;
			if (ipv4h->next_proto_id == IPPROTO_TCP){
				oryx_counter_inc(&tv->perf_private_ctx0, dtv->counter_tcp);
				packet_type |= RTE_PTYPE_L4_TCP;
			}
			else if (ipv4h->next_proto_id == IPPROTO_UDP){
				oryx_counter_inc(&tv->perf_private_ctx0, dtv->counter_udp);
				packet_type |= RTE_PTYPE_L4_UDP;
			}
			else if (ipv4h->next_proto_id == IPPROTO_SCTP){
				oryx_counter_inc(&tv->perf_private_ctx0, dtv->counter_sctp);
				packet_type |= RTE_PTYPE_L4_SCTP;
			}
		} else
			packet_type |= RTE_PTYPE_L3_IPV4_EXT;
	} else if (ether_type == rte_cpu_to_be_16(ETHER_TYPE_IPv6)) {
		ipv6h = (struct ipv6_hdr *)l3;
		if (ipv6h->proto == IPPROTO_TCP)
			packet_type |= RTE_PTYPE_L3_IPV6 | RTE_PTYPE_L4_TCP;
		else if (ipv6h->proto == IPPROTO_UDP)
			packet_type |= RTE_PTYPE_L3_IPV6 | RTE_PTYPE_L4_UDP;
		else if (ipv6h->proto == IPPROTO_SCTP)
			packet_type |= RTE_PTYPE_L3_IPV6 | RTE_PTYPE_L4_SCTP;
		else
			packet_type |= RTE_PTYPE_L3_IPV6_EXT_UNKNOWN;
	}

	m->packet_type = packet_type;
}

int
rss_check_ptype0(int portid)
{
  int i, ret;
  int ptype_l3_ipv4 = 0;
  int ptype_l3_ipv6 = 0;
  int ptype_l3_ipv4_ext = 0;
  int ptype_l3_ipv6_ext = 0;
  int ptype_l4_tcp = 0;
  int ptype_l4_udp = 0;
  uint32_t ptype_mask = RTE_PTYPE_L3_MASK | RTE_PTYPE_L4_MASK;

  ret = rte_eth_dev_get_supported_ptypes(portid, ptype_mask, NULL, 0);
  if (ret <= 0)
	  return 0;

  uint32_t ptypes[ret];

  ret = rte_eth_dev_get_supported_ptypes(portid, ptype_mask, ptypes, ret);
  for (i = 0; i < ret; ++i) {
	  switch (ptypes[i]) {
	  case RTE_PTYPE_L3_IPV4:
	  	  ptype_l3_ipv4 = 1;
	  	  break;
	  case RTE_PTYPE_L3_IPV6:
	  	  ptype_l3_ipv6 = 1;
	  	  break;
	  case RTE_PTYPE_L3_IPV4_EXT:
		  ptype_l3_ipv4_ext = 1;
		  break;
	  case RTE_PTYPE_L3_IPV6_EXT:
		  ptype_l3_ipv6_ext = 1;
		  break;
	  case RTE_PTYPE_L4_TCP:
		  ptype_l4_tcp = 1;
		  break;
	  case RTE_PTYPE_L4_UDP:
		  ptype_l4_udp = 1;
		  break;
	  }
  }

  if(ptype_l3_ipv4 == 0)
  	 oryx_logw(0, "port %d cannot parse RTE_PTYPE_L3_IPV4\n", portid);
  if(ptype_l3_ipv6 == 0)
  	 oryx_logw(0, "port %d cannot parse RTE_PTYPE_L3_IPV6\n", portid);
  if (ptype_l3_ipv4_ext == 0)
	  oryx_logw(0, "port %d cannot parse RTE_PTYPE_L3_IPV4_EXT\n", portid);
  if (ptype_l3_ipv6_ext == 0)
	  oryx_logw(0, "port %d cannot parse RTE_PTYPE_L3_IPV6_EXT\n", portid);
  if (!ptype_l3_ipv4_ext || !ptype_l3_ipv6_ext)
	  return 0;
  if (ptype_l4_tcp == 0)
	  oryx_logw(0, "port %d cannot parse RTE_PTYPE_L4_TCP\n", portid);
  if (ptype_l4_udp == 0)
	  oryx_logw(0, "port %d cannot parse RTE_PTYPE_L4_UDP\n", portid);
  if (ptype_l4_tcp && ptype_l4_udp)
	  return 1;

  return 0;
}


int
rss_check_ptype1(int portid)
{
  int i, ret;
  int ptype_l3_ipv4 = 0, ptype_l3_ipv6 = 0;
  uint32_t ptype_mask = RTE_PTYPE_L3_MASK;

  ret = rte_eth_dev_get_supported_ptypes(portid, ptype_mask, NULL, 0);
  if (ret <= 0)
	  return 0;

  uint32_t ptypes[ret];

  ret = rte_eth_dev_get_supported_ptypes(portid, ptype_mask, ptypes, ret);
  for (i = 0; i < ret; ++i) {
	  if (ptypes[i] & RTE_PTYPE_L3_IPV4)
		  ptype_l3_ipv4 = 1;
	  if (ptypes[i] & RTE_PTYPE_L3_IPV6)
		  ptype_l3_ipv6 = 1;
  }

  if (ptype_l3_ipv4 == 0)
	  printf("port %d cannot parse RTE_PTYPE_L3_IPV4\n", portid);

  if (ptype_l3_ipv6 == 0)
	  printf("port %d cannot parse RTE_PTYPE_L3_IPV6\n", portid);

  if (ptype_l3_ipv4 && ptype_l3_ipv6)
	  return 1;

  return 0;

}

static __oryx_always_inline__
void rss_parse_ptype1(struct rte_mbuf *m)
{
	struct ether_hdr *eth_hdr;
	uint32_t packet_type = RTE_PTYPE_UNKNOWN;
	uint16_t ether_type;

	eth_hdr = rte_pktmbuf_mtod(m, struct ether_hdr *);
	ether_type = eth_hdr->ether_type;
	if (ether_type == rte_cpu_to_be_16(ETHER_TYPE_IPv4))
		packet_type |= RTE_PTYPE_L3_IPV4_EXT_UNKNOWN;
	else if (ether_type == rte_cpu_to_be_16(ETHER_TYPE_IPv6))
		packet_type |= RTE_PTYPE_L3_IPV6_EXT_UNKNOWN;

	m->packet_type = packet_type;
}

uint16_t
rss_cb_parse_ptype0(uint8_t port __rte_unused, uint16_t queue __rte_unused,
		  struct rte_mbuf *pkts[], uint16_t nb_pkts,
		  uint16_t max_pkts __rte_unused,
		  void *user_param __rte_unused)
{
	unsigned i;
	struct lcore_conf *lconf = (struct lcore_conf*)user_param;
	struct iface_t *iface;
	vlib_port_main_t *vp = &vlib_port_main;

	iface_lookup_id(vp, port, &iface);

#if defined(BUILD_DEBUG)
	BUG_ON(iface == NULL);
	if(nb_pkts) {
		oryx_logd("port=%d, queue=%d, lcore=%d", port, queue, lconf->lcore_id);
	}
#endif

	if (iface_support_marvell_dsa(iface)) {
		/** Marvell DSA frame decode. */
	} else {
		for (i = 0; i < nb_pkts; ++i) {
			rss_parse_ptype0(pkts[i], lconf);	
		}
	}
	
	return nb_pkts;
}


static __oryx_always_inline__ uint16_t
rss_cb_parse_ptype1(uint8_t port __rte_unused, uint16_t queue __rte_unused,
		struct rte_mbuf *pkts[], uint16_t nb_pkts,
		uint16_t max_pkts __rte_unused,
		void *user_param __rte_unused)
{
  unsigned i;

  for (i = 0; i < nb_pkts; ++i)
	  rss_parse_ptype1(pkts[i]);

  return nb_pkts;
}
#endif


static __oryx_always_inline__
uint8_t em_get_ipv4_dst_port(void *ipv4h, void *lookup_struct)
{
	int ret = 0;
	union ipv4_5tuple_host key;
	struct rte_hash *ipv4_l3fwd_lookup_struct = (struct rte_hash *)lookup_struct;

#if defined(BUILD_DEBUG)
	BUG_ON(ipv4_l3fwd_lookup_struct == NULL);
#endif

	ipv4h = (uint8_t *)ipv4h + offsetof(struct ipv4_hdr, time_to_live);

	/*
	 * Get 5 tuple: dst port, src port, dst IP address,
	 * src IP address and protocol.
	 */
	key.xmm = em_mask_key(ipv4h, mask0.x);

#if defined(BUILD_DEBUG)
	oryx_logn("sip=%08x dip=%08x proto=%u sp=%u dp=%u",
			key.ip_src, key.ip_dst, key.proto, key.port_src, key.port_dst);
#endif

	/* Find destination port */
	//ret = rte_hash_lookup(ipv4_l3fwd_lookup_struct, (const void *)&key);
	//return (uint8_t)((ret < 0) ? 0 : ipv4_l3fwd_out_if[ret]);
	return 0;
}

void dp_dpdk_perf_tmr_handler(struct oryx_timer_t __oryx_unused__*tmr,
	int __oryx_unused__ argc, char **argv)
{	
	argv = argv;
}

/* Send burst of packets on an output interface */
static __oryx_always_inline__
int send_burst(ThreadVars *tv, struct lcore_conf *qconf, uint16_t n, uint8_t tx_port_id)
{
	struct rte_mbuf **m_table;
	int ret;
	uint16_t tx_queue_id, n_tx_pkts;
	struct iface_t *iface;
	vlib_port_main_t *vp = &vlib_port_main;

	/** port ID -> iface */
	iface_lookup_id(vp, tx_port_id, &iface);

	tx_queue_id = qconf->tx_queue_id[tx_port_id];
	m_table = (struct rte_mbuf **)qconf->tx_mbufs[tx_port_id].m_table;

	n_tx_pkts = ret = rte_eth_tx_burst(tx_port_id, tx_queue_id, m_table, n);
	if (unlikely(ret < n)) {
		do {
			rte_pktmbuf_free(m_table[ret]);
		} while (++ ret < n);
	}

#if defined(BUILD_DEBUG)
	oryx_logd("tx_port_id=%d lcore %d, tx_pkts %d", tx_port_id, tv->lcore, n_tx_pkts);
#endif

	iface_counters_add(iface, iface->if_counter_ctx->lcore_counter_pkts[QUA_COUNTER_TX][qconf->lcore_id], n_tx_pkts);

	return 0;
}

/* Enqueue a single packet, and send burst if queue is filled */
static __oryx_always_inline__
int send_single_packet(ThreadVars *tv, struct lcore_conf *qconf,
		struct rte_mbuf *m, uint8_t tx_port_id)
{
	uint16_t len;

	len = qconf->tx_mbufs[tx_port_id].len;
	qconf->tx_mbufs[tx_port_id].m_table[len] = m;
	len++;

	/* enough pkts to be sent */
	if (likely(len == DPDK_MAX_TX_BURST)) {
		send_burst(tv, qconf, DPDK_MAX_TX_BURST, tx_port_id);
		len = 0;
	}

	qconf->tx_mbufs[tx_port_id].len = len;
	return 0;
}

static __oryx_always_inline__
void simple_forward(ThreadVars *tv,
		struct rte_mbuf *m, struct iface_t *iface, struct lcore_conf *qconf)
{
	uint8_t tx_port_id = -1;
	Packet *p;
	struct ether_hdr *ethh;
	struct ipv4_hdr *ipv4h;
	uint8_t dst_port;
	uint32_t tcp_or_udp;
	uint32_t l3_ptypes;
	p = GET_MBUF_PRIVATE(Packet, m);

	ethh = rte_pktmbuf_mtod(m, struct ether_hdr *);
	tcp_or_udp = m->packet_type & (RTE_PTYPE_L4_TCP | RTE_PTYPE_L4_UDP);
	l3_ptypes = m->packet_type & RTE_PTYPE_L3_MASK;

	if (tcp_or_udp && (l3_ptypes == RTE_PTYPE_L3_IPV4)) {
		/* Handle IPv4 headers.*/
		ipv4h = rte_pktmbuf_mtod_offset(m, struct ipv4_hdr *,
						   sizeof(struct ether_hdr));
#if defined(DO_RFC_1812_CHECKS)
		/* Check to make sure the packet is valid (RFC1812) */
		if (is_valid_ipv4_pkt(ipv4h, m->pkt_len) < 0) {
			rte_pktmbuf_free(m);
			return;
		}
#endif

		/** fetch dst_port */
		tx_port_id = em_get_ipv4_dst_port(ipv4h, qconf->ipv4_lookup_struct);

#if defined(BUILD_DEBUG)
		struct udp_hdr *udph = rte_pktmbuf_mtod_offset(m, struct udp_hdr *,
						   (sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr)));		
		oryx_logd("rx_port_id %d sip=%08x dip=%08x proto=%02x sp=%u dp=%u\n",
				iface_id(iface), ipv4h->src_addr, ipv4h->dst_addr, ipv4h->next_proto_id, udph->src_port, udph->dst_port);
#endif

	}

	tx_port_id = GET_PKT_DST_PHY(p);
	send_single_packet(tv, qconf, m, tx_port_id);
}

/*
 * Buffer non-optimized handling of packets, invoked
 * from main_loop.
 */
static __oryx_always_inline__
void no_opt_send_packets(ThreadVars *tv,
			int nb_rx, struct rte_mbuf **pkts_burst,
			uint8_t rx_port_id, struct lcore_conf *qconf)
{
	int32_t j = 0;
	struct iface_t *iface;
	vlib_port_main_t *vp = &vlib_port_main;

	iface_lookup_id(vp, rx_port_id, &iface);

#if defined(BUILD_DEBUG)
	BUG_ON(iface == NULL);
#endif

#if 0
	/* Prefetch first packets */
	for (j = 0; j < PREFETCH_OFFSET && j < nb_rx; j++)
		rte_prefetch0(rte_pktmbuf_mtod(pkts_burst[j], void *));

	/*
	 * Prefetch and forward already prefetched
	 * packets.
	 */
	for (j = 0; j < (nb_rx - PREFETCH_OFFSET); j++) {
		rte_prefetch0(rte_pktmbuf_mtod(pkts_burst[
				j + PREFETCH_OFFSET], void *));
		simple_forward(tv, pkts_burst[j], iface, qconf);
	}
#endif

	/* Forward remaining prefetched packets */
	for (; j < nb_rx; j++)
		simple_forward(tv, pkts_burst[j], iface, qconf);
}

/* main processing loop */
int
main_loop(__attribute__((unused)) void *ptr_data)
{
	vlib_main_t *vm = (vlib_main_t *)ptr_data;
	struct rte_mbuf *pkts_burst[DPDK_MAX_RX_BURST], *m;
	unsigned lcore_id;
	uint64_t prev_tsc, diff_tsc, cur_tsc, timer_tsc;
	int i, j, nb_rx;
	uint8_t rx_port_id, tx_port_id, rx_queue_id, tx_queue_id;
	struct lcore_conf *qconf;
	ThreadVars *tv;
	struct iface_t *iface;
	vlib_port_main_t *vp = &vlib_port_main;
	int qua = QUA_COUNTER_RX;
	uint16_t pkt_len;
	uint8_t *pkt;
	Packet *p;
	
	const uint64_t drain_tsc = (rte_get_tsc_hz() + US_PER_S - 1) /
		US_PER_S * BURST_TX_DRAIN_US;
	
	prev_tsc = 0;

	lcore_id = rte_lcore_id();

	tv = &g_tv[lcore_id];
	qconf = &lcore_conf[lcore_id];
	tv->lcore = lcore_id;

	if (qconf->n_rx_queue == 0) {
		oryx_logn("lcore %u has nothing to do\n", lcore_id);
		return 0;
	}

	while (!(vm->ul_flags & VLIB_DP_INITIALIZED)) {
		printf("lcore %d wait for dataplane ...\n", tv->lcore);
		sleep(1);
	}

	oryx_logn("entering main loop on lcore %u\n", lcore_id);

	for (i = 0; i < qconf->n_rx_queue; i++) {
		rx_port_id = qconf->rx_queue_list[i].port_id;
		rx_queue_id = qconf->rx_queue_list[i].queue_id;
		printf(" -- lcoreid=%u portid=%hhu rxqueueid=%hhu --\n",
			lcore_id, rx_port_id, rx_queue_id);
		fflush(stdout);
	}

	prev_tsc = 0;
	timer_tsc = 0;

	while (!force_quit) {

		cur_tsc = rte_rdtsc();

		/*
		 * TX burst queue drain
		 */
		diff_tsc = cur_tsc - prev_tsc;
		if (unlikely(diff_tsc > drain_tsc)) {

			for (i = 0; i < qconf->n_tx_port; ++i) {
				tx_port_id = qconf->tx_port_id[i];
				if (qconf->tx_mbufs[tx_port_id].len == 0)
					continue;
				send_burst(tv, qconf,
					qconf->tx_mbufs[tx_port_id].len,
					tx_port_id);
				qconf->tx_mbufs[tx_port_id].len = 0;
			}
			/* if timer is enabled */
			if (timer_period > 0) {

				/* advance the timer */
				timer_tsc += diff_tsc;

				/* if timer has reached its timeout */
				if (unlikely(timer_tsc >= timer_period)) {

					/* do this only on a sepcific core */
					if (lcore_id == rte_get_master_lcore()) {
						/* reset the timer */
						timer_tsc = 0;
						/** Master lcore is never called. */
					}
				}
			}

			prev_tsc = cur_tsc;
		}

		/*
		 * Read packet from RX queues
		 */
		for (i = 0; i < qconf->n_rx_queue; ++i) {
			rx_port_id = qconf->rx_queue_list[i].port_id;
			rx_queue_id = qconf->rx_queue_list[i].queue_id;
			nb_rx = rte_eth_rx_burst(rx_port_id, rx_queue_id, pkts_burst,
								DPDK_MAX_RX_BURST);
			if (nb_rx == 0)
				continue;

			/** port ID -> iface */
			iface_lookup_id(vp, rx_port_id, &iface);			
			iface_counters_add(iface, iface->if_counter_ctx->lcore_counter_pkts[QUA_COUNTER_RX][lcore_id], nb_rx);
			
			for (j = 0; j < nb_rx; j++) {
				m = pkts_burst[j];
				rte_prefetch0(rte_pktmbuf_mtod(m, void *));
				pkt = (uint8_t *)rte_pktmbuf_mtod(m, void *);
				pkt_len = m->pkt_len;
				/** it costs a lot when calling Packet from mbuf. */
				p = GET_MBUF_PRIVATE(Packet, m);
				SET_PKT_LEN(p, pkt_len);
				SET_PKT(p, pkt);
				SET_PKT_SRC_PHY(p, rx_port_id); 
				iface_counters_add(iface, iface->if_counter_ctx->lcore_counter_bytes[QUA_COUNTER_RX][lcore_id], pkt_len);
				/** which port this packet should be send out. */
				tx_port_id = 1;
				SET_PKT_DST_PHY(p, tx_port_id);
			}
			no_opt_send_packets(tv, nb_rx, pkts_burst, rx_port_id, qconf);
		}
	}

	return 0;
}

