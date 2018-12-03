#include "oryx.h"
#include "dp_decode.h"
#include "mpm.h"


threadvar_ctx_t g_tv[MAX_LCORES];
decode_threadvar_ctx_t g_dtv[MAX_LCORES];
pq_t g_pq[MAX_LCORES];
volatile bool force_quit = false;

#if defined(HAVE_MPM)
int pattern_id;
mpm_ctx_t mpm_ctx;
mpm_threadctx_t mpm_thread_ctx;
PrefilterRuleStore pmq;

static void mpm_install_patterns(mpm_ctx_t *mpm_ctx)
{
	mpm_pattern_add_ci(mpm_ctx, (uint8_t *)"video/flv", strlen("video/flv"),
			0, 0, pattern_id ++, 0, 0);
	mpm_pattern_add_ci(mpm_ctx, (uint8_t *)"video/x-flv", strlen("video/x-flv"),
			0, 0, pattern_id ++, 0, 0);
	mpm_pattern_add_ci(mpm_ctx, (uint8_t *)"video/mp4", strlen("video/mp4"),
			0, 0, pattern_id ++, 0, 0);
	mpm_pattern_add_ci(mpm_ctx, (uint8_t *)"video/mp2t", strlen("video/mp2t"),
			0, 0, pattern_id ++, 0, 0);
	mpm_pattern_add_ci(mpm_ctx, (uint8_t *)"application/mp4", strlen("application/mp4"),
			0, 0, pattern_id ++, 0, 0);
	mpm_pattern_add_ci(mpm_ctx, (uint8_t *)"application/octet-stream", strlen("application/octet-stream"),
			0, 0, pattern_id ++, 0, 0);
	mpm_pattern_add_ci(mpm_ctx, (uint8_t *)"flv-application/octet-stream", strlen("flv-application/octet-stream"),
			0, 0, pattern_id ++, 0, 0); 
}
#endif

#if defined(HAVE_DPDK)
extern void dp_init_dpdk(vlib_main_t *vm);
void dpdk_env_setup(vlib_main_t *vm);
void dp_start_dpdk(vlib_main_t *vm);
void dp_end_dpdk(vlib_main_t *vm);
#endif

static void dp_register_perf_counters(decode_threadvar_ctx_t *dtv, threadvar_ctx_t *tv)
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

	int i = 0;
	for (i = 0; i < DECODE_EVENT_PACKET_MAX; i++) {
		BUG_ON(i != (int)DEvents[i].code);
		dtv->counter_invalid_events[i] = oryx_register_counter(DEvents[i].event_name,
									NULL, &tv->perf_private_ctx0);
	}

	oryx_counter_get_array_range(1, 
		atomic_read(tv->perf_private_ctx0.curr_id), &tv->perf_private_ctx0);

    return;
}

void
notify_dp(vlib_main_t *vm, int signum)
{
	vm = vm;
	if (signum == SIGINT || signum == SIGTERM) {
		fprintf (stdout, "\n\nSignal %d received, preparing to exit...\n",
				signum);
		force_quit = true;
	}
}

void dp_init_mpm(vlib_main_t		*vm)
{
#if defined(HAVE_MPM)
	/* Install MPM Table. */
	mpm_table_setup();

	/* Init MPM context */
	memset(&mpm_ctx, 0, sizeof(mpm_ctx_t));
	mpm_ctx_init(&mpm_ctx, MPM_HS);

	/* Setup PMQ */
	mpm_pmq_setup(&pmq);

	/* Install patterns for VIDEO filter. */
	mpm_install_patterns(&mpm_ctx);

	/* Compare installed patterns. */
	mpm_pattern_prepare(&mpm_ctx);

	/* Init MPM Thread context */
	memset(&mpm_thread_ctx, 0, sizeof(mpm_threadctx_t));
	mpm_threadctx_init(&mpm_thread_ctx, MPM_HS);
#endif
	vm->ul_flags |= VLIB_MPM_INITIALIZED;
	fprintf(stdout, "mpm init done.\n");
}

void dp_start(vlib_main_t *vm)
{
	int i;
	threadvar_ctx_t *tv;
	decode_threadvar_ctx_t *dtv;
	char thrgp_name[128] = {0}; 

#if defined(HAVE_DPDK)
	dp_init_dpdk(vm);
	dp_start_dpdk(vm);
#endif

	/** init thread vars for dataplane. */
	for (i = 0; i < vm->nb_lcores; i ++) {
		tv = &g_tv[i];
		dtv = &g_dtv[i];
		sprintf (thrgp_name, "dp[%u] hd-thread", i);
		tv->thread_group_name = strdup(thrgp_name);
		ATOMIC_INIT(tv->flags);
		pthread_mutex_init(&tv->perf_private_ctx0.m, NULL);
		dp_register_perf_counters(dtv, tv);
	}

	dp_init_mpm(vm);
	
	vm->ul_flags |= VLIB_DP_INITIALIZED;
}

void dp_end(vlib_main_t *vm)
{
#if defined(HAVE_DPDK)
	dp_end_dpdk(vm);
#endif
}

