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

#include "dpdk.h"

/* Configure how many packets ahead to prefetch, when reading packets */
#define PREFETCH_OFFSET	  3

extern volatile bool force_quit;
extern ThreadVars g_tv[];
extern DecodeThreadVars g_dtv[];
extern PacketQueue g_pq[];

static __oryx_always_inline__
u32 calc_hash(uint8_t *pkt, size_t pkt_len)
{
	int i = 0;
	u32 h;

	for (i = 0; i < pkt_len; i += 4) {
		h += *(uint32_t *)&pkt[i];
	}
	return h;
}

/* Send burst of packets on an output interface */
static __oryx_always_inline__
int send_burst(ThreadVars *tv, struct lcore_conf *qconf, uint16_t n, uint8_t tx_port_id)
{
	struct rte_mbuf **m_table;
	int ret;
	uint16_t queueid;

	queueid = qconf->tx_queue_id[tx_port_id];
	m_table = (struct rte_mbuf **)qconf->tx_mbufs[tx_port_id].m_table;

	ret = rte_eth_tx_burst(tx_port_id, queueid, m_table, n);
	if (unlikely(ret < n)) {
		do {
			rte_pktmbuf_free(m_table[ret]);
		} while (++ret < n);
	}

	tv->n_tx_packets += ret;

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
	if (unlikely(len == DPDK_MAX_TX_BURST)) {
		send_burst(tv, qconf, DPDK_MAX_TX_BURST, tx_port_id);
		len = 0;
	}

	qconf->tx_mbufs[tx_port_id].len = len;
	return 0;
}

static __oryx_always_inline__
void simple_forward(ThreadVars *tv,
		struct rte_mbuf *m, uint8_t rx_port_id, struct lcore_conf *qconf)
{
	uint8_t tx_port_id;
	
	/** calc dst_port */
	tx_port_id = (rx_port_id);
	/** a test for ge port. */
	tx_port_id = 1;
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
		simple_forward(tv, pkts_burst[j], rx_port_id, qconf);
	}
#endif
	/* Forward remaining prefetched packets */
	for (; j < nb_rx; j++)
		simple_forward(tv, pkts_burst[j], rx_port_id, qconf);
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
	uint8_t portid, queueid;
	struct lcore_conf *qconf;
	ThreadVars *tv;
	DecodeThreadVars *dtv;
	PacketQueue *pq;
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
	dtv = &g_dtv[lcore_id];
	pq = &g_pq[lcore_id];

	tv->lcore = lcore_id;
	qconf = &lcore_conf[lcore_id];

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

		portid = qconf->rx_queue_list[i].port_id;
		queueid = qconf->rx_queue_list[i].queue_id;
		printf(" -- lcoreid=%u portid=%hhu rxqueueid=%hhu --\n",
			lcore_id, portid, queueid);
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
				portid = qconf->tx_port_id[i];
				if (qconf->tx_mbufs[portid].len == 0)
					continue;
				send_burst(tv, qconf,
					qconf->tx_mbufs[portid].len,
					portid);
				qconf->tx_mbufs[portid].len = 0;
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
					}
				}
			}

			prev_tsc = cur_tsc;
		}

		/*
		 * Read packet from RX queues
		 */
		for (i = 0; i < qconf->n_rx_queue; ++i) {
			portid = qconf->rx_queue_list[i].port_id;
			queueid = qconf->rx_queue_list[i].queue_id;
			nb_rx = rte_eth_rx_burst(portid, queueid, pkts_burst,
								DPDK_MAX_RX_BURST);
			if (nb_rx == 0)
				continue;
			
			tv->n_rx_packets += nb_rx;

			/** port ID -> iface */
			iface_lookup_id(vp, portid, &iface);
			iface_counters_add(iface, iface->if_counter_ctx->counter_pkts[qua], nb_rx);
			oryx_logd("Lcore[%d].n_rx_packets = %lu [%d]", tv->lcore, tv->n_rx_packets, nb_rx);
#if 1
			for (j = 0; j < nb_rx; j++) {
				m = pkts_burst[j];
				rte_prefetch0(rte_pktmbuf_mtod(m, void *));
				pkt = (uint8_t *)rte_pktmbuf_mtod(m, void *);
				pkt_len = m->pkt_len;
				p = GET_MBUF_PRIVATE(Packet, m);
				
				iface_counters_add(iface, iface->if_counter_ctx->counter_bytes[qua], pkt_len);
				SET_PKT_LEN(p, pkt_len);
				SET_PKT(p, pkt);
				SET_PKT_SRC_PHY(p, portid);

				tv->n_rx_bytes += pkt_len;

				/** it costs a lot when calling Packet from mbuf. */
				oryx_counter_inc(&tv->perf_private_ctx0, dtv->counter_pkts);
			    oryx_counter_add(&tv->perf_private_ctx0, dtv->counter_bytes, pkt_len);
    			oryx_counter_add(&tv->perf_private_ctx0, dtv->counter_avg_pkt_size, pkt_len);
    			oryx_counter_add(&tv->perf_private_ctx0, dtv->counter_max_pkt_size, pkt_len);				
				if(iface_support_marvell_dsa(iface)) {
					DecodeMarvellDSA0(tv, dtv, p, pkt, pkt_len, pq);
				}else {
					DecodeEthernet0(tv, dtv, p, pkt, pkt_len, pq);
				}
				PacketDecodeFinalize(tv, dtv, p);
			}		
#endif
			no_opt_send_packets(tv, nb_rx, pkts_burst, portid, qconf);
		}
	}

	return 0;
}

