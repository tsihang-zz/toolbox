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

#include "dpdk_classify.h"

/* Configure how many packets ahead to prefetch, when reading packets */
#define PREFETCH_OFFSET	  3

extern volatile bool force_quit;
extern ThreadVars g_tv[];
extern DecodeThreadVars g_dtv[];
extern PacketQueue g_pq[];


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
	oryx_logd("tx_port_id=%d tx_queue_id %d, tx_lcore_id %d tx_pkts_num %d", tx_port_id, tx_queue_id, tv->lcore, n_tx_pkts);
#endif

	iface_counters_add(iface, iface->if_counter_ctx->lcore_counter_pkts[QUA_TX][qconf->lcore_id], n_tx_pkts);

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
void dsa_tag_strip(struct rte_mbuf *m)
{
	struct ether_hdr *eh
		 = rte_pktmbuf_mtod(m, struct ether_hdr *);

	/* Copy ether header over rather than moving whole packet */
	memmove(rte_pktmbuf_adj(m, sizeof(struct vlan_hdr)),
		eh, 2 * ETHER_ADDR_LEN);
}

static __oryx_always_inline__
int dsa_tag_insert(struct rte_mbuf **m, uint32_t new_cpu_dsa)
{
	MarvellDSAEthernetHdr *nh;
	struct ether_hdr *oh;

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

	SET_DSA_CMD(new_cpu_dsa, DSA_CMD_EGRESS); 		/** which port this packet should be send out. */
	SET_DSA_PORT(new_cpu_dsa, tx_virtual_port_id);  /** update dsa.port */
	SET_DSA_CFI(new_cpu_dsa, DSA_CFI(cpu_tag));		/** update dsa.C dsa.R1, dsa.R2 */
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
void simple_forward(ThreadVars *tv, Packet *p, uint16_t pkt_len,
		struct rte_mbuf *m, struct iface_t *rx_cpu_iface, struct lcore_conf *qconf)
{
	uint8_t tx_port_id = -1, tx_panel_port_id = -1;
	struct ipv4_hdr *ipv4h;
	uint8_t dst_port;
	uint32_t tcp_or_udp;
	uint32_t l3_ptypes;
	vlib_port_main_t *vp = &vlib_port_main;
	struct iface_t *tx_panel_iface;
	
#if defined(BUILD_DEBUG)
	BUG_ON(rx_cpu_iface == NULL);
#endif

	/** its an invalid packet. */
	if (p->flags & PKT_IS_INVALID) {
		rte_pktmbuf_free(m);
		return;
	}
	
	tcp_or_udp = m->packet_type & (RTE_PTYPE_L4_TCP | RTE_PTYPE_L4_UDP);
	l3_ptypes = m->packet_type & RTE_PTYPE_L3_MASK;

	/** which port this packet should be send out. */
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

		/** exact match */
		tx_panel_port_id = em_get_ipv4_dst_port(ipv4h);

		/** lpm */
		//tx_port_id = lpm_get_ipv4_dst_port(ipv4h, rx_cpu_iface, qconf->ipv4_lookup_struct);

#if defined(BUILD_DEBUG)
		struct udp_hdr *udph = rte_pktmbuf_mtod_offset(m, struct udp_hdr *,
						   (sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr)));		
		oryx_logn("rx_port_id %d  tx_panel_port_id %d",
				iface_id(rx_cpu_iface), tx_panel_port_id);
#endif

	}

	tx_panel_port_id = 1;	/** send to xe */
	tx_panel_port_id = 5;   /** send to ge */
	if (DSA_IS_INGRESS(p->dsa)) {
		MarvellDSAEthernetHdr *dsaeth = rte_pktmbuf_mtod(m, MarvellDSAEthernetHdr *);
		if (tx_panel_port_id > SW_PORT_OFFSET) /** SW -> SW */ {
			uint32_t new_cpu_dsa = dsa_tag_update(p->dsa, PANEL_GE_ID_TO_DSA(tx_panel_port_id));		
			iface_lookup_id(vp, tx_panel_port_id, &tx_panel_iface);
		#if defined(BUILD_DEBUG)
			BUG_ON(tx_panel_iface == NULL);
		#endif
			iface_counters_inc(tx_panel_iface, tx_panel_iface->if_counter_ctx->lcore_counter_pkts[QUA_TX][tv->lcore]);
			iface_counters_add(tx_panel_iface, tx_panel_iface->if_counter_ctx->lcore_counter_bytes[QUA_TX][tv->lcore], pkt_len);
			dsaeth->dsah.dsa = rte_cpu_to_be_32(new_cpu_dsa);
			tx_port_id = 0;	/** tx_port_id 0 is a cpu interface connected with SW unit. */
		} else {
			/* SW -> XE */
			dsa_tag_strip(m);
			tx_port_id = tx_panel_port_id;
		}
	} else {
		if (tx_panel_port_id > SW_PORT_OFFSET) /* XE -> SW */ {
			/* add a dsa tag */
			uint32_t new_cpu_dsa = dsa_tag_update(0, PANEL_GE_ID_TO_DSA(tx_panel_port_id));
			dsa_tag_insert(&m, new_cpu_dsa);
			iface_lookup_id(vp, tx_panel_port_id, &tx_panel_iface);
		#if defined(BUILD_DEBUG)
			BUG_ON(tx_panel_iface == NULL);
		#endif
			iface_counters_inc(tx_panel_iface, tx_panel_iface->if_counter_ctx->lcore_counter_pkts[QUA_TX][tv->lcore]);
			iface_counters_add(tx_panel_iface, tx_panel_iface->if_counter_ctx->lcore_counter_bytes[QUA_TX][tv->lcore], pkt_len);
			tx_port_id = 0; /** tx_port_id 0 is a cpu interface connected with SW unit. */
		} else {
			/* XE -> XE */
			tx_port_id = tx_panel_port_id;
		}
	}
	
	send_single_packet(tv, qconf, m, tx_port_id);
}

/*
 * Buffer non-optimized handling of packets, invoked
 * from main_loop.
 */
static __oryx_always_inline__
void no_opt_send_packets(ThreadVars *tv,
			int nb_rx, struct rte_mbuf **pkts_burst,
			struct iface_t *rx_cpu_iface, struct iface_t *rx_sw_iface, struct lcore_conf *qconf)
{
	int32_t j = 0;
	Packet *p;
#if defined(BUILD_DEBUG)
	BUG_ON(rx_cpu_iface == NULL);
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
	for (; j < nb_rx; j++) {
		p = GET_MBUF_PRIVATE(Packet, pkts_burst[j]);
		simple_forward(tv, p, pkts_burst[j]->pkt_len, pkts_burst[j], rx_cpu_iface, qconf);
	}
}

static __oryx_always_inline__
void standard_eth_frame(ThreadVars *tv, DecodeThreadVars *dtv, Packet *p,
				  uint8_t *pkt, uint16_t pkt_len, PacketQueue *pq, struct rte_mbuf *m, struct iface_t *rx_cpu_iface)
{
	oryx_logd("Standard ethernet ...");
	struct ether_hdr *ethh;
	struct ipv4_hdr *ipv4h;
	struct ipv6_hdr *ipv6h;
	uint32_t packet_type = RTE_PTYPE_UNKNOWN;
	uint16_t ether_type;
	void *l3;
	int hdr_len;
	vlib_port_main_t *vp = &vlib_port_main;
	struct iface_t *rx_sw_iface;

	oryx_counter_add(&tv->perf_private_ctx0, dtv->counter_bytes, pkt_len);
	iface_counters_add(rx_cpu_iface, rx_cpu_iface->if_counter_ctx->lcore_counter_bytes[QUA_RX][tv->lcore], pkt_len);

	ethh = rte_pktmbuf_mtod(m, struct ether_hdr *);
	ether_type = ethh->ether_type;

#if defined(BUILD_DEBUG)
		oryx_logn("%s pkt_len %d virtual_port %d rx_port_id %d ether_type %04x",
			"ingress",
			pkt_len,
			m->port,
			GET_PKT_SRC_PHY(p),
			rte_be_to_cpu_16(ether_type));
#endif
	l3 = (uint8_t *)ethh + sizeof(struct ether_hdr);
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
 
static __oryx_always_inline__
void marvell_dsa_frame(ThreadVars *tv, DecodeThreadVars *dtv, Packet *p,
				  uint8_t *pkt, uint16_t pkt_len, PacketQueue *pq, struct rte_mbuf *m, struct iface_t *rx_cpu_iface)
{
	oryx_logd("Marvell DSA ...");

	struct ether_hdr *ethh;
	struct ipv4_hdr *ipv4h;
	struct ipv6_hdr *ipv6h;
	uint32_t packet_type = RTE_PTYPE_UNKNOWN;
	uint16_t ether_type;
	void *l3;
	int hdr_len;
	vlib_port_main_t *vp = &vlib_port_main;
	MarvellDSAEthernetHdr *dsaeth;
	struct iface_t *rx_sw_iface;

	oryx_counter_add(&tv->perf_private_ctx0, dtv->counter_bytes, pkt_len);
	iface_counters_add(rx_cpu_iface, rx_cpu_iface->if_counter_ctx->lcore_counter_bytes[QUA_RX][tv->lcore], pkt_len);

	dsaeth = rte_pktmbuf_mtod(m, MarvellDSAEthernetHdr *);
	p->dsa = rte_be_to_cpu_32(dsaeth->dsah.dsa);

#if defined(BUILD_DEBUG)
	oryx_logn("%s pkt_len %d virtual_port %d rx_port_id %d ether_type %04x dsa %08x, from sw port %d",
		DSA_IS_INGRESS(p->dsa) ? "ingress" : "unknown",
		pkt_len,
		m->port,
		GET_PKT_SRC_PHY(p),
		rte_be_to_cpu_16(dsaeth->eth_type), 
		dsaeth->dsah.dsa,
		DSA_TO_PANEL_GE_ID(p->dsa));
	PrintDSA("RX", p->dsa, QUA_RX);
#endif

	if (!DSA_IS_INGRESS(p->dsa)) {
		SET_PKT_FLAGS(p, PKT_IS_INVALID | PKT_DSA_NOT_INGRESS);
		return;
	}
	
	iface_lookup_id(vp, DSA_TO_GLOBAL_PORT_ID(p->dsa), &rx_sw_iface);
#if defined(BUILD_DEBUG)
	BUG_ON(rx_sw_iface == NULL);
#endif
	iface_counters_inc(rx_sw_iface, rx_sw_iface->if_counter_ctx->lcore_counter_pkts[QUA_RX][tv->lcore]);
	iface_counters_add(rx_sw_iface, rx_sw_iface->if_counter_ctx->lcore_counter_bytes[QUA_RX][tv->lcore], pkt_len);

	l3 = (uint8_t *)dsaeth + sizeof(MarvellDSAEthernetHdr);
	ether_type = dsaeth->eth_type;
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
	} else if (ether_type == rte_cpu_to_be_16(ETHER_TYPE_ARP)) {
		oryx_counter_inc(&tv->perf_private_ctx0, dtv->counter_arp);
	}

	m->packet_type = packet_type;

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
	DecodeThreadVars *dtv;
	struct iface_t *rx_cpu_iface;
	vlib_port_main_t *vp = &vlib_port_main;
	int qua = QUA_RX;
	uint16_t pkt_len;
	uint8_t *pkt;
	Packet *p;
	struct ether_hdr *ethh;
	struct ipv4_hdr *ipv4h;
	struct ipv6_hdr *ipv6h;
	uint32_t packet_type = RTE_PTYPE_UNKNOWN;
	uint16_t ether_type;
	void *l3;
	int hdr_len;


	const uint64_t drain_tsc = (rte_get_tsc_hz() + US_PER_S - 1) /
		US_PER_S * BURST_TX_DRAIN_US;
	
	prev_tsc = 0;

	lcore_id = rte_lcore_id();

	tv = &g_tv[lcore_id];
	dtv = &g_dtv[lcore_id];
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

#if defined(BUILD_DEBUG)
			oryx_logn("rx_port_id %d, rx_queue_id %d, rx_core_id %d",
				rx_port_id, rx_queue_id, tv->lcore);
#endif			
			/** port ID -> iface */
			iface_lookup_id(vp, rx_port_id, &rx_cpu_iface);			
			iface_counters_add(rx_cpu_iface, rx_cpu_iface->if_counter_ctx->lcore_counter_pkts[QUA_RX][lcore_id], nb_rx);
			oryx_counter_add(&tv->perf_private_ctx0, dtv->counter_pkts, nb_rx);

			/** packet come from sw unit. */
			if(iface_support_marvell_dsa(rx_cpu_iface)) {
				oryx_counter_add(&tv->perf_private_ctx0, dtv->counter_dsa, nb_rx);
				for (j = 0; j < nb_rx; j++) {
					m = pkts_burst[j];
					rte_prefetch0(rte_pktmbuf_mtod(m, void *));	
					/** it costs a lot when calling Packet from mbuf. */
					p = GET_MBUF_PRIVATE(Packet, m);
					pkt = (uint8_t *)rte_pktmbuf_mtod(m, void *);
					pkt_len = m->pkt_len;
					SET_PKT_LEN(p, pkt_len);
					SET_PKT(p, pkt);
					SET_PKT_SRC_PHY(p, rx_port_id); 
					marvell_dsa_frame(tv, dtv, p, pkt, pkt_len, NULL, m, rx_cpu_iface);
					simple_forward(tv, p, pkt_len, m, rx_cpu_iface,qconf);
				}				
			} else {
				oryx_counter_add(&tv->perf_private_ctx0, dtv->counter_eth, nb_rx);
				for (j = 0; j < nb_rx; j++) {
					m = pkts_burst[j];
					rte_prefetch0(rte_pktmbuf_mtod(m, void *));
					/** it costs a lot when calling Packet from mbuf. */
					p = GET_MBUF_PRIVATE(Packet, m);
					pkt = (uint8_t *)rte_pktmbuf_mtod(m, void *);
					pkt_len = m->pkt_len;
					SET_PKT_LEN(p, pkt_len);
					SET_PKT(p, pkt);
					SET_PKT_SRC_PHY(p, rx_port_id); 
					standard_eth_frame(tv, dtv, p, pkt, pkt_len, NULL, m, rx_cpu_iface);
					simple_forward(tv, p, pkt_len, m, rx_cpu_iface, qconf);
				}
			}
			//no_opt_send_packets(tv, nb_rx, pkts_burst, rx_cpu_iface, rx_sw_iface /**always null*/, qconf);
		}
	}

	return 0;
}

