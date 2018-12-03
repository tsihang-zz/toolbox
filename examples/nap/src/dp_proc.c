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
#include "dp_decode_http.h"

#include "iface_private.h"
#include "map_private.h"
#include "appl_private.h"

#include "dpdk_classify.h"
#include "util_map.h"

#include "mpm.h"

/* Configure how many packets ahead to prefetch, when reading packets */
#define PREFETCH_OFFSET	  3
#define tx_panel_port_is_online(tx_panel_port_id)\
	((tx_panel_port_id) != UINT32_MAX)

extern volatile bool force_quit;
extern threadvar_ctx_t g_tv[];
extern decode_threadvar_ctx_t g_dtv[];
extern pq_t g_pq[];

#if defined(HAVE_MPM)
extern mpm_ctx_t mpm_ctx;
extern mpm_threadctx_t mpm_thread_ctx;
extern PrefilterRuleStore pmq;
#endif

static __oryx_always_inline__
void dsa_tag_strip(struct rte_mbuf *m)
{
	void *new_start;
	
	/** hold the source and destination MAC address. */
	struct ether_hdr *ethh
		 = rte_pktmbuf_mtod(m, struct ether_hdr *);

	/** remove len bytes at the beginning of an mbuf to get a new start. */
	new_start = rte_pktmbuf_adj(m, sizeof(struct vlan_hdr));
	
	/* refill with 2 MAC address. */
	memmove(new_start, ethh, 2 * ETHER_ADDR_LEN);
}

static __oryx_always_inline__
void act_packet_trim(struct rte_mbuf *m, uint16_t slice_size)
{
	if (slice_size > m->pkt_len)
		return;
	/* packet will always be 64 bytes when drop_size = m->pkt_len - 64. */
	rte_pktmbuf_trim(m, m->pkt_len - slice_size);
}

static __oryx_always_inline__
void act_packet_timestamp(struct rte_mbuf *m)
{
	m  = m;
}

static __oryx_always_inline__
int dsa_tag_insert(struct rte_mbuf **m, uint32_t new_cpu_dsa)
{
	struct ether_hdr *oh;
	MarvellDSAEthernetHdr *nh;

	/* Can't insert header if mbuf is shared */
	if (rte_mbuf_refcnt_read(*m) > 1) {
		struct rte_mbuf *copy;

		copy = rte_pktmbuf_clone(*m, (*m)->pool);
		if (unlikely(copy == NULL))
			return -ENOMEM;
		rte_pktmbuf_free(*m);
		*m = copy;
	}

	oh = rte_pktmbuf_mtod(*m, struct ether_hdr *);
	nh = (MarvellDSAEthernetHdr *)
		rte_pktmbuf_prepend(*m, sizeof(MarvellDSAHdr));
	if (nh == NULL)
		return -ENOSPC;

	memmove(nh, oh, 2 * ETHER_ADDR_LEN);
	nh->dsah.dsa = rte_cpu_to_be_32(new_cpu_dsa);
	
	return 0;
}


static __oryx_always_inline__
uint32_t dsa_tag_update(uint32_t cpu_tag, uint8_t tx_virtual_port_id)
{
	uint32_t new_cpu_dsa = 0; /** this packet received from a panel sw virtual port ,
	                           * and also send to a panel sw virtual port, here just update the dsa tag. */

	SET_DSA_CMD(new_cpu_dsa, DSA_CMD_EGRESS); 	/** which port this packet should be send out. */
	SET_DSA_PORT(new_cpu_dsa, tx_virtual_port_id);  /** update dsa.port */
	SET_DSA_CFI(new_cpu_dsa, DSA_CFI(cpu_tag));	/** update dsa.C dsa.R1, dsa.R2 */
	SET_DSA_R1(new_cpu_dsa, DSA_R1(cpu_tag));
	SET_DSA_PRI(new_cpu_dsa, DSA_PRI(cpu_tag));
	SET_DSA_R0(new_cpu_dsa, DSA_R0(cpu_tag));
	SET_DSA_VLAN(new_cpu_dsa, DSA_VLAN(cpu_tag));

#if defined(BUILD_DEBUG)
	PrintDSA("Tx", new_cpu_dsa, QUA_TX);
#endif
	return new_cpu_dsa;
}

static __oryx_always_inline__
uint32_t ipv4_hash_crc(const void *data, __rte_unused uint32_t data_len,
		uint32_t init_val)
{
	uint32_t		t;
	const uint32_t		*p;
	const union ipv4_5tuple_host *k;

	k = data;
	t = k->proto;
	p = (const uint32_t *)&k->port_src;

	init_val = rte_hash_crc_4byte(t, init_val);
	init_val = rte_hash_crc_4byte(k->ip_src, init_val);
	init_val = rte_hash_crc_4byte(k->ip_dst, init_val);
	init_val = rte_hash_crc_4byte(*p, init_val);

	return init_val;
}

static __oryx_always_inline__
void dp_load_frame_key4(union ipv4_5tuple_host *key, struct rte_mbuf *pkt, uint32_t ipoff)
{
	void *iph;

	iph = rte_pktmbuf_mtod_offset(pkt, struct ipv4_hdr *, ipoff);
	iph = (uint8_t *)iph + offsetof(struct ipv4_hdr, time_to_live);
	/*
	 * Get 5 tuple: dst port, src port, dst IP address,
	 * src IP address and protocol.
	 */
	key->xmm = em_mask_key(iph, mask0.x);
}

static __oryx_always_inline__
void dp_dump_packet_key4(union ipv4_5tuple_host *key, struct rte_mbuf *pkt)
{
	char s[16];
	oryx_logn ("%16s%08d", "packet_type: ", pkt->packet_type);
	oryx_logn ("%16s%s", "ip_src: ",		inet_ntop(AF_INET, &key->ip_src, s, 16));
	oryx_logn ("%16s%s", "ip_dst: ",		inet_ntop(AF_INET, &key->ip_dst, s, 16));
	oryx_logn ("%16s%04d", "port_src: ",	__ntoh16__(key->port_src));
	oryx_logn ("%16s%04d", "port_dst: ",	__ntoh16__(key->port_dst));
	oryx_logn ("%16s%08d", "protocol: ",	key->proto);
	oryx_logn ("%16s%08u", "RSS: ", 		pkt->hash.rss);
}

static __oryx_always_inline__
uint32_t dp_recalc_rss(struct rte_mbuf *m)
{
	void *ipv4h;
	union ipv4_5tuple_host key;

	ipv4h = rte_pktmbuf_mtod_offset(m, struct ipv4_hdr *, sizeof(MarvellDSAEthernetHdr));
	ipv4h = (uint8_t *)ipv4h + offsetof(struct ipv4_hdr, time_to_live);
	/*
	 * Get 5 tuple: dst port, src port, dst IP address,
	 * src IP address and protocol.
	 */
	key.xmm = em_mask_key(ipv4h, mask0.x);

	return ipv4_hash_crc(&key, sizeof(key), 0);
}


static __oryx_always_inline__
int dp_decide_tx_port(struct map_t *map, struct iface_t *rx_iface,
		struct rte_mbuf *m, uint32_t *tx_port_id)
{
	uint32_t rss;
	uint32_t index;
	uint32_t tx_panel_port_id = 0;

	if(map->nb_online_tx_panel_ports == 0) {
		return UINT32_MAX;
	}
	
	rss = iface_id(rx_iface) == SW_CPU_XAUI_PORT_ID ? dp_recalc_rss(m) : m->hash.rss;
	index = rss % map->nb_online_tx_panel_ports;

#if defined(BUILD_DEBUG)
	oryx_logn("%20s%8u", "m->hash.rss: ", m->hash.rss);
	oryx_logn("%20s%8u", "rss: ", rss);
	oryx_logn("%20s%8u", "index: ", index);
#endif

	*tx_port_id = tx_panel_port_id = map->online_tx_panel_ports[index];
	if(tx_panel_port_id != UINT32_MAX &&
		tx_panel_port_id > SW_PORT_OFFSET)
		*tx_port_id = SW_CPU_XAUI_PORT_ID;

	return tx_panel_port_id;
}

static __oryx_always_inline__
void dp_free_packet(threadvar_ctx_t *tv, decode_threadvar_ctx_t *dtv,
	struct rte_mbuf *pkt){
	rte_pktmbuf_free(pkt);
	oryx_counter_inc(&tv->perf_private_ctx0, dtv->counter_drop);
	tv->nr_mbufs_feedback ++;
}
	
static __oryx_always_inline__
void dp_drop_all(threadvar_ctx_t *tv, decode_threadvar_ctx_t *dtv,
	struct rte_mbuf **pkts_in, int nb){
	int i;
	for (i = 0; i < nb; i ++) {
		struct rte_mbuf *pkt = pkts_in[i];
		dp_free_packet(tv, dtv, pkt);
	}
}
	
/* Send burst of packets on an output interface */
static __oryx_always_inline__
int dp_send_burst(threadvar_ctx_t *tv, struct lcore_conf *qconf, uint16_t n, uint32_t tx_port_id)
{
	int 		ret;
	uint16_t		tx_queue_id;
	uint16_t		nb_tx_pkts;
	struct iface_t		*tx_iface = NULL;
	struct rte_mbuf 	**m_table;
	vlib_iface_main_t	*pm = &vlib_iface_main;

	/** port ID -> iface */
	iface_lookup_id(pm, tx_port_id, &tx_iface);

	tx_queue_id = qconf->tx_queue_id[tx_port_id];
	m_table = (struct rte_mbuf **)qconf->tx_mbufs[tx_port_id].m_table;

	nb_tx_pkts = ret = rte_eth_tx_burst((uint8_t)tx_port_id, tx_queue_id, m_table, n);
	if (unlikely(ret < n)) {
		do {
			rte_pktmbuf_free(m_table[ret]);
		} while (++ ret < n);
	}

#if defined(BUILD_DEBUG)
	oryx_logd("tx_port_id=%d tx_queue_id %d, tx_lcore_id %d tx_pkts_num %d", tx_port_id, tx_queue_id, tv->lcore, nb_tx_pkts);
#endif

	iface_counters_add(tx_iface, tx_iface->if_counter_ctx->lcore_counter_pkts[QUA_TX][qconf->lcore_id], nb_tx_pkts);

	tv->nr_mbufs_feedback += n;

	return 0;
}

/* Enqueue a single packet, and send burst if queue is filled */
static __oryx_always_inline__
int dp_send_single_packet(threadvar_ctx_t *tv, decode_threadvar_ctx_t *dtv,
		struct iface_t *rx_iface, struct lcore_conf *qconf, 
		struct rte_mbuf *m, uint32_t tx_port_id, uint8_t tx_panel_port_id)
{
	uint16_t		len;
	uint32_t		cpu_dsa = 0;
	uint32_t		tx_panel_ge_id = 0;
	struct iface_t		*tx_panel_iface = NULL;
	vlib_iface_main_t	*pm = &vlib_iface_main;
	
	if (iface_id(rx_iface) == SW_CPU_XAUI_PORT_ID) {
		packet_t *p = GET_MBUF_PRIVATE(packet_t, m);
		if(tx_panel_port_id > SW_PORT_OFFSET) {
			/* GE ---> GE */
			tx_panel_ge_id = tx_panel_port_id - SW_PORT_OFFSET;
			cpu_dsa = dsa_tag_update(p->dsa, PANEL_GE_ID_TO_DSA(tx_panel_ge_id));
			MarvellDSAEthernetHdr *dsaeth = rte_pktmbuf_mtod(m, MarvellDSAEthernetHdr *);
			dsaeth->dsah.dsa = rte_cpu_to_be_32(cpu_dsa);
	#if defined(BUILD_DEBUG)
			BUG_ON(tx_port_id != SW_CPU_XAUI_PORT_ID);
			oryx_logn ("update dsa tag %08x -> %08x", p->dsa, cpu_dsa);
	#endif
			goto flush2_buffer;
		} else {
			/* GE ---> XE */
			dsa_tag_strip(m);
	#if defined(BUILD_DEBUG)
			BUG_ON(tx_port_id == SW_CPU_XAUI_PORT_ID);
			oryx_logn ("strip dsa tag %08x", p->dsa);
	#endif
			goto flush2_buffer;
		}

	} else {
		if (tx_panel_port_id > SW_PORT_OFFSET) {
			/* XE ---> GE */
			tx_panel_ge_id = tx_panel_port_id - SW_PORT_OFFSET;
			/* add a dsa tag */
			cpu_dsa = dsa_tag_update(0, PANEL_GE_ID_TO_DSA(tx_panel_ge_id));
			dsa_tag_insert(&m, cpu_dsa);

	#if defined(BUILD_DEBUG)
			BUG_ON(tx_port_id != SW_CPU_XAUI_PORT_ID);
			oryx_logn ("insert dsa tag %08x", cpu_dsa);
	#endif
			goto flush2_buffer;
		} else {
			/** XE -> XE */
		#if defined(BUILD_DEBUG)
				BUG_ON(tx_port_id == SW_CPU_XAUI_PORT_ID);
		#endif
		}
	}

flush2_buffer:

	if (tx_panel_port_id > SW_PORT_OFFSET) {
		iface_lookup_id(pm, tx_panel_port_id, &tx_panel_iface);
		iface_counter_update(tx_panel_iface, 1, m->pkt_len, QUA_TX, tv->lcore);
	}
	
	len = qconf->tx_mbufs[tx_port_id].len;
	qconf->tx_mbufs[tx_port_id].m_table[len] = m;
	len++;
	
#if defined(BUILD_DEBUG)
	oryx_logn ("		buffering pkt (%p, pkt_len=%d @lcore=%d) for tx_port_id %d",
				m, m->pkt_len, tv->lcore, tx_port_id);
#endif

	/* enough pkts to be sent */
	if (likely(len == DPDK_MAX_TX_BURST)) {
		dp_send_burst(tv, qconf, DPDK_MAX_TX_BURST, tx_port_id);
		len = 0;
	}
	qconf->tx_mbufs[tx_port_id].len = len;
	
	return 0;
}

#if defined(HAVE_MPM)
int http_match(mpm_ctx_t *mpm_ctx, mpm_threadctx_t *mpm_thread_ctx, PrefilterRuleStore *pmq, struct http_keyval_t *v)
{
	return mpm_pattern_search(mpm_ctx, mpm_thread_ctx, pmq,
				(uint8_t *)&v->val[0], v->valen);
}
#endif

static __oryx_always_inline__
void dp_parse_http(threadvar_ctx_t *tv, decode_threadvar_ctx_t *dtv,
			struct rte_mbuf *pkt, size_t *nr_off, union ipv4_5tuple_host *k)
{
	/* HTTP acl is registered. */
	size_t nroff = (*nr_off);
	/* redirect to start of this layer */
	char *v = rte_pktmbuf_mtod_offset(pkt, char *, nroff);
	struct http_ctx_t ctx;

	if(__ntoh16__(k->port_src) == 80 || __ntoh16__(k->port_dst) == 80)
		return;
	oryx_counter_inc(&tv->perf_private_ctx0, dtv->counter_http);

	/* Not a wanted HTTP frame. */
	if(http_parse(&ctx, v, 0))
		return;
	
	if(ctx.ul_flags & HTTP_APPEAR_URI)
		__http_kv_dump("URI", &ctx.uri);
	if(ctx.ul_flags & HTTP_APPEAR_CONTENT_TYPE)
		__http_kv_dump("Content-Type", &ctx.ct);
}

static __oryx_always_inline__
void dp_parse_key4(threadvar_ctx_t *tv, decode_threadvar_ctx_t *dtv,
			struct rte_mbuf *pkt, size_t *nr_off, union ipv4_5tuple_host *k)
{
	size_t nroff = (*nr_off);
	
	if (k->proto == IPPROTO_TCP) {
		nroff += TCP_HEADER_LEN;	/* offset to application header skip TCP. */
		oryx_counter_inc(&tv->perf_private_ctx0, dtv->counter_tcp);
	}
	else if (k->proto == IPPROTO_SCTP) {
		nroff += SCTP_HEADER_LEN;	/* offset to application header skip SCTP. */
		oryx_counter_inc(&tv->perf_private_ctx0, dtv->counter_sctp);
	}
	else if (k->proto == IPPROTO_UDP) {
		nroff += UDP_HEADER_LEN;	/* offset to application header skip UDP. */
		oryx_counter_inc(&tv->perf_private_ctx0, dtv->counter_udp);
	}
	else {
		return;
	}

	(*nr_off) = nroff;
}

static __oryx_always_inline__
void dp_classify_prepare_one_packet(threadvar_ctx_t *tv, decode_threadvar_ctx_t *dtv,
	struct iface_t *rx_iface, struct rte_mbuf **pkts_in, struct parser_ctx_t *parser, int index)
{
	packet_t			*p;
	vlib_iface_main_t	*pm = &vlib_iface_main;
	struct rte_mbuf 	*pkt = pkts_in[index];
	struct iface_t		*rx_panel_iface = NULL;
	
	p = GET_MBUF_PRIVATE(packet_t, pkt);

	/* ethernet type. */
	uint16_t ether_type;

	/* offset of every header. */
	size_t nroff = ETHERNET_HEADER_LEN;

	size_t pkt_len = 0;
	uint8_t rx_panel_port_id;
	const bool dsa_support = iface_support_marvell_dsa(rx_iface);

#if 1
	if (dsa_support) {
		/* change default if we get a dsa frame. */
		nroff = ETHERNET_DSA_HEADER_LEN;
		
		MarvellDSAEthernetHdr *dsaeth = rte_pktmbuf_mtod(pkt, MarvellDSAEthernetHdr *);
		p->dsa = rte_be_to_cpu_32(dsaeth->dsah.dsa);
		ether_type = dsaeth->eth_type;
		rx_panel_port_id = DSA_TO_GLOBAL_PORT_ID(p->dsa);

#if defined(BUILD_DEBUG)
		oryx_logn("%20s%4d", "rx_panel_port_id: ",	rx_panel_port_id);
		oryx_logn("%20s%4x", "eth_type: ",			rte_be_to_cpu_16(ether_type));
		PrintDSA("RX", p->dsa, QUA_RX);
		dump_pkt((uint8_t *)rte_pktmbuf_mtod(pkt, void *), pkt->pkt_len);
#endif		
		/* DSA is invalid. */
		if (!DSA_IS_INGRESS(p->dsa)) {
			dp_free_packet(tv, dtv, pkt);
			return;
		}
		/* Rx statistics on an ethernet GE port. */
		iface_lookup_id(pm, rx_panel_port_id, &rx_panel_iface);
		iface_counter_update(rx_panel_iface, 1, pkt->pkt_len, QUA_RX, tv->lcore);
	} else {
		EthernetHdr *eth = rte_pktmbuf_mtod(pkt, EthernetHdr *);
		ether_type = eth->eth_type;
	}

	if (ether_type == rte_cpu_to_be_16(ETHERNET_TYPE_IP)) {
		union ipv4_5tuple_host k4;
		/* load IPv4 key. */
		dp_load_frame_key4(&k4, pkt, nroff);
#if defined(BUILD_DEBUG)
		dp_dump_packet_key4(&k4, pkt);
#endif
		/* IPv4 frame statistics. */
		oryx_counter_inc(&tv->perf_private_ctx0, dtv->counter_ipv4);
		nroff += IPv4_HEADER_LEN;		/* start from IPv4 header to offset of the next layer header. */
		parser->data_ipv4[parser->num_ipv4]	= dsa_support ? DSA_MBUF_IPv4_2PROTO(pkt) : MBUF_IPv4_2PROTO(pkt);
		parser->m_ipv4[parser->num_ipv4]		= pkt;
		parser->num_ipv4 ++;
		dp_parse_key4(tv, dtv, pkt, &nroff, &k4);
#if defined(HAVE_MPM)
		dp_parse_http(tv, dtv, pkt, &nroff, &k4);
#endif
	} else if (ether_type == rte_cpu_to_be_16(ETHERNET_TYPE_IPv6)) {
		/* IPv6 frame statistics. */
		oryx_counter_inc(&tv->perf_private_ctx0, dtv->counter_ipv6);
		nroff += IPv6_HEADER_LEN;
		/* Fill acl structure */
		parser->data_ipv6[parser->num_ipv6]	= dsa_support ? DSA_MBUF_IPv6_2PROTO(pkt) : MBUF_IPv6_2PROTO(pkt);
		parser->m_ipv6[parser->num_ipv6]		= pkt;
		parser->num_ipv6 ++;
	}  else {
		if (ether_type == rte_cpu_to_be_16(ETHERNET_TYPE_ARP))
			oryx_counter_inc(&tv->perf_private_ctx0, dtv->counter_arp);
		parser->m_notip[(parser->num_notip)++] = pkt;
		/* Unknown type, a simplified drop for the packet */
		dp_free_packet(tv, dtv, pkt);
		return;
	}

#else
	if (iface_support_marvell_dsa(rx_iface)) {
		uint16_t ether_type;
		uint8_t rx_panel_port_id;
		MarvellDSAEthernetHdr *dsaeth = rte_pktmbuf_mtod(pkt, MarvellDSAEthernetHdr *);
		
		ether_type = dsaeth->eth_type;
		p->dsa = rte_be_to_cpu_32(dsaeth->dsah.dsa);
		rx_panel_port_id = DSA_TO_GLOBAL_PORT_ID(p->dsa);

#if defined(BUILD_DEBUG)
		oryx_logn("%20s%4d", "rx_panel_port_id: ",	rx_panel_port_id);
		oryx_logn("%20s%4x", "eth_type: ",			rte_be_to_cpu_16(ether_type));
		PrintDSA("RX", p->dsa, QUA_RX);
		dump_pkt((uint8_t *)rte_pktmbuf_mtod(pkt, void *), pkt->pkt_len);
#endif

		/* Rx statistics on an ethernet GE port. */
		if (rx_panel_port_id > SW_PORT_OFFSET) {
			iface_lookup_id(pm, rx_panel_port_id, &rx_panel_iface);
			iface_counter_update(rx_panel_iface, 1, pkt->pkt_len, QUA_RX, tv->lcore);
		}

		/* DSA is invalid. */
		if (!DSA_IS_INGRESS(p->dsa)) {
			dp_free_packet(tv, dtv, pkt);
			return;
		}
		
		if (ether_type == rte_cpu_to_be_16(ETHERNET_TYPE_IP)) {
			/* Fill acl structure */
			parser->data_ipv4[parser->num_ipv4]	= DSA_MBUF_IPv4_2PROTO(pkt);
			parser->m_ipv4[parser->num_ipv4]		= pkt;
			parser->num_ipv4 ++;
	#if defined(BUILD_DEBUG)
			dp_dump_packet_key4(pkt, sizeof(MarvellDSAEthernetHdr));
	#endif
		} else if (ether_type == rte_cpu_to_be_16(ETHERNET_TYPE_IPv6)) {
			/* Fill acl structure */
			parser->data_ipv6[parser->num_ipv6]	= DSA_MBUF_IPv6_2PROTO(pkt);
			parser->m_ipv6[parser->num_ipv6]		= pkt;
			parser->num_ipv6 ++;
		} else {
			parser->m_notip[(parser->num_notip)++] = pkt;
			/* Unknown type, a simplified drop for the packet */
			dp_free_packet(tv, dtv, pkt);
			return;
		}
	} else {

		if (RTE_ETH_IS_IPV4_HDR(pkt->packet_type)) {
			/* Fill acl structure */
			parser->data_ipv4[parser->num_ipv4]	= MBUF_IPv4_2PROTO(pkt);
			parser->m_ipv4[parser->num_ipv4]		= pkt;
			parser->num_ipv4 ++;
		
		#if defined(BUILD_DEBUG)
			dp_dump_packet_key4(pkt, sizeof(struct ether_hdr));
		#endif
		} 
		else if (RTE_ETH_IS_IPV6_HDR(pkt->packet_type)) {
			/* Fill acl structure */
			parser->data_ipv6[parser->num_ipv6]	= MBUF_IPv6_2PROTO(pkt);
			parser->m_ipv6[parser->num_ipv6]		= pkt;
			parser->num_ipv6 ++;
		}
		else {
			parser->m_notip[(parser->num_notip)++] = pkt;
			/* Unknown type, a simplified drop for the packet */
			dp_free_packet(tv, dtv, pkt);
			return;
		}
	}
#endif
}

static __oryx_always_inline__
void dp_classify_post(threadvar_ctx_t *tv, decode_threadvar_ctx_t *dtv,
		struct iface_t *rx_iface, struct lcore_conf *qconf, struct rte_mbuf *m, uint32_t res)
{
	int			i;
	uint32_t		mapid;
	uint32_t		appid;
	uint32_t		tx_panel_port_id = 0;
	uint32_t		tx_port_id = iface_id(rx_iface);
	struct appl_t		*appl = NULL;	
	struct map_t		*map = NULL;
	vlib_map_main_t		*mm = &vlib_map_main;
	vlib_appl_main_t	*am = &vlib_appl_main;
	
	if (likely(res == 0)) {
	#if defined(BUILD_DEBUG)
		oryx_logn("acl lookup error");
	#endif
		goto finish;
	}

	appid = __UNPACK_USERDATA(res);
	if(appid > MAX_APPLICATIONS) {
	#if defined(BUILD_DEBUG)
		oryx_logn("no such application %d", appid);
	#endif
		goto finish;
	}
	
	appl_entry_lookup_id(am, appid, &appl);
	if(appl_is_invalid(appl)) {
	#if defined(BUILD_DEBUG)
		oryx_logn("no such application %d", appid);
	#endif
		goto finish;
	}

	if(appl->nb_maps == 0) {
	#if defined(BUILD_DEBUG)
		oryx_logn("application(\"%s\") is not mapped.", appl_alias(appl));
	#endif
		goto finish;
	}

#if defined(BUILD_DEBUG)
	oryx_logn ("%18s%15s(id=%08d)", "hit_appl: ", appl_alias(appl), appid);
#endif

	appl->refcnt ++;

	/* What we want is that an ACL must not be in different maps. */
	for (i = 0; i < (int)appl->nb_maps; i ++) {
		
		struct appl_priv_t *priv = &appl->priv[i];
		mapid = priv->ul_map_id;

		/** lookup map table with userdata(map_mask) */
		map_entry_lookup_id(mm, mapid, &map);
		if (map_is_invalid(map)) {
			/* drop it defaultly if no map for this application */
		#if defined(BUILD_DEBUG)
			oryx_logn("Can not find an valid map by id %d for iface %s", mapid, iface_alias(rx_iface));
		#endif
			continue;
		}
		
	#if defined(BUILD_DEBUG)
		oryx_logn(" 	###### Map[%s] %d", map_alias(map), mapid);
		oryx_logn("		 rx_iface (%s)_%d is in map ? %s", iface_alias(rx_iface), iface_id(rx_iface),
					map_rx_has_iface(map, rx_iface) ? "yes" : "no");
		oryx_logn("		 tx_online_ports_num %d", map->nb_online_tx_panel_ports);
	#endif

		/* drop this packet ASAP, if this rx_iface is not mapped */
		if(!map_rx_has_iface(map, rx_iface)) {
			continue;
		}

	#if 0
		/* ACT_DROP has a highest priority than other actions.
		 * Here is a problem that when an ACL is hold by different map ??? */
		if (priv->ul_flags & ACT_DROP) {
			goto finish;
		}
	#endif
		tx_panel_port_id = dp_decide_tx_port(map, rx_iface, m, &tx_port_id);
	#if defined(BUILD_DEBUG)
		oryx_logn(" 	send to tx_panel_port_id %d, tx_port_id %d", tx_panel_port_id, tx_port_id);
	#endif
		if(tx_panel_port_is_online(tx_panel_port_id)) {
			
			if(priv->ul_flags & ACT_SLICE) {
				act_packet_trim(m, 64);
			}
			
			if(priv->ul_flags & ACT_TIMESTAMP) {
				act_packet_timestamp(m);
			}
			
			/* forward packets for this map. */
			dp_send_single_packet(tv, dtv, rx_iface, qconf, m, tx_port_id, tx_panel_port_id);
		}
	}
	
	/* everything is running okay, let's go back to the caller. */
	return;

finish:
	dp_free_packet(tv, dtv, m);
	return;
}

static __oryx_always_inline__
void dp_classify_prepare (threadvar_ctx_t *tv, decode_threadvar_ctx_t *dtv,
	struct iface_t *rx_iface, struct rte_mbuf **pkts_in, struct parser_ctx_t *parser, int nb_rx)
{
	int i;

	/* reset acl search context. */
	parser->num_ipv4	= 0;
	parser->num_ipv6	= 0;
	parser->num_notip	= 0;

	/* Prefetch first packets */
	for (i = 0; i < PREFETCH_OFFSET && i < nb_rx; i++) {
		rte_prefetch0(rte_pktmbuf_mtod(
				pkts_in[i], void *));
	}

	for (i = 0; i < (nb_rx - PREFETCH_OFFSET); i++) {
		rte_prefetch0(rte_pktmbuf_mtod(pkts_in[
				i + PREFETCH_OFFSET], void *));
		dp_classify_prepare_one_packet(tv, dtv, rx_iface, pkts_in, parser, i);
	}

	/* Process left packets */
	for (; i < nb_rx; i++) {
		dp_classify_prepare_one_packet(tv, dtv, rx_iface, pkts_in, parser, i);
	}
}

static __oryx_always_inline__
void dp_classify (threadvar_ctx_t *tv, decode_threadvar_ctx_t *dtv,
		struct iface_t *rx_iface, struct lcore_conf *qconf, struct rte_mbuf **m, uint32_t *res, int num)
{
	int i;

	/* Prefetch first packets */
	for (i = 0; i < PREFETCH_OFFSET && i < num; i++) {
		rte_prefetch0(rte_pktmbuf_mtod(
				m[i], void *));
	}

	for (i = 0; i < (num - PREFETCH_OFFSET); i++) {
		rte_prefetch0(rte_pktmbuf_mtod(m[
				i + PREFETCH_OFFSET], void *));
		dp_classify_post(tv, dtv, rx_iface, qconf, m[i], res[i]);
	}

	/* Process left packets */
	for (; i < num; i++)
		dp_classify_post(tv, dtv, rx_iface, qconf, m[i], res[i]);
}

/* main processing loop */
int main_loop (void *ptr_data)
{
	int i, nb_rx;
	int			socketid;
	uint8_t			lcore_id;
	uint8_t			rx_port_id;
	uint8_t			rx_queue_id;
	uint8_t			tx_queue_id;
	uint32_t		tx_port_id;
	uint64_t		prev_tsc;
	uint64_t		diff_tsc;
	uint64_t		timer_tsc;
	int32_t			nr_ip4_acl;
	int32_t			nr_ip6_acl;
	const uint64_t	drain_tsc = (rte_get_tsc_hz() + US_PER_S - 1) /
							US_PER_S * BURST_TX_DRAIN_US;

	threadvar_ctx_t		*tv;
	decode_threadvar_ctx_t	*dtv;
	vlib_main_t		*vm = (vlib_main_t *)ptr_data;
	vlib_iface_main_t	*pm = &vlib_iface_main;
	vlib_map_main_t		*mm = &vlib_map_main;

	struct lcore_conf	*qconf;
	struct iface_t		*rx_cpu_iface = NULL;
	struct rte_mbuf		*pkts_burst[DPDK_MAX_RX_BURST];
	
	prev_tsc = 0;
	timer_tsc = 0;

	lcore_id = rte_lcore_id();
	tv		=	&g_tv[lcore_id];
	dtv		=	&g_dtv[lcore_id];
	qconf		=	&lcore_conf[lcore_id];
	socketid	=	rte_lcore_to_socket_id(lcore_id);

	if(lcore_id == rte_get_master_lcore()) {
		
	}
	
	/* record which TV this core belong to. */	
	tv->lcore	=	lcore_id;

	if (qconf->n_rx_queue == 0) {
		oryx_logn("lcore %u has nothing to do\n", tv->lcore);
		return 0;
	}

	while (!(vm->ul_flags & VLIB_DP_INITIALIZED)) {
		fprintf (stdout, "lcore %d wait for dataplane ...\n", tv->lcore);
		sleep(1);
	}

	oryx_logn("entering main loop @lcore %u @socket %d\n", tv->lcore, socketid);

	for (i = 0; i < qconf->n_rx_queue; i++) {
		rx_port_id = qconf->rx_queue_list[i].port_id;
		rx_queue_id = qconf->rx_queue_list[i].queue_id;
		fprintf (stdout, " -- lcoreid=%u portid=%hhu rxqueueid=%hhu --\n",
			tv->lcore, rx_port_id, rx_queue_id);
		fflush(stdout);
	}

	while (!force_quit) {

		tv->cur_tsc = rte_rdtsc();

		/*
		 * TX burst queue drain
		 */
		diff_tsc = tv->cur_tsc - prev_tsc;
		if (unlikely(diff_tsc > drain_tsc)) {

			for (i = 0; i < qconf->n_tx_port; ++i) {
				tx_port_id = qconf->tx_port_id[i];
				if (qconf->tx_mbufs[tx_port_id].len == 0)
					continue;
				dp_send_burst(tv, qconf,
					qconf->tx_mbufs[tx_port_id].len, tx_port_id);
				qconf->tx_mbufs[tx_port_id].len = 0;
			}
			/* if timer is enabled */
			if (timer_period > 0) {

				/* advance the timer */
				timer_tsc += diff_tsc;

				/* if timer has reached its timeout */
				if (unlikely(timer_tsc >= timer_period)) {

					/* do this only on a sepcific core */
					if (tv->lcore == rte_get_master_lcore()) {
						/* reset the timer */
						timer_tsc = 0;
						/** Master lcore is never called. */
					}
				}
			}

			prev_tsc = tv->cur_tsc;
		}

		/* SYNC */
		while(vm->ul_flags & VLIB_DP_SYNC) {
			if(!(vm->ul_core_mask & (1 << tv->lcore)))
				vm->ul_core_mask |= (1 << tv->lcore);
		}

		/* fast switch to a prepared ACL context. */
		if(vm->ul_core_mask & (1 << tv->lcore)) {
			g_runtime_acl_config = &acl_config[g_runtime_acl_config_qua % ACL_TABLES];
			vm->ul_core_mask &= ~(1 << tv->lcore);
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

			tv->nr_mbufs_refcnt += nb_rx;
			
			/* port ID -> iface */
			iface_lookup_id(pm, rx_port_id, &rx_cpu_iface);

			/* Rx statistics on this port. */
			iface_counter_update(rx_cpu_iface, nb_rx, 0, QUA_RX, tv->lcore);

			//iface_counters_add(rx_cpu_iface, rx_cpu_iface->if_counter_ctx->lcore_counter_pkts[QUA_RX][lcore_id], nb_rx);

			oryx_counter_add(&tv->perf_private_ctx0, dtv->counter_pkts, nb_rx);
			oryx_counter_add(&tv->perf_private_ctx0, dtv->counter_eth, nb_rx);

			struct parser_ctx_t parser;
			nr_ip4_acl = g_runtime_acl_config->nr_ipv4_rules;
			nr_ip6_acl = g_runtime_acl_config->nr_ipv6_rules;

		#if defined(BUILD_DEBUG)
			oryx_logn("rx_port_id %d, rx_queue_id %d, rx_core_id %d, rx_cpu_iface %p, iface_id %d",
							rx_port_id, rx_queue_id, tv->lcore,
							rx_cpu_iface, iface_id(rx_cpu_iface));
			oryx_logn("Rules [IPv4=%d, IPv6=%d]", nr_ip4_acl, nr_ip6_acl);
		#endif
#if 0
			dp_drop_all(tv, dtv, pkts_burst, nb_rx);
#else
			/* Anyway, classify frames with IPv4, IPv6 and non-IP. */
			dp_classify_prepare (tv, dtv,
					rx_cpu_iface, pkts_burst, &parser, nb_rx);

			/* Do classify on IPv4 frames. */
			if (nr_ip4_acl &&
				parser.num_ipv4) {
				{
					/* get classify private context to guide us how to handle coming frames. */
					rte_acl_classify(
						g_runtime_acl_config->acl_ctx_ip4[socketid],
						parser.data_ipv4,
						parser.res_ipv4,
						parser.num_ipv4,
						DEFAULT_MAX_CATEGORIES);

					/* do real classification. */
					dp_classify (tv, dtv, rx_cpu_iface,
						qconf,
						parser.m_ipv4,
						parser.res_ipv4,
						parser.num_ipv4);
					
					/* IPv4 frame statistics. */
					oryx_counter_add(&tv->perf_private_ctx0, dtv->counter_ipv4, parser.num_ipv4);
				}
			}else {
				/* drop them all. */
				dp_drop_all(tv, dtv, (struct rte_mbuf **)parser.m_ipv4, parser.num_ipv4);
			}
			
			/* Do classify on IPv6 frames. */
			if (nr_ip6_acl &&
				parser.num_ipv6) {
				{				
					/* get classify private context to guide us how to handle coming frames. */
					rte_acl_classify(
						g_runtime_acl_config->acl_ctx_ip6[socketid],
						parser.data_ipv6,
						parser.res_ipv6,
						parser.num_ipv6,
						DEFAULT_MAX_CATEGORIES);

					/* do real classification. */
					dp_classify (tv, dtv, rx_cpu_iface,
						qconf,
						parser.m_ipv6,
						parser.res_ipv6,
						parser.num_ipv6);

					/* IPv6 frame statistics. */
					oryx_counter_add(&tv->perf_private_ctx0, dtv->counter_ipv6, parser.num_ipv6);
				}
			}else {
				/* drop them all. */
				dp_drop_all(tv, dtv, (struct rte_mbuf **)&parser.m_ipv6[0], parser.num_ipv6);
			}
#endif				
		}
	}

	return 0;
}

