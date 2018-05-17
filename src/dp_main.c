#include "oryx.h"
#include "dp_decode.h"
#include "dpdk.h"

ThreadVars g_tv[MAX_LCORES];
DecodeThreadVars g_dtv[MAX_LCORES];
PacketQueue g_pq[MAX_LCORES];

extern void DecodeUpdateCounters(ThreadVars *tv,
                                const DecodeThreadVars *dtv, const Packet *p);

typedef struct _dp_args_t {
	ThreadVars tv[MAX_LCORES];
	DecodeThreadVars dtv[MAX_LCORES];
	PacketQueue pq[MAX_LCORES];
}dp_private_t;


dp_private_t dp_private_main;

static dpdk_config_main_t dpdk_config_main = {
	.nchannels = 2,		/** */		
	.coremask = 0x0f,	/** 4 lcores. */
	.portmask = 0x03,	/** 2 xe ports */
	.n_rx_q_per_lcore = 1,
	.uio_driver_name = (char *)"uio_pci_generic",
	.num_mbufs =		DPDK_DEFAULT_NB_MBUF,
	.cache_size =		DPDK_DEFAULT_CACHE_SIZE,
	.data_room_size =	DPDK_DEFAULT_BUFFER_SIZE
};


static void
netdev_pkt_handler(u_char *argv,
		const struct pcap_pkthdr *pcaphdr,
		u_char *packet)
{
	int lcore = 0; //rte_lcore_id();
	ThreadVars *tv = &g_tv[lcore];
	DecodeThreadVars *dtv = &g_dtv[lcore];
	PacketQueue *pq = &g_pq[lcore];
	Packet *p = PacketGetFromAlloc();
	struct netdev_t *netdev = (struct netdev_t *)argv;

	SET_PKT_LEN(p, pcaphdr->len);
	DecodeEthernet0(tv, dtv, p, 
				packet, pcaphdr->len, pq);

	DecodeUpdateCounters(tv, dtv, p); 

	/** StatsINCR called within PakcetDecodeFinalize. */
	PacketDecodeFinalize(tv, dtv, p);

#if defined(HAVE_FLOW_MGR)
	/** PKT_WANTS_FLOW is set by FlowSetupPacket in every decode routine. */
	if (p->flags & PKT_WANTS_FLOW) {
		//FLOWWORKER_PROFILING_START(p, PROFILE_FLOWWORKER_FLOW);
		FlowHandlePacket(tv, dtv, p);
		if(likely(p->flow != NULL)) {
			DEBUG_ASSERT_FLOW_LOCKED(p->flow);
			if (FlowUpdate0(p) == TM_ECODE_DONE) {
				FLOWLOCK_UNLOCK(p->flow);
				return;
			}
		}
		/* if PKT_WANTS_FLOW is not set, but PKT_HAS_FLOW is, then this is a
			* pseudo packet created by the flow manager. */
	} else if (p->flags & PKT_HAS_FLOW) {
		FLOWLOCK_WRLOCK(p->flow);
		/** lock this flow. */
	}

	oryx_logd("packet %"PRIu64" has flow? %s", p->pcap_cnt, p->flow ? "yes" : "no");

	/** unlock this flow. */
	if (p->flow) {
        DEBUG_ASSERT_FLOW_LOCKED(p->flow);
        FLOWLOCK_UNLOCK(p->flow);
    }
#endif

finish:
	/** recycle packet. */
	if(likely(p)){
        PACKET_RECYCLE(p);
        free(p);
	}
}

static struct netdev_t netdev = {
	.handler = NULL,
	.devname = "enp5s0f4",
	.dispatch = netdev_pkt_handler,
	.private = NULL,
};

static struct oryx_task_t netdev_task =
{
	.module = THIS,
	.sc_alias = "Netdev Task",
	.fn_handler = netdev_cap,
	.ul_lcore = INVALID_CORE,
	.ul_prio = KERNEL_SCHED,
	.argc = 0,
	.argv = &netdev,
	.ul_flags = 0,	/** Can not be recyclable. */
};


static void
DecodeRegisterPerfCounters(DecodeThreadVars *dtv, ThreadVars *tv)
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

static __oryx_always_inline__ 
void perf_tmr_handler(struct oryx_timer_t *tmr, int __oryx_unused__ argc, 
                char **argv)
{
	tmr = tmr;
	argc = argc;
	argv = argv;
}

dpdk_main_t dpdk_main = {
	.vm = NULL,
	.conf = &dpdk_config_main,
	.stat_poll_interval = DPDK_STATS_POLL_INTERVAL,
	.link_state_poll_interval = DPDK_LINK_POLL_INTERVAL,
	.dp_fn = NULL,
	.ext_private = &dp_private_main,
	.force_quit = false,
};

void dp_init(vlib_main_t *vm)
{
	dpdk_main.vm = vm;
	
#define DATAPATH_LOGTYPE	"datapath"
#define DATAPATH_LOGLEVEL	ORYX_LOG_DEBUG
	oryx_log_register(DATAPATH_LOGTYPE);
	oryx_log_set_level_regexp(DATAPATH_LOGTYPE, DATAPATH_LOGLEVEL);
	
	int lcore = 0; //rte_lcore_id();
	ThreadVars *tv = &g_tv[lcore];
	DecodeThreadVars *dtv = &g_dtv[lcore];
	PacketQueue *pq = &g_pq[lcore];

	char thrgp_name[128] = {0};

	sprintf (thrgp_name, "dp[%u] hd-thread", 0);
	tv->thread_group_name = strdup(thrgp_name);
	SC_ATOMIC_INIT(tv->flags);
	pthread_mutex_init(&tv->perf_private_ctx0.m, NULL);

	DecodeRegisterPerfCounters(dtv, tv);

	netdev_open(&netdev);
	oryx_task_registry(&netdev_task);

	
	uint32_t ul_perf_tmr_setting_flags = TMR_OPTIONS_PERIODIC | TMR_OPTIONS_ADVANCED;
	
	dpdk_main.perf_tmr = oryx_tmr_create (1, "perf_tmr", ul_perf_tmr_setting_flags,
											  perf_tmr_handler, 0, (char **)&netdev, 3000);
	vm->ul_flags |= VLIB_DP_INITIALIZED;
}

typedef struct _vlib_dataplane_main_t {
	int (*decoder) (ThreadVars *tv, DecodeThreadVars *dtv, Packet *p,
					   uint8_t *pkt, uint16_t len, PacketQueue *pq);

} vlib_dataplane_main_t;

vlib_dataplane_main_t vlib_dp_main;




