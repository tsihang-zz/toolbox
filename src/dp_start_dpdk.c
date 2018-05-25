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

#define US_PER_SEC (US_PER_MS * MS_PER_S)

extern dp_private_t dp_private_main;
extern ThreadVars g_tv[];
extern DecodeThreadVars g_dtv[];
extern PacketQueue g_pq[];
extern bool force_quit;
extern uint64_t timer_period;

extern void
dp_register_perf_counters(DecodeThreadVars *dtv, ThreadVars *tv);


extern void dpdk_env_setup(vlib_main_t *vm);

/* Send burst of packets on an output interface */
static inline int
__forward_burst(ThreadVars *tv, DecodeThreadVars *dtv, 
				struct LcoreConfigContext *lconf, uint16_t n, uint8_t port)
{
	struct rte_mbuf **m_table;
	int ret;
	uint16_t queueid;
	vlib_port_main_t *vp = &vlib_port_main;
	struct iface_t *iface;
	int qua = QUA_COUNTER_TX;

	iface_lookup_id(vp, port, &iface);
	if(!iface) {
		oryx_panic(-1, "Cannot locate a tx_port[%d].", port);
	}

	queueid = 0;
	m_table = (struct rte_mbuf **)lconf->tx_burst[port].burst;

	ret = rte_eth_tx_burst(port, queueid, m_table, n);
	if (unlikely(ret < n)) {
		do {
			rte_pktmbuf_free(m_table[ret]);
		} while (++ret < n);
	}

	iface_counters_add(iface, iface->if_counter_ctx->counter_pkts[qua], tv->n_tx_packets);
	//iface_counters_add(iface, iface->if_counter_ctx->counter_bytes[qua], tv->n_tx_bytes);

	return 0;
}

static inline int forward_out(ThreadVars *tv, DecodeThreadVars *dtv, 
				Packet *p, struct rte_mbuf *mbuf, u8 rx_port, u8 tx_port, PacketQueue *pq)
{	
	uint16_t n_tx_try_burst;
	int n_tx_burst = 0;	
	struct rte_mbuf **m;
	struct LcoreConfigContext *lconf = &lconf_ctx[tv->lcore];
	int qua = QUA_COUNTER_TX;
	struct iface_t *iface;
	vlib_port_main_t *vp = &vlib_port_main;

	iface_lookup_id(vp, tx_port, &iface);
	if(!iface) {
		oryx_panic(-1, "Cannot locate a tx_port[%d].", tx_port);
	}


	if(p->dsah) {
		oryx_logd("from GE ...");
		u32 new_dsa = 0;
		MarvellDSAHdr *dsah = p->dsah;
		u32 dsa = p->dsa;
		/** DSA TAG means that this frame come from a switch port (ge1 ... ge8). */
		SET_DSA_CMD(new_dsa, DSA_TAG_FORWARD);
		SET_DSA_TAG(new_dsa, DSA_TAG(dsa));
		SET_DSA_PORT(new_dsa, 1); /* send to ge5. */
		SET_DSA_R2(new_dsa, DSA_R2(dsa));
		SET_DSA_R1(new_dsa,0);
		SET_DSA_PRI(new_dsa, DSA_PRI(dsa));
		SET_DSA_R0(new_dsa, 0);
		SET_DSA_VLAN(new_dsa, DSA_VLAN(dsa));
		/* overwrite DSA tag with the new one. */
		dsah->dsa = hton32(new_dsa);
		//PrintDSA("Tx", new_dsa, QUA_TX);
	} else {
		oryx_logd("no DSA header ...");
	}
	
	n_tx_try_burst = lconf->tx_burst[tx_port].n_tx_burst_len;
	lconf->tx_burst[tx_port].burst[n_tx_try_burst] = mbuf;
	n_tx_try_burst ++;

	/** Provisional satistics hold by this threadvar. */
	tv->n_tx_bytes_prov += GET_PKT_LEN(p);

	/** reach maximum burst threshold. */
	if (likely(n_tx_try_burst == DPDK_MAX_TX_BURST)) {
		oryx_logd("trying tx_burst ... %d\n", n_tx_try_burst);
		m = (struct rte_mbuf **)lconf->tx_burst[tx_port].burst;
		tv->n_tx_packets_prov = n_tx_burst = rte_eth_tx_burst(tx_port, 0, m, n_tx_try_burst);
		if (unlikely(n_tx_burst < n_tx_try_burst)) {
			oryx_logi("%d not sent \n", (n_tx_try_burst - n_tx_burst));
			do {
				tv->n_tx_bytes_prov -= m[n_tx_burst]->pkt_len;
				rte_pktmbuf_free(m[n_tx_burst]);
			} while (++ n_tx_burst < n_tx_try_burst);
		}
		
		tv->n_tx_bytes += tv->n_tx_bytes_prov;
		tv->n_tx_packets += tv->n_tx_packets_prov;		
		//iface_counters_add(iface, iface->if_counter_ctx->counter_pkts[qua], tv->n_tx_packets);
		//iface_counters_add(iface, iface->if_counter_ctx->counter_bytes[qua], tv->n_tx_bytes);

		n_tx_try_burst = 0;
		tv->n_tx_bytes_prov = 0;
		tv->n_tx_packets_prov = 0;
	}
	/** reset counter. */
	lconf->tx_burst[tx_port].n_tx_burst_len = n_tx_try_burst;
	return 0;
}

static int
dp_dpdk_running_fn0(void *ptr_data)
{
	vlib_main_t *vm = (vlib_main_t *)ptr_data;
	dpdk_main_t *dm = &dpdk_main;
	struct LcoreConfigContext *lconf;
	unsigned i, j, portid, nb_rx;
	struct rte_mbuf *rx_pkts_burst[DPDK_MAX_RX_BURST];
	struct rte_mbuf *m;
	Packet *p;
	uint8_t *pkt;
	ThreadVars *tv = &g_tv[rte_lcore_id() % RTE_MAX_LCORE];
	DecodeThreadVars *dtv = &g_dtv[rte_lcore_id() % RTE_MAX_LCORE];
	PacketQueue *pq = &g_pq[rte_lcore_id() % RTE_MAX_LCORE];
	u32 n_rx_port = 0;
	struct oryx_fmt_buff_t fb = FMT_BUFF_INITIALIZATION;
	struct iface_t *iface;
	vlib_port_main_t *vp = &vlib_port_main;
	int qua = QUA_COUNTER_RX;
	uint64_t prev_tsc, diff_tsc, cur_tsc;
	const uint64_t drain_tsc = (rte_get_tsc_hz() + US_PER_SEC - 1) /
		US_PER_SEC * BURST_TX_DRAIN_US;

	tv->lcore = rte_lcore_id();
	lconf = &lconf_ctx[tv->lcore];
	
	while (!vm->cli_ready) {
		oryx_logn("lcore %d wait for CLI ...", tv->lcore);
		sleep(1);
	}
	
	oryx_format(&fb, "entering main loop on lcore %u, total %u port(s)\n", tv->lcore, lconf->n_rx_port);
	for (n_rx_port = 0; n_rx_port < lconf->n_rx_port; n_rx_port ++) {
		oryx_format(&fb, "	.... rx_port %u", lconf->rx_port_list[n_rx_port]);
	}
	oryx_logn("%s", FMT_DATA(fb));
	oryx_format_free(&fb);

	while (!force_quit) {

		/*
		 * Read packet from RX queues
		 */
		for (i = 0; i < lconf->n_rx_port; i++) {
			
			portid = lconf->rx_port_list[i];
				
			nb_rx = rte_eth_rx_burst((uint8_t) portid, 0,
						 rx_pkts_burst, DPDK_MAX_RX_BURST);
			if (nb_rx == 0)
				continue;

			tv->n_rx_packets += nb_rx;
			
			//iface_lookup_id(vp, portid, &iface);
			//if(!iface) {
				//oryx_panic(-1, "Cannot locate a port.");
			//}

			for (j = 0; j < nb_rx; j++) {
				m = rx_pkts_burst[j];
				rte_prefetch0(rte_pktmbuf_mtod(m, void *));

				p = get_priv(m);
				pkt = (uint8_t *)rte_pktmbuf_mtod(m, void *);
				tv->n_rx_bytes += m->pkt_len;
				SET_PKT_LEN(p, m->pkt_len);
				SET_PKT(p, pkt);
				SET_PKT_SRC_PHY(p, portid);
				//p->iface[QUA_RX] = iface;
				/** stats */
				//iface_counters_inc(iface, iface->if_counter_ctx->counter_pkts[qua]);
				//iface_counters_add(iface, iface->if_counter_ctx->counter_bytes[qua], GET_PKT_LEN(p));

				//DecodeEthernet0(tv, dtv, p, pkt, m->pkt_len, pq);

				//DecodeUpdateCounters(tv, dtv, p);

				/** StatsINCR called within PakcetDecodeFinalize. */
				//PacketDecodeFinalize(tv, dtv, p);
			
				u32 rx_port = portid;
				u32 tx_port = portid;	/** loopback. */
				SET_PKT_DST_PHY(p, tx_port);
				forward_out(tv, dtv, p, m, rx_port, tx_port, pq);
			}
		}
	}

	oryx_logn ("dp_main[%d] exiting ...", tv->lcore);
	return 0;
}

static inline void simple_loopback(ThreadVars *tv, struct rte_mbuf *m, unsigned portid)
{
	unsigned dst_port;
	int sent;
	struct rte_eth_dev_tx_buffer *buffer;
	dpdk_main_t *dm = &dpdk_main;

	/** loopback. */
	dst_port = portid;;

	buffer = dm->tx_buffer[dst_port];
	/** hold m in tx_buffer. */
	sent = rte_eth_tx_buffer(dst_port, 0, buffer, m);
	if (sent)
		tv->n_tx_packets += sent;

}

static int
dp_dpdk_running_fn(void *ptr_data)
{
	vlib_main_t *vm = (vlib_main_t *)ptr_data;
	dpdk_main_t *dm = &dpdk_main;
	struct LcoreConfigContext *lconf;
	unsigned i, j, portid, nb_rx;
	struct rte_mbuf *rx_pkts_burst[DPDK_MAX_RX_BURST];
	struct rte_mbuf *m;
	Packet *p;
	uint8_t *pkt;
	ThreadVars *tv = &g_tv[rte_lcore_id() % RTE_MAX_LCORE];
	DecodeThreadVars *dtv = &g_dtv[rte_lcore_id() % RTE_MAX_LCORE];
	PacketQueue *pq = &g_pq[rte_lcore_id() % RTE_MAX_LCORE];
	u32 n_rx_port = 0;
	struct oryx_fmt_buff_t fb = FMT_BUFF_INITIALIZATION;
	struct iface_t *iface;
	vlib_port_main_t *vp = &vlib_port_main;
	int qua = QUA_COUNTER_RX;
	uint64_t prev_tsc, diff_tsc, cur_tsc, timer_tsc;
	const uint64_t drain_tsc = (rte_get_tsc_hz() + US_PER_SEC - 1) /
		US_PER_SEC * BURST_TX_DRAIN_US;
	struct rte_eth_dev_tx_buffer *buffer;
	u32 nb_tx_packets;
	uint16_t pkt_len;
	
	prev_tsc = 0;
	timer_tsc = 0;

	tv->lcore = rte_lcore_id();
	lconf = &lconf_ctx[tv->lcore];
	
	while (!vm->cli_ready) {
		oryx_logn("lcore %d wait for CLI ...", tv->lcore);
		sleep(1);
	}
	
	oryx_format(&fb, "entering main loop on lcore %u, total %u port(s)\n", tv->lcore, lconf->n_rx_port);
	for (n_rx_port = 0; n_rx_port < lconf->n_rx_port; n_rx_port ++) {
		oryx_format(&fb, "	.... rx_port %u", lconf->rx_port_list[n_rx_port]);
	}
	oryx_logn("%s", FMT_DATA(fb));
	oryx_format_free(&fb);

	while (!force_quit) {
		cur_tsc = rte_rdtsc();

		/*
		 * TX burst queue drain
		 */
		diff_tsc = cur_tsc - prev_tsc;
		if (unlikely(diff_tsc > drain_tsc)) {

			for (i = 0; i < lconf->n_rx_port; i++) {
				/** loopback. */
				portid = lconf->rx_port_list[i]; //l2fwd_dst_ports[lconf->rx_port_list[i]];
				buffer = dm->tx_buffer[portid];

				nb_tx_packets = rte_eth_tx_buffer_flush(portid, 0, buffer);
				if(nb_tx_packets)
					tv->n_tx_packets += nb_tx_packets;
			}

			/* if timer is enabled */
			if (timer_period > 0) {

				/* advance the timer */
				timer_tsc += diff_tsc;

				/* if timer has reached its timeout */
				if (unlikely(timer_tsc >= timer_period)) {

					/* do this only on a sepcific core */
					if (tv->lcore == 1) {
						//printf ("%llu %llu\n", tv->n_rx_packets, tv->n_tx_packets);
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
		for (i = 0; i < lconf->n_rx_port; i++) {
			
			portid = lconf->rx_port_list[i];
				
			nb_rx = rte_eth_rx_burst((uint8_t) portid, 0,
						 rx_pkts_burst, DPDK_MAX_RX_BURST);
			if (nb_rx == 0)
				continue;

			tv->n_rx_packets += nb_rx;

			/** port ID -> iface */
			iface_lookup_id(vp, portid, &iface);
			iface_counters_add(iface, iface->if_counter_ctx->counter_pkts[qua], nb_rx);

			for (j = 0; j < nb_rx; j++) {
				m = rx_pkts_burst[j];
				rte_prefetch0(rte_pktmbuf_mtod(m, void *));
				pkt_len = m->pkt_len;
				pkt = (uint8_t *)rte_pktmbuf_mtod(m, void *);				
				p = get_priv(m);
				SET_PKT_LEN(p, pkt_len);
				SET_PKT(p, pkt);
				SET_PKT_SRC_PHY(p, portid);
				iface_counters_add(iface, iface->if_counter_ctx->counter_bytes[qua], pkt_len);
				tv->n_rx_bytes += pkt_len;
				DecodeEthernet0(tv, dtv, p, pkt, pkt_len, pq);
				DecodeUpdateCounters(tv, dtv, p);
				PacketDecodeFinalize(tv, dtv, p);
				simple_loopback(tv, m, portid);
			}
			
		}
	}

	oryx_logn ("dp_main[%d] exiting ...", tv->lcore);
	return 0;
}

void dp_end_dpdk(struct vlib_main_t *vm)
{
	dpdk_main_t *dm = &dpdk_main;
	u8 portid;
	
	for (portid = 0; portid < dm->n_ports; portid++) {
		if ((dm->conf->portmask & (1 << portid)) == 0)
			continue;
		printf("Closing port %d...", portid);
		rte_eth_dev_stop(portid);
		rte_eth_dev_close(portid);
		printf(" Done\n");
	}
	printf("Bye...\n");
}

static __oryx_always_inline__ 
void dp_dpdk_perf_tmr_handler(struct oryx_timer_t *tmr, int __oryx_unused__ argc, 
                char **argv)
{
	tmr = tmr;
	argc = argc;
	argv = argv;
}

static int dp_dpdk_free(void *mbuf)
{
	mbuf = mbuf;
	return 0;
}


static const char *enp5s0fx[] = {
	"enp5s0f1",
	"enp5s0f2",
	"enp5s0f3"
};

static int dp_dpdk_check_port(struct vlib_main_t *vm)
{
	u8 portid;
	struct iface_t *this;
	vlib_port_main_t *vp = &vlib_port_main;
	int n_ports_now = vec_count(vp->entry_vec);

	if(!(vm->ul_flags & VLIB_PORT_INITIALIZED)) {
		oryx_panic(-1, "Run port initialization first.");
	}
	
	/* register to vlib_port_main. */
	for (portid = 0; portid < vm->nb_ports; portid ++) {
		iface_lookup_id(vp, portid, &this);
		if(!this) {
			oryx_panic(-1, "no such ethdev.");
		}
		if(strcmp(this->sc_alias_fixed, enp5s0fx[portid])) {
			oryx_panic(-1, "no such ethdev named %s.", this->sc_alias_fixed);
		}
	}

	return 0;
}

void dp_start_dpdk(struct vlib_main_t *vm) {
	dpdk_main_t *dm = &dpdk_main;
	int i;
	char thrgp_name[128] = {0}; 
	ThreadVars *tv;
	DecodeThreadVars *dtv;

	dm->conf->mempool_priv_size = vm->extra_priv_size;
	vm->dp_running_fn = dp_dpdk_running_fn;
	
	dp_dpdk_check_port(vm);
	dpdk_env_setup(vm);
	
	printf ("Master Lcore @ %d/%d\n", rte_get_master_lcore(),
		vm->nb_lcores);
	
	for (i = 0; i < vm->nb_lcores; i ++) {
		tv = &g_tv[i];
		dtv = &g_dtv[i];
		sprintf (thrgp_name, "dp[%u] hd-thread", i);
		tv->thread_group_name = strdup(thrgp_name);
		SC_ATOMIC_INIT(tv->flags);
		pthread_mutex_init(&tv->perf_private_ctx0.m, NULL);
		dp_register_perf_counters(dtv, tv);
		tv->free_fn = dp_dpdk_free;
	}
	
	uint32_t ul_perf_tmr_setting_flags = TMR_OPTIONS_PERIODIC | TMR_OPTIONS_ADVANCED;
	vm->perf_tmr = oryx_tmr_create (1, "dp_perf_tmr", ul_perf_tmr_setting_flags,
											  dp_dpdk_perf_tmr_handler, 0, NULL, 3000);

	/* launch per-lcore init on every lcore */
	rte_eal_mp_remote_launch(dp_dpdk_running_fn, vm, SKIP_MASTER);
}




