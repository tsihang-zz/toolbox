#include "oryx.h"
#include "config.h"
#include "dpdk.h"
#include "iface.h"
#include "dp.h"
/* < protocol headers> */
#include "ethh.h"
#include "iph.h"
#include "tcph.h"
#include "udph.h"
#include "icmph.h"
#include "sctph.h"
#include "ppph.h"
#include "pppoeh.h"
#include "greh.h"
#include "mplsh.h"
#include "802xx.h"
#include "dsa.h"

/* Configure how many packets ahead to prefetch, when reading packets */
#define PREFETCH_OFFSET	  3
#define GET_MBUF_PRIVATE(type,m)\
	((type *)RTE_PTR_ADD((m), sizeof(struct rte_mbuf)));
#define tx_panel_port_is_online(tx_panel_port_id)\
	((tx_panel_port_id) != UINT32_MAX)

threadvar_ctx_t g_tv[VLIB_MAX_LCORES];
decode_threadvar_ctx_t g_dtv[VLIB_MAX_LCORES];
volatile bool force_quit = false;

struct enp5s0fx_t {
	const char *name;
};

static struct enp5s0fx_t enp5s0fx[] = {
	{
		.name = "enp5s0f1",
	},
	{
		.name = "enp5s0f2",
	},
	{
		.name = "enp5s0f3",
	}
};

__oryx_always_extern__
void dp_check_port
(
	IN vlib_main_t *vm
)
{
	uint8_t portid;
	struct iface_t *this;
	vlib_iface_main_t *pm = &vlib_iface_main;
	int nr_ports = rte_eth_dev_count();

	force_quit = false;
	/* convert to number of cycles */
	timer_period *= rte_get_timer_hz();

	dpdk_main_t *dm = &dpdk_main;
	dm->conf->mempool_priv_size = (RTE_CACHE_LINE_ROUNDUP(sizeof(struct vlib_pkt_t)));
	
	if(!(vm->ul_flags & VLIB_PORT_INITIALIZED)) {
		oryx_panic(-1, "Run port initialization first.");
	}
	
	/* register to vlib_iface_main. */
	for (portid = 0; portid < nr_ports; portid ++) {
		iface_lookup_id(pm, portid, &this);
		if(!this) {
			oryx_panic(-1, "no such ethdev.");
		}
		if(strcmp(this->sc_alias_fixed, enp5s0fx[portid].name)) {
			oryx_panic(-1, "no such ethdev named %s.", this->sc_alias_fixed);
		}
	}

}

__oryx_always_extern__
void notify_dp
(
	IN vlib_main_t *vm,
	IN int signum
)
{
	vm = vm;
	if (signum == SIGINT || signum == SIGTERM) {
		fprintf (stdout, "\n\nSignal %d received, preparing to exit...\n",
				signum);
		force_quit = true;
	}
}

static void dp_register_perf_counters
(
	IN decode_threadvar_ctx_t *dtv,
	IN threadvar_ctx_t *tv
)
{
	/* register counters */
	dtv->counter_pkts = 
		oryx_register_counter("decoder.pkts", 
									"Packets that Rx have been decoded by PRG", &tv->perf_private_ctx0);
	dtv->counter_bytes = 
		oryx_register_counter("decoder.bytes", 
									"Bytes that Rx have been decoded by PRG", &tv->perf_private_ctx0);
	dtv->counter_invalid = 
		oryx_register_counter("decoder.invalid", 
									"Counter for unknown packets which can not been decoded by PRG", &tv->perf_private_ctx0);
	dtv->counter_ipv4 = 
		oryx_register_counter("decoder.ipv4", 
									NULL, &tv->perf_private_ctx0);

	dtv->counter_dsa = 
		oryx_register_counter("decoder.dsa", 
									NULL, &tv->perf_private_ctx0);

	dtv->counter_ipv6 = 
		oryx_register_counter("decoder.ipv6", 
									NULL, &tv->perf_private_ctx0);
	dtv->counter_eth = 
		oryx_register_counter("decoder.ethernet",
									NULL, &tv->perf_private_ctx0);
	dtv->counter_raw = 
		oryx_register_counter("decoder.raw", 
									NULL, &tv->perf_private_ctx0);
	dtv->counter_null = 
		oryx_register_counter("decoder.null",
									NULL, &tv->perf_private_ctx0);
	dtv->counter_sll = 
		oryx_register_counter("decoder.sll",
									NULL, &tv->perf_private_ctx0);
	dtv->counter_tcp = 
		oryx_register_counter("decoder.tcp", 
									NULL, &tv->perf_private_ctx0);
	dtv->counter_udp = 
		oryx_register_counter("decoder.udp",
									NULL, &tv->perf_private_ctx0);
	dtv->counter_sctp = 
		oryx_register_counter("decoder.sctp",
									NULL, &tv->perf_private_ctx0);
	dtv->counter_icmpv4 =
		oryx_register_counter("decoder.icmpv4", 
									NULL, &tv->perf_private_ctx0);
	dtv->counter_icmpv6 =
		oryx_register_counter("decoder.icmpv6", 
									NULL, &tv->perf_private_ctx0);
	dtv->counter_ppp =
		oryx_register_counter("decoder.ppp", 
									NULL, &tv->perf_private_ctx0);
	dtv->counter_pppoe =
		oryx_register_counter("decoder.pppoe",
									NULL, &tv->perf_private_ctx0);
	dtv->counter_gre =
		oryx_register_counter("decoder.gre",
									NULL, &tv->perf_private_ctx0);
	dtv->counter_arp =
		oryx_register_counter("decoder.arp",
									NULL, &tv->perf_private_ctx0);
	dtv->counter_vlan =
		oryx_register_counter("decoder.vlan",
									NULL, &tv->perf_private_ctx0);
	dtv->counter_vlan_qinq =
		oryx_register_counter("decoder.vlan_qinq",
									NULL, &tv->perf_private_ctx0);
	dtv->counter_ieee8021ah =
		oryx_register_counter("decoder.ieee8021ah",
									NULL, &tv->perf_private_ctx0);
	dtv->counter_teredo =
		oryx_register_counter("decoder.teredo",
									NULL, &tv->perf_private_ctx0);
	dtv->counter_ipv4inipv6 =
		oryx_register_counter("decoder.ipv4_in_ipv6",
									NULL, &tv->perf_private_ctx0);
	dtv->counter_ipv6inipv6 =
		oryx_register_counter("decoder.ipv6_in_ipv6",
									NULL, &tv->perf_private_ctx0);
	dtv->counter_mpls =
		oryx_register_counter("decoder.mpls",
									NULL, &tv->perf_private_ctx0);
	dtv->counter_avg_pkt_size =
		oryx_register_counter("decoder.avg_pkt_size",
									NULL, &tv->perf_private_ctx0);
	dtv->counter_max_pkt_size =
		oryx_register_counter("decoder.max_pkt_size",
									NULL, &tv->perf_private_ctx0);
	dtv->counter_erspan =
		oryx_register_counter("decoder.erspan",
									NULL, &tv->perf_private_ctx0);

	dtv->counter_http = 
		oryx_register_counter("decoder.http",
									NULL, &tv->perf_private_ctx0);

	dtv->counter_drop =
		oryx_register_counter("decoder.drop",
									NULL, &tv->perf_private_ctx0);
	
	dtv->counter_flow_memcap =
		oryx_register_counter("flow.memcap",
									NULL, &tv->perf_private_ctx0);
	dtv->counter_flow_tcp =
		oryx_register_counter("flow.tcp",
									NULL, &tv->perf_private_ctx0);
	dtv->counter_flow_udp =
		oryx_register_counter("flow.udp",
									NULL, &tv->perf_private_ctx0);
	dtv->counter_flow_icmp4 =
		oryx_register_counter("flow.icmpv4",
									NULL, &tv->perf_private_ctx0);
	dtv->counter_flow_icmp6 =
		oryx_register_counter("flow.icmpv6",
									NULL, &tv->perf_private_ctx0);

	dtv->counter_defrag_ipv4_fragments =
		oryx_register_counter("defrag.ipv4.fragments",
									NULL, &tv->perf_private_ctx0);
	dtv->counter_defrag_ipv4_reassembled =
		oryx_register_counter("defrag.ipv4.reassembled",
									NULL, &tv->perf_private_ctx0);
	dtv->counter_defrag_ipv4_timeouts =
		oryx_register_counter("defrag.ipv4.timeouts",
									NULL, &tv->perf_private_ctx0);
	dtv->counter_defrag_ipv6_fragments =
		oryx_register_counter("defrag.ipv6.fragments",
									NULL, &tv->perf_private_ctx0);
	dtv->counter_defrag_ipv6_reassembled =
		oryx_register_counter("defrag.ipv6.reassembled",
									NULL, &tv->perf_private_ctx0);
	dtv->counter_defrag_ipv6_timeouts =
		oryx_register_counter("defrag.ipv6.timeouts",
									NULL, &tv->perf_private_ctx0);
	dtv->counter_defrag_max_hit =
		oryx_register_counter("defrag.max_frag_hits",
									NULL, &tv->perf_private_ctx0);
	
	oryx_counter_get_array_range(1, 
		atomic_read(tv->perf_private_ctx0.curr_id), &tv->perf_private_ctx0);

    return;
}

static __oryx_always_inline__
void dsa_tag_strip
(
	IN struct rte_mbuf *m
)
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
void act_packet_trim
(
	IN struct rte_mbuf *m,
	IN uint16_t slice_size
)
{
	if (slice_size > m->pkt_len)
		return;
	/* packet will always be 64 bytes when drop_size = m->pkt_len - 64. */
	rte_pktmbuf_trim(m, m->pkt_len - slice_size);
}

static __oryx_always_inline__
int dsa_tag_insert
(
	OUT struct rte_mbuf **m,
	IN uint32_t new_cpu_dsa
)
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
uint32_t dsa_tag_update
(
	IN uint32_t cpu_tag,
	IN uint8_t tx_virtual_port_id
)
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
uint32_t ipv4_hash_crc
(
	IN const void *data,
	IN uint32_t __oryx_unused__ data_len,
	IN uint32_t init_val
)
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
void dump_pkt(uint8_t *pkt, int len)
{
	int i = 0;
	
	for (i = 0; i < len; i ++){
		if (!(i % 16))
			fprintf (stdout, "\n");
		fprintf (stdout, "%02x ", pkt[i]);
	}
	fprintf (stdout, "\n");
}

static __oryx_always_inline__
uint32_t dp_recalc_rss
(
	IN struct rte_mbuf *m
)
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
int dp_decide_tx_port
(
	IN struct iface_t *rx_cpu_iface,
	IN struct rte_mbuf *m,
	IN uint32_t *tx_port_id
)
{
	uint32_t rss;
	uint32_t index;
	uint32_t tx_panel_port_id = 0;
	
	rss = (iface_id(rx_cpu_iface) == SW_CPU_XAUI_PORT_ID) ? \
			dp_recalc_rss(m) : m->hash.rss;

	/*
	 * Packets from GE port will be delivered to 2 XE ports
	 * and those from XE port will be delivered to 8 GE ports.
	 */
	if (iface_id(rx_cpu_iface) == SW_CPU_XAUI_PORT_ID) {
		index = rss % 2;
		*tx_port_id = tx_panel_port_id = (index + 1);	/* skip enp5s0f1 */
	} else {
		index = rss % ET1500_N_GE_PORTS;
		*tx_port_id = tx_panel_port_id = index;
	}
	
	if(tx_panel_port_id != UINT32_MAX &&
		tx_panel_port_id > SW_PORT_OFFSET)
		*tx_port_id = SW_CPU_XAUI_PORT_ID;

#if defined(BUILD_DEBUG)
		oryx_logn("%20s%8u", "rx_cpu_iface: ", iface_id(rx_cpu_iface));
		oryx_logn("%20s%8u(%8u)", "rss: ", rss, m->hash.rss);
		oryx_logn("%20s%8u", "tx_cpu_iface: ", tx_panel_port_id);
#endif

	return tx_panel_port_id;
}

static __oryx_always_inline__
void dp_load_frame_key4
(
	IN union ipv4_5tuple_host *key,
	IN struct rte_mbuf *pkt,
	IN uint32_t ipoff
)
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
void dp_dump_packet_key4
(
	IN union ipv4_5tuple_host *key,
	IN struct rte_mbuf *pkt
)
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
void dp_drop_packet
(
	IN threadvar_ctx_t *tv,
	IN decode_threadvar_ctx_t *dtv,
	IN struct rte_mbuf *pkt
)
{
	rte_pktmbuf_free(pkt);
	oryx_counter_inc(&tv->perf_private_ctx0, dtv->counter_drop);
	tv->nr_mbufs_feedback ++;
}
	
static __oryx_always_inline__
void dp_drop_all
(
	IN threadvar_ctx_t *tv,
	IN decode_threadvar_ctx_t *dtv,
	IN struct rte_mbuf **pkts_in,
	IN int nb
)
{
	int i;
	for (i = 0; i < nb; i ++) {
		struct rte_mbuf *pkt = pkts_in[i];
		dp_drop_packet(tv, dtv, pkt);
	}
}

/* Send burst of packets on an output interface */
static __oryx_always_inline__
int dp_send_burst
(
	IN threadvar_ctx_t *tv,
	IN struct lcore_conf *qconf,
	IN uint16_t n,
	IN uint32_t tx_port_id
)
{
	int 		ret;
	uint16_t		tx_queue_id;
	uint16_t		nr_tx_pkts = 0;
	struct iface_t		*tx_iface = NULL;
	struct rte_mbuf 	**m_table;
	vlib_iface_main_t	*pm = &vlib_iface_main;

	/** port ID -> iface */
	iface_lookup_id(pm, tx_port_id, &tx_iface);

	tx_queue_id = qconf->tx_queue_id[tx_port_id];
	m_table = (struct rte_mbuf **)qconf->tx_mbufs[tx_port_id].m_table;

	nr_tx_pkts = ret = rte_eth_tx_burst((uint8_t)tx_port_id, tx_queue_id, m_table, n);
	if (unlikely(ret < n)) {
		do {
			rte_pktmbuf_free(m_table[ret]);
		} while (++ ret < n);
	}
	tv->nr_mbufs_feedback += n;

#if defined(BUILD_DEBUG)
	oryx_logd("tx_port_id=%d tx_queue_id %d, tx_lcore_id %d tx_pkts_num %d/%d",
		tx_port_id, tx_queue_id, tv->lcore, nr_tx_pkts, n);
#endif
	iface_counters_add(tx_iface,
		tx_iface->if_counter_ctx->lcore_counter_pkts[QUA_TX][qconf->lcore_id], nr_tx_pkts);

	return 0;
}

static __oryx_always_inline__
void dp_parse_key4
(
	IN threadvar_ctx_t *tv,
	IN decode_threadvar_ctx_t *dtv,
	OUT size_t *nr_off,
	IN union ipv4_5tuple_host *k
)
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
void dp_classify_prepare_one_packet
(
	IN threadvar_ctx_t *tv,
	IN decode_threadvar_ctx_t *dtv,
	IN struct iface_t *rx_iface,
	IN struct rte_mbuf **rx_pkts,
	IN struct parser_ctx_t *parser,
	IN int index
)
{
	vlib_pkt_t			*p;
	vlib_iface_main_t	*pm = &vlib_iface_main;
	struct rte_mbuf 	*pkt = rx_pkts[index];
	struct iface_t		*rx_panel_iface = NULL;
	
	p = GET_MBUF_PRIVATE(vlib_pkt_t, pkt);

	/* ethernet type. */
	uint16_t ether_type;

	/* offset of every header. */
	size_t nroff = ETHERNET_HEADER_LEN;

	uint8_t rx_panel_port_id = -1;
	const bool dsa_support = iface_support_marvell_dsa(rx_iface);

	if (dsa_support) {
		/* change default if we get a dsa frame. */
		nroff = ETHERNET_DSA_HEADER_LEN;
		
		MarvellDSAEthernetHdr *dsaeth = rte_pktmbuf_mtod(pkt, MarvellDSAEthernetHdr *);
		p->dsa = rte_be_to_cpu_32(dsaeth->dsah.dsa);
		ether_type = dsaeth->eth_type;
		rx_panel_port_id = DSA_TO_GLOBAL_PORT_ID(p->dsa);

		/* DSA is invalid. */
		if (!DSA_IS_INGRESS(p->dsa)) {
			dp_drop_packet(tv, dtv, pkt);
			return;
		}
		/* Rx statistics on an ethernet GE port. */
		iface_lookup_id(pm, rx_panel_port_id, &rx_panel_iface);
		iface_counter_update(rx_panel_iface, 1, pkt->pkt_len, QUA_RX, tv->lcore);
	} else {
		EthernetHdr *eth = rte_pktmbuf_mtod(pkt, EthernetHdr *);
		ether_type = eth->eth_type;
	}
	
#if defined(BUILD_DEBUG)
	oryx_logn("%20s%4d", "rx_panel_port_id: ",	rx_panel_port_id);
	oryx_logn("%20s%4x", "eth_type: ",			rte_be_to_cpu_16(ether_type));
	if(dsa_support) PrintDSA("RX", p->dsa, QUA_RX);
	dump_pkt((uint8_t *)rte_pktmbuf_mtod(pkt, void *), pkt->pkt_len);
#endif

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
		dp_parse_key4(tv, dtv, &nroff, &k4);
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
		//dp_drop_packet(tv, dtv, pkt);
		return;
	}

}

static __oryx_always_inline__
void dp_classify_prepare
(
	IN threadvar_ctx_t *tv,
	IN decode_threadvar_ctx_t *dtv,
	IN struct iface_t *rx_iface,
	IN struct rte_mbuf **rx_pkts,
	IN int nr_rx_pkts,
	OUT struct parser_ctx_t *parser
)
{
	int i;

	/* reset acl search context. */
	parser->num_ipv4	= 0;
	parser->num_ipv6	= 0;
	parser->num_notip	= 0;

	/* Prefetch first packets */
	for (i = 0; i < PREFETCH_OFFSET && i < nr_rx_pkts; i++) {
		rte_prefetch0(rte_pktmbuf_mtod(
				rx_pkts[i], void *));
	}

	for (i = 0; i < (nr_rx_pkts - PREFETCH_OFFSET); i++) {
		rte_prefetch0(rte_pktmbuf_mtod(rx_pkts[
				i + PREFETCH_OFFSET], void *));
		dp_classify_prepare_one_packet(tv, dtv, rx_iface, rx_pkts, parser, i);
	}

	/* Process left packets */
	for (; i < nr_rx_pkts; i++) {
		dp_classify_prepare_one_packet(tv, dtv, rx_iface, rx_pkts, parser, i);
	}
}


/* Enqueue a single packet, and send burst if queue is filled */
static __oryx_always_inline__
int dp_send_single_packet
(
	IN threadvar_ctx_t *tv,
	IN decode_threadvar_ctx_t __oryx_unused__ *dtv,
	IN struct iface_t *rx_cpu_iface,
	IN struct lcore_conf *qconf,
	IN struct rte_mbuf *m,
	IN uint32_t tx_port_id,
	IN uint8_t tx_panel_port_id
)
{
	uint16_t		len;
	uint32_t		cpu_dsa = 0;
	uint32_t		tx_panel_ge_id = 0;
	struct iface_t		*tx_panel_iface = NULL;
	vlib_iface_main_t	*pm = &vlib_iface_main;
	
	if (iface_id(rx_cpu_iface) == SW_CPU_XAUI_PORT_ID) {
		vlib_pkt_t *p = GET_MBUF_PRIVATE(vlib_pkt_t, m);
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

static __oryx_always_inline__
void dp_classify_post
(
	IN threadvar_ctx_t *tv,
	IN decode_threadvar_ctx_t *dtv,
	IN struct iface_t *rx_cpu_iface,
	IN struct lcore_conf *qconf,
	IN struct rte_mbuf *m,
	IN uint32_t __oryx_unused__ res
)
{
	uint32_t		tx_panel_port_id = 0;
	uint32_t		tx_port_id = iface_id(rx_cpu_iface);

	tx_panel_port_id = dp_decide_tx_port(rx_cpu_iface, m, &tx_port_id);
	
	if(tx_panel_port_is_online(tx_panel_port_id)) {
		dp_send_single_packet(tv, dtv, rx_cpu_iface, qconf, m, tx_port_id, tx_panel_port_id);
	} else {
		dp_drop_packet(tv, dtv, m);
	}
}
		
static __oryx_always_inline__
void dp_classify
(
	IN threadvar_ctx_t *tv,
	IN decode_threadvar_ctx_t *dtv,
	IN struct iface_t *rx_cpu_iface,
	IN struct lcore_conf *qconf,
	IN struct rte_mbuf **m,
	IN uint32_t __oryx_unused__ *res,
	IN int num
)
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
		dp_classify_post (tv, dtv, rx_cpu_iface, qconf, m[i], 0/*res[i]*/);
	}

	/* Process left packets */
	for (; i < num; i++)
		dp_classify_post (tv, dtv, rx_cpu_iface, qconf, m[i], 0/*res[i]*/);
}


static
int main_loop (void *ptr_data)
{
	int i, nr_rx_pkts;
	int 		socketid;
	uint8_t 		lcore_id;
	uint8_t 		rx_port_id;
	uint8_t 		rx_queue_id;
	uint32_t		tx_port_id;
	uint64_t		prev_tsc;
	uint64_t		diff_tsc;
	uint64_t		timer_tsc;
	const uint64_t	drain_tsc = (rte_get_tsc_hz() + US_PER_S - 1) /
							US_PER_S * BURST_TX_DRAIN_US;

	threadvar_ctx_t 	*tv;
	decode_threadvar_ctx_t	*dtv;
	vlib_main_t 	*vm = (vlib_main_t *)ptr_data;
	vlib_iface_main_t	*pm = &vlib_iface_main;

	struct lcore_conf	*qconf;
	struct iface_t		*rx_cpu_iface = NULL;
	struct rte_mbuf 	*rx_pkts[DPDK_MAX_RX_BURST];
	
	prev_tsc = 0;
	timer_tsc = 0;

	lcore_id = rte_lcore_id();
	tv		=	&g_tv[lcore_id];
	dtv 	=	&g_dtv[lcore_id];
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
		if (force_quit)
			break;
	}

	oryx_logn("entering main loop @lcore %u @socket %d", tv->lcore, socketid);

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

		/* Interface of SYNC from controlplane to dataplane.
		 * Used to sync ACL rules or some other purpose. */
		while(vm->ul_flags & VLIB_DP_SYNC) {
			if(!(vm->ul_core_mask & (1 << tv->lcore)))
				vm->ul_core_mask |= (1 << tv->lcore);
		}

		/*
		 * Read packet from RX queues
		 */
		for (i = 0; i < qconf->n_rx_queue; ++ i) {
			rx_port_id = qconf->rx_queue_list[i].port_id;
			rx_queue_id = qconf->rx_queue_list[i].queue_id;
			nr_rx_pkts = rte_eth_rx_burst(rx_port_id, rx_queue_id, rx_pkts,
								DPDK_MAX_RX_BURST);
			if (nr_rx_pkts == 0)
				continue;

			tv->nr_mbufs_refcnt += nr_rx_pkts;
			
			/* port ID -> iface */
			iface_lookup_id(pm, rx_port_id, &rx_cpu_iface);

			/* Rx statistics on this port. */
			iface_counter_update(rx_cpu_iface, nr_rx_pkts, 0, QUA_RX, tv->lcore);

			oryx_counter_add(&tv->perf_private_ctx0, dtv->counter_pkts, nr_rx_pkts);
			oryx_counter_add(&tv->perf_private_ctx0, dtv->counter_eth, nr_rx_pkts);

	#if defined(BUILD_DEBUG)
			oryx_logn("rx_port[%d], rx_queue[%d], rx_core[%d], rx_cpu_iface %p, iface_id %d",
							rx_port_id, rx_queue_id, tv->lcore,
							rx_cpu_iface, iface_id(rx_cpu_iface));
	#endif

			struct parser_ctx_t parser = {
				.data_ipv4 = {0},
				.m_ipv4 = {0},
				.res_ipv4 = {0},
				.num_ipv4 = 0,
				.data_ipv6 = {0},
				.m_ipv6 = {0},
				.res_ipv6 = {0},
				.num_ipv6 = 0,
				.data_notip = {0},
				.m_notip = {0},
				.res_notip = {0},
				.num_notip = 0,
			};
				
			/* Anyway, classify frames with IPv4, IPv6 and non-IP. */
			dp_classify_prepare (tv, dtv,
					rx_cpu_iface, rx_pkts, nr_rx_pkts, &parser);

			/* Do classify on IPv4 frames. */
			if (parser.num_ipv4) {
				/* IPv4 frame statistics. */
				oryx_counter_add (&tv->perf_private_ctx0, dtv->counter_ipv4, parser.num_ipv4);
				/* do real classification. */
				dp_classify (tv, dtv, rx_cpu_iface,
					qconf,
					parser.m_ipv4,
					parser.res_ipv4,
					parser.num_ipv4);
			}

			/* Do classify on IPv6 frames. */
			if (parser.num_ipv6) {
				/* IPv6 frame statistics. */
				oryx_counter_add(&tv->perf_private_ctx0, dtv->counter_ipv6, parser.num_ipv6);
				/* do real classification. */
				dp_classify (tv, dtv, rx_cpu_iface,
					qconf,
					parser.m_ipv6,
					parser.res_ipv6,
					parser.num_ipv6);
			}
			
			/* drop all unidentified packets. */
			dp_drop_all(tv, dtv, (struct rte_mbuf **)parser.m_notip, parser.num_notip);
		}
	}

	return 0;
}

void dp_start
(
	IN vlib_main_t *vm
)
{
	int i;
	threadvar_ctx_t *tv;
	decode_threadvar_ctx_t *dtv;
	char thrgp_name[128] = {0}; 


	fprintf(stdout,
		"Master Lcore @ %d/%d\n", rte_get_master_lcore(), vm->nb_lcores);

	/* launch per-lcore init on every lcore */
	rte_eal_mp_remote_launch(main_loop, vm, CALL_MASTER);

	/** init thread vars for dataplane. */
	for (i = 0; i < vm->nb_lcores; i ++) {
		tv = &g_tv[i];
		dtv = &g_dtv[i];
		sprintf (thrgp_name, "dp[%u] hd-thread", i);
		tv->thread_group_name = strdup(thrgp_name);
		ATOMIC_INIT(tv->flags);
		dp_register_perf_counters(dtv, tv);
	}

	vm->ul_flags |= VLIB_DP_INITIALIZED;
	
}

void dp_stop
(
	IN vlib_main_t *vm
)
{
	dpdk_main_t *dm = &dpdk_main;
	uint8_t portid;
	
	for (portid = 0; portid < vm->nr_sys_ports; portid++) {
		if ((dm->conf->portmask & (1 << portid)) == 0)
			continue;
		fprintf (stdout, "Closing port %d...", portid);
		rte_eth_dev_stop(portid);
		rte_eth_dev_close(portid);
		fprintf (stdout, " Done\n");
	}
	fprintf (stdout, "Bye...\n");
}

void dp_stats(vlib_main_t *vm)
{
	int lcore = 0;
	threadvar_ctx_t *tv;
	decode_threadvar_ctx_t *dtv;
	dpdk_main_t *dm = &dpdk_main;
	dpdk_config_main_t *conf = dm->conf;
	uint64_t nr_mbufs_delta = 0;
	struct timeval start, end;
	static uint64_t cur_tsc[VLIB_MAX_LCORES];
	struct oryx_fmt_buff_t fb = FMT_BUFF_INITIALIZATION;
	uint64_t counter_eth[VLIB_MAX_LCORES] = {0};
	uint64_t counter_ipv4[VLIB_MAX_LCORES] = {0};
	uint64_t counter_ipv6[VLIB_MAX_LCORES] = {0};
	uint64_t counter_tcp[VLIB_MAX_LCORES] = {0};
	uint64_t counter_udp[VLIB_MAX_LCORES] = {0};
	uint64_t counter_sctp[VLIB_MAX_LCORES] = {0};
	uint64_t counter_arp[VLIB_MAX_LCORES] = {0};
	uint64_t counter_icmpv4[VLIB_MAX_LCORES] = {0};
	uint64_t counter_icmpv6[VLIB_MAX_LCORES] = {0};
	uint64_t counter_pkts[VLIB_MAX_LCORES] = {0};
	uint64_t counter_bytes[VLIB_MAX_LCORES] = {0};
	uint64_t counter_pkts_invalid[VLIB_MAX_LCORES] = {0};
	uint64_t counter_drop[VLIB_MAX_LCORES] = {0};
	uint64_t counter_http[VLIB_MAX_LCORES] = {0};
	uint64_t counter_eth_total = 0;
	uint64_t counter_ipv4_total = 0;
	uint64_t counter_ipv6_total = 0;
	uint64_t counter_tcp_total = 0;
	uint64_t counter_udp_total = 0;
	uint64_t counter_icmpv4_total = 0;
	uint64_t counter_icmpv6_total = 0;
	uint64_t counter_pkts_total = 0;
	uint64_t counter_bytes_total = 0;
	uint64_t counter_arp_total = 0;
	uint64_t counter_sctp_total = 0;
	uint64_t counter_pkts_invalid_total = 0;
	uint64_t counter_drop_total = 0;
	uint64_t counter_http_total = 0;
	uint64_t nr_mbufs_refcnt = 0;
	uint64_t nr_mbufs_feedback = 0;

	FILE *fp;
	const char *box_dp_stats = "/tmp/box_dp_stats.log";
	char emptycmd[128] = "cat /dev/null > ";

	strcat(emptycmd, box_dp_stats);
	do_system(emptycmd);

	fp = fopen(box_dp_stats, "a+");
	if(!fp) {
		return;
	}

	if (!(vm->ul_flags & VLIB_DP_INITIALIZED)) {
		fprintf(fp, "Dataplane is not ready\n");
		fclose(fp);
		return;
	}

	gettimeofday(&start, NULL);

	for (lcore = 0; lcore < vm->nb_lcores; lcore ++) {
		tv = &g_tv[lcore % vm->nb_lcores];
		dtv = &g_dtv[lcore % vm->nb_lcores];

		cur_tsc[lcore]		=  tv->cur_tsc;
		nr_mbufs_refcnt 	+= tv->nr_mbufs_refcnt;
		nr_mbufs_feedback	+= tv->nr_mbufs_feedback;
		
		counter_pkts_total += 
			counter_pkts[lcore] = oryx_counter_get(&tv->perf_private_ctx0, dtv->counter_pkts);
		counter_bytes_total += 
			counter_bytes[lcore] = oryx_counter_get(&tv->perf_private_ctx0, dtv->counter_bytes);
		counter_eth_total += 
			counter_eth[lcore] = oryx_counter_get(&tv->perf_private_ctx0, dtv->counter_eth);
		counter_ipv4_total += 
			counter_ipv4[lcore] = oryx_counter_get(&tv->perf_private_ctx0, dtv->counter_ipv4);
		counter_ipv6_total += 
			counter_ipv6[lcore] = oryx_counter_get(&tv->perf_private_ctx0, dtv->counter_ipv6);
		counter_tcp_total += 
			counter_tcp[lcore] = oryx_counter_get(&tv->perf_private_ctx0, dtv->counter_tcp);
		counter_udp_total += 
			counter_udp[lcore] = oryx_counter_get(&tv->perf_private_ctx0, dtv->counter_udp);
		counter_icmpv4_total += 
			counter_icmpv4[lcore] = oryx_counter_get(&tv->perf_private_ctx0, dtv->counter_icmpv4);
		counter_icmpv6_total +=
			counter_icmpv6[lcore] = oryx_counter_get(&tv->perf_private_ctx0, dtv->counter_icmpv6);
		counter_arp_total +=
			counter_arp[lcore] = oryx_counter_get(&tv->perf_private_ctx0, dtv->counter_arp);
		counter_sctp_total +=
			counter_sctp[lcore] = oryx_counter_get(&tv->perf_private_ctx0, dtv->counter_sctp);
		counter_pkts_invalid_total +=
			counter_pkts_invalid[lcore] = oryx_counter_get(&tv->perf_private_ctx0, dtv->counter_invalid);
		counter_http_total +=
			counter_http[lcore] = oryx_counter_get(&tv->perf_private_ctx0, dtv->counter_http);
		counter_drop_total +=
			counter_drop[lcore] = oryx_counter_get(&tv->perf_private_ctx0, dtv->counter_drop);
	}

#if 0
	if (argc == 1 && !strncmp (argv[0], "c", 1)) {
		for (lcore = 0; lcore < vm->nb_lcores; lcore ++) {
			tv	= &g_tv[lcore % vm->nb_lcores];
			dtv = &g_dtv[lcore % vm->nb_lcores];

			tv->nr_mbufs_feedback	= 0;
			tv->nr_mbufs_refcnt 	= 0;
			
			oryx_counter_set(&tv->perf_private_ctx0, dtv->counter_pkts, 0);
			oryx_counter_set(&tv->perf_private_ctx0, dtv->counter_bytes, 0);
			oryx_counter_set(&tv->perf_private_ctx0, dtv->counter_eth, 0);
			oryx_counter_set(&tv->perf_private_ctx0, dtv->counter_ipv4, 0);
			oryx_counter_set(&tv->perf_private_ctx0, dtv->counter_ipv6, 0);
			oryx_counter_set(&tv->perf_private_ctx0, dtv->counter_tcp, 0);
			oryx_counter_set(&tv->perf_private_ctx0, dtv->counter_udp, 0);
			oryx_counter_set(&tv->perf_private_ctx0, dtv->counter_icmpv4, 0);
			oryx_counter_set(&tv->perf_private_ctx0, dtv->counter_icmpv6, 0);
			oryx_counter_set(&tv->perf_private_ctx0, dtv->counter_arp, 0);
			oryx_counter_set(&tv->perf_private_ctx0, dtv->counter_sctp, 0);
			oryx_counter_set(&tv->perf_private_ctx0, dtv->counter_invalid, 0);
			oryx_counter_set(&tv->perf_private_ctx0, dtv->counter_drop, 0);
			oryx_counter_set(&tv->perf_private_ctx0, dtv->counter_http, 0);
		}
	}
#endif
	nr_mbufs_delta = (nr_mbufs_refcnt - nr_mbufs_feedback);

	fprintf(fp, "==== Global Graph%s", "\n");	
	fprintf(fp, "%12s%16lu%s", "Total_Pkts:", counter_pkts_total, "\n");
	fprintf(fp, "%12s%16lu (%u, %.2f%%)%s", "MBUF Usage:", nr_mbufs_delta, conf->nr_mbufs, ratio_of(nr_mbufs_delta, conf->nr_mbufs), "\n");
	fprintf(fp, "%12s%16lu (%.2f%%)%s", "IPv4:", counter_ipv4_total, ratio_of(counter_ipv4_total, counter_pkts_total), "\n");
	fprintf(fp, "%12s%16lu (%.2f%%)%s", "IPv6:", counter_ipv6_total, ratio_of(counter_ipv6_total, counter_pkts_total), "\n");
	fprintf(fp, "%12s%16lu (%.2f%%)%s", "UDP:", counter_udp_total, ratio_of(counter_udp_total, counter_pkts_total), "\n");
	fprintf(fp, "%12s%16lu (%.2f%%)%s", "TCP:", counter_tcp_total, ratio_of(counter_tcp_total, counter_pkts_total), "\n");
	fprintf(fp, "%12s%16lu (%.2f%%)%s", "SCTP:", counter_sctp_total, ratio_of(counter_sctp_total, counter_pkts_total), "\n");
	fprintf(fp, "%12s%16lu (%.2f%%)%s", "Drop:", counter_drop_total, ratio_of(counter_drop_total, counter_pkts_total), "\n");

	fprintf(fp, "%s", "\n");
	
	oryx_format_reset(&fb);
	oryx_format(&fb, "%12s", " ");
	for (lcore = 0; lcore < vm->nb_lcores; lcore ++) {
		char lcore_str[32];
		sprintf(lcore_str, "lcore%d", lcore);
		oryx_format(&fb, "%20s", lcore_str);
	}
	fprintf(fp, "%s%s", FMT_DATA(fb), "\n");
	oryx_format_reset(&fb);

	oryx_format(&fb, "%12s", "core_ratio");
	for (lcore = 0; lcore < vm->nb_lcores; lcore ++) {
		char format_pkts[20] = {0};
		sprintf (format_pkts, "%.2f%s", ratio_of(counter_pkts[lcore], counter_pkts_total), "%");
		oryx_format(&fb, "%20s", format_pkts);
	}
	fprintf(fp, "%s%s", FMT_DATA(fb), "\n");
	oryx_format_reset(&fb);

	oryx_format(&fb, "%12s", "alive");
	for (lcore = 0; lcore < vm->nb_lcores; lcore ++) {
		tv = &g_tv[lcore % vm->nb_lcores];
		oryx_format(&fb, "%20s", cur_tsc[lcore] == tv->cur_tsc ? "N" : "-");
	}
	fprintf(fp, "%s%s", FMT_DATA(fb), "\n");
	oryx_format_reset(&fb);

	oryx_format(&fb, "%12s", "pkts");
	for (lcore = 0; lcore < vm->nb_lcores; lcore ++) {
		if(counter_pkts[lcore] == 0)
			oryx_format(&fb, "%20s", "-");
		else
			oryx_format(&fb, "%20llu", counter_pkts[lcore]);
	}
	fprintf(fp, "%s%s", FMT_DATA(fb), "\n");
	oryx_format_reset(&fb);

	oryx_format(&fb, "%12s", "ethernet");
	for (lcore = 0; lcore < vm->nb_lcores; lcore ++) {
		if(counter_eth[lcore] == 0)
			oryx_format(&fb, "%20s", "-");
		else
			oryx_format(&fb, "%20llu", counter_eth[lcore]);
	}
	fprintf(fp, "%s%s", FMT_DATA(fb), "\n");
	oryx_format_reset(&fb);

	oryx_format(&fb, "%12s", "ipv4");
	for (lcore = 0; lcore < vm->nb_lcores; lcore ++) {
		if(counter_ipv4[lcore] == 0)
			oryx_format(&fb, "%20s", "-");
		else
			oryx_format(&fb, "%20llu", counter_ipv4[lcore]);
	}
	fprintf(fp, "%s%s", FMT_DATA(fb), "\n");
	oryx_format_reset(&fb);

	oryx_format(&fb, "%12s", "ipv6");
	for (lcore = 0; lcore < vm->nb_lcores; lcore ++) {
		if(counter_ipv6[lcore] == 0)
			oryx_format(&fb, "%20s", "-");
		else
			oryx_format(&fb, "%20llu", counter_ipv6[lcore]);
	}
	fprintf(fp, "%s%s", FMT_DATA(fb), "\n");
	oryx_format_reset(&fb);

	oryx_format(&fb, "%12s", "tcp");
	for (lcore = 0; lcore < vm->nb_lcores; lcore ++) {
		if(counter_tcp[lcore] == 0)
			oryx_format(&fb, "%20s", "-");
		else
			oryx_format(&fb, "%20llu", counter_tcp[lcore]);
	}
	fprintf(fp, "%s%s", FMT_DATA(fb), "\n");
	oryx_format_reset(&fb);

	oryx_format(&fb, "%12s", "udp");
	for (lcore = 0; lcore < vm->nb_lcores; lcore ++) {
		if(counter_udp[lcore] == 0)
			oryx_format(&fb, "%20s", "-");
		else
			oryx_format(&fb, "%20llu", counter_udp[lcore]);
	}
	fprintf(fp, "%s%s", FMT_DATA(fb), "\n");
	oryx_format_reset(&fb);

	oryx_format(&fb, "%12s", "sctp");
	for (lcore = 0; lcore < vm->nb_lcores; lcore ++) {
		if(counter_sctp[lcore] == 0)
			oryx_format(&fb, "%20s", "-");
		else
			oryx_format(&fb, "%20llu", counter_sctp[lcore]);
	}
	fprintf(fp, "%s%s", FMT_DATA(fb), "\n");
	oryx_format_reset(&fb);

	oryx_format(&fb, "%12s", "http");
	for (lcore = 0; lcore < vm->nb_lcores; lcore ++) {
		if(counter_http[lcore] == 0)
			oryx_format(&fb, "%20s", "-");
		else
			oryx_format(&fb, "%20llu", counter_http[lcore]);
	}
	fprintf(fp, "%s%s", FMT_DATA(fb), "\n");
	oryx_format_reset(&fb);

	oryx_format(&fb, "%12s", "arp");
	for (lcore = 0; lcore < vm->nb_lcores; lcore ++) {
		if(counter_arp[lcore] == 0)
			oryx_format(&fb, "%20s", "-");
		else
			oryx_format(&fb, "%20llu", counter_arp[lcore]);
	}
	fprintf(fp, "%s%s", FMT_DATA(fb), "\n");
	oryx_format_reset(&fb);

	oryx_format(&fb, "%12s", "icmpv4");
	for (lcore = 0; lcore < vm->nb_lcores; lcore ++) {
		if(counter_icmpv4[lcore] == 0)
			oryx_format(&fb, "%20s", "-");
		else
			oryx_format(&fb, "%20llu", counter_icmpv4[lcore]);
	}
	fprintf(fp, "%s%s", FMT_DATA(fb), "\n");
	oryx_format_reset(&fb);

	oryx_format(&fb, "%12s", "icmpv6");
	for (lcore = 0; lcore < vm->nb_lcores; lcore ++) {
		if(counter_icmpv6[lcore] == 0)
			oryx_format(&fb, "%20s", "-");
		else
			oryx_format(&fb, "%20llu", counter_icmpv6[lcore]);
	}
	fprintf(fp, "%s%s", FMT_DATA(fb), "\n");
	oryx_format_reset(&fb); 

	oryx_format(&fb, "%12s", "drop");
	for (lcore = 0; lcore < vm->nb_lcores; lcore ++) {
		if(counter_drop[lcore] == 0)
			oryx_format(&fb, "%20s", "-");
		else
			oryx_format(&fb, "%20llu", counter_drop[lcore]);
	}
	fprintf(fp, "%s%s", FMT_DATA(fb), "\n");
	oryx_format_reset(&fb); 


	oryx_format_free(&fb);

	gettimeofday(&end, NULL);
	fprintf(fp, ", cost %lu us%s", oryx_elapsed_us(&start, &end), "\n");

	fflush(fp);
	fclose(fp);
	
	return;
}


