#include "oryx.h"
#include "dp_decode.h"

ThreadVars g_tv[MAX_LCORES];
DecodeThreadVars g_dtv[MAX_LCORES];
PacketQueue g_pq[MAX_LCORES];

dp_private_t dp_private_main;
bool force_quit = false;

#if defined(HAVE_DPDK)
void dpdk_env_setup(struct vlib_main_t *vm);
void dp_start_dpdk(struct vlib_main_t *vm);
void dp_end_dpdk(struct vlib_main_t *vm);
#endif

void
dp_register_perf_counters(DecodeThreadVars *dtv, ThreadVars *tv)
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

	int i = 0;
	for (i = 0; i < DECODE_EVENT_PACKET_MAX; i++) {
		BUG_ON(i != (int)DEvents[i].code);
		dtv->counter_invalid_events[i] = oryx_register_counter(DEvents[i].event_name,
									NULL, &tv->perf_private_ctx0);
	}

	oryx_counter_get_array_range(1, 
		atomic_read(&tv->perf_private_ctx0.curr_id), &tv->perf_private_ctx0);

    return;
}

void
notify_dp(vlib_main_t *vm, int signum)
{
	vm = vm;
	if (signum == SIGINT || signum == SIGTERM) {
		printf("\n\nSignal %d received, preparing to exit...\n",
				signum);
		force_quit = true;
	}
}

void dp_start(struct vlib_main_t *vm)
{
	printf("===== sizeof(struct Packet)=%d ========\n", sizeof(Packet));

#if defined(HAVE_DPDK)
	dpdk_main.conf->mempool_priv_size = vm->extra_priv_size;
	dpdk_env_setup(vm);
	vm->max_lcores = rte_lcore_count();
#else
	vm->max_lcores = MAX_LCORES;
#endif
	vm->ul_flags |= VLIB_DP_INITIALIZED;


#if defined(HAVE_DPDK)
	dp_start_dpdk(vm);
#else
	dp_start_pcap(vm);
#endif
}

void dp_end(struct vlib_main_t *vm)
{
#if defined(HAVE_DPDK)
	dp_end_dpdk(vm);
#else
	dp_end_pcap(vm);
#endif
}

