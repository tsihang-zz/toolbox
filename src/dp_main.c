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
	.devname = "ens37",
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
    dtv->counter_pkts = StatsRegisterCounter("decoder.pkts", tv);
    dtv->counter_bytes = StatsRegisterCounter("decoder.bytes", tv);
    dtv->counter_invalid = StatsRegisterCounter("decoder.invalid", tv);
    dtv->counter_ipv4 = StatsRegisterCounter("decoder.ipv4", tv);
    dtv->counter_ipv6 = StatsRegisterCounter("decoder.ipv6", tv);
    dtv->counter_eth = StatsRegisterCounter("decoder.ethernet", tv);
    dtv->counter_raw = StatsRegisterCounter("decoder.raw", tv);
    dtv->counter_null = StatsRegisterCounter("decoder.null", tv);
    dtv->counter_sll = StatsRegisterCounter("decoder.sll", tv);
    dtv->counter_tcp = StatsRegisterCounter("decoder.tcp", tv);
    dtv->counter_udp = StatsRegisterCounter("decoder.udp", tv);
    dtv->counter_sctp = StatsRegisterCounter("decoder.sctp", tv);
    dtv->counter_icmpv4 = StatsRegisterCounter("decoder.icmpv4", tv);
    dtv->counter_icmpv6 = StatsRegisterCounter("decoder.icmpv6", tv);
    dtv->counter_ppp = StatsRegisterCounter("decoder.ppp", tv);
    dtv->counter_pppoe = StatsRegisterCounter("decoder.pppoe", tv);
    dtv->counter_gre = StatsRegisterCounter("decoder.gre", tv);
	dtv->counter_arp = StatsRegisterCounter("decoder.arp", tv);
    dtv->counter_vlan = StatsRegisterCounter("decoder.vlan", tv);
    dtv->counter_vlan_qinq = StatsRegisterCounter("decoder.vlan_qinq", tv);
    dtv->counter_ieee8021ah = StatsRegisterCounter("decoder.ieee8021ah", tv);
    dtv->counter_teredo = StatsRegisterCounter("decoder.teredo", tv);
    dtv->counter_ipv4inipv6 = StatsRegisterCounter("decoder.ipv4_in_ipv6", tv);
    dtv->counter_ipv6inipv6 = StatsRegisterCounter("decoder.ipv6_in_ipv6", tv);
    dtv->counter_mpls = StatsRegisterCounter("decoder.mpls", tv);
    dtv->counter_avg_pkt_size = StatsRegisterAvgCounter("decoder.avg_pkt_size", tv);
    dtv->counter_max_pkt_size = StatsRegisterMaxCounter("decoder.max_pkt_size", tv);
    dtv->counter_erspan = StatsRegisterMaxCounter("decoder.erspan", tv);
    dtv->counter_flow_memcap = StatsRegisterCounter("flow.memcap", tv);

    dtv->counter_flow_tcp = StatsRegisterCounter("flow.tcp", tv);
    dtv->counter_flow_udp = StatsRegisterCounter("flow.udp", tv);
    dtv->counter_flow_icmp4 = StatsRegisterCounter("flow.icmpv4", tv);
    dtv->counter_flow_icmp6 = StatsRegisterCounter("flow.icmpv6", tv);

    dtv->counter_defrag_ipv4_fragments =
        StatsRegisterCounter("defrag.ipv4.fragments", tv);
    dtv->counter_defrag_ipv4_reassembled =
        StatsRegisterCounter("defrag.ipv4.reassembled", tv);
    dtv->counter_defrag_ipv4_timeouts =
        StatsRegisterCounter("defrag.ipv4.timeouts", tv);
    dtv->counter_defrag_ipv6_fragments =
        StatsRegisterCounter("defrag.ipv6.fragments", tv);
    dtv->counter_defrag_ipv6_reassembled =
        StatsRegisterCounter("defrag.ipv6.reassembled", tv);
    dtv->counter_defrag_ipv6_timeouts =
        StatsRegisterCounter("defrag.ipv6.timeouts", tv);
    dtv->counter_defrag_max_hit =
        StatsRegisterCounter("defrag.max_frag_hits", tv);

    int i = 0;
    for (i = 0; i < DECODE_EVENT_PACKET_MAX; i++) {
        BUG_ON(i != (int)DEvents[i].code);
        dtv->counter_invalid_events[i] = StatsRegisterCounter(
                DEvents[i].event_name, tv);
    }

    return;
}

static __oryx_always_inline__ 
void perf_tmr_handler(struct oryx_timer_t *tmr, int __oryx_unused__ argc, 
                char **argv)
{
	int lcore = 0;
	ThreadVars *tv = &g_tv[lcore];
	DecodeThreadVars *dtv = &g_dtv[lcore];
	PacketQueue *pq = &g_pq[lcore];

#define format_buf_size 4096
	char buf[format_buf_size] = {0};
	size_t step = 0;
	
	step += snprintf (buf + step, format_buf_size - step, 
			"\"%15s\"		%lu\n", "pkts:", StatsGetLocalCounterValue(tv, dtv->counter_pkts));
	step += snprintf (buf + step, format_buf_size - step, 
			"\"%15s\"		%lu\n", "bytes:", StatsGetLocalCounterValue(tv, dtv->counter_bytes));
	step += snprintf (buf + step, format_buf_size - step, 
			"\"%15s\"		%lu\n", "eth:", StatsGetLocalCounterValue(tv, dtv->counter_eth));
	step += snprintf (buf + step, format_buf_size - step, 
			"\"%15s\"		%lu\n", "arp:", StatsGetLocalCounterValue(tv, dtv->counter_arp));
	step += snprintf (buf + step, format_buf_size - step, 
			"\"%15s\"		%lu\n", "ipv4:", StatsGetLocalCounterValue(tv, dtv->counter_ipv4));
	step += snprintf (buf + step, format_buf_size - step, 
			"\"%15s\"		%lu\n", "ipv6:", StatsGetLocalCounterValue(tv, dtv->counter_ipv6));
	step += snprintf (buf + step, format_buf_size - step, 
			"\"%15s\"		%lu\n", "udp:", StatsGetLocalCounterValue(tv, dtv->counter_udp));
	step += snprintf (buf + step, format_buf_size - step, 
			"\"%15s\"		%lu\n", "tcp:", StatsGetLocalCounterValue(tv, dtv->counter_tcp));
	step += snprintf (buf + step, format_buf_size - step, 
			"\"%15s\"		%lu\n", "sctp:", StatsGetLocalCounterValue(tv, dtv->counter_sctp));
	step += snprintf (buf + step, format_buf_size - step, 
			"\"%15s\"		%lu\n", "icmpv4:", StatsGetLocalCounterValue(tv, dtv->counter_icmpv4));
	step += snprintf (buf + step, format_buf_size - step, 
			"\"%15s\"		%lu\n", "icmpv6:", StatsGetLocalCounterValue(tv, dtv->counter_icmpv6));
	step += snprintf (buf + step, format_buf_size - step, 
			"\"%15s\"		%lu\n", "flows.memcap:", StatsGetLocalCounterValue(tv, dtv->counter_flow_memcap));
	step += snprintf (buf + step, format_buf_size - step, 
			"\"%15s\"		%lu\n", "flows.tcp:", StatsGetLocalCounterValue(tv, dtv->counter_flow_tcp));
	step += snprintf (buf + step, format_buf_size - step, 
			"\"%15s\"		%lu\n", "flows.udp:", StatsGetLocalCounterValue(tv, dtv->counter_flow_udp));
	step += snprintf (buf + step, format_buf_size - step, 
			"\"%15s\"		%lu\n", "flows.icmpv4:", StatsGetLocalCounterValue(tv, dtv->counter_flow_icmp4));
	step += snprintf (buf + step, format_buf_size - step, 
			"\"%15s\"		%lu\n", "flows.icmpv6:", StatsGetLocalCounterValue(tv, dtv->counter_flow_icmp6));

	oryx_logn("\n%s", buf);
	
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
	pthread_mutex_init(&tv->perf_public_ctx.m, NULL);

	DecodeRegisterPerfCounters(dtv, tv);
	/** setup private. */
	StatsSetupPrivate(tv);

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




