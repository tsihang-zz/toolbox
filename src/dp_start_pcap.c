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

extern ThreadVars g_tv[];
extern DecodeThreadVars g_dtv[];
extern PacketQueue g_pq[];
extern bool force_quit;

extern void
dp_register_perf_counters(DecodeThreadVars *dtv, ThreadVars *tv);

void
dp_pkt_handler(u_char *argv,
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

	/** recycle packet. */
	if(likely(p)){
        PACKET_RECYCLE(p);
        free(p);
	}
}

static struct netdev_t netdev = {
	.handler = NULL,
	.devname = "enp5s0f4",
	.dispatch = dp_pkt_handler,
	.private = NULL,
};

void *dp_libpcap_running_fn(void *argv)
{
	int64_t rank_acc;
	struct netdev_t *netdev = (struct netdev_t *)argv;
	
	atomic64_set(&netdev->rank, 0);

	netdev_open(netdev);
	
	FOREVER {
		if (force_quit)
			break;
		
		if (!netdev->handler)
			continue;
		
		rank_acc = pcap_dispatch(netdev->handler,
			1024, 
			netdev->dispatch, 
			(u_char *)netdev);

		if (rank_acc >= 0) {
			atomic64_add (&netdev->rank, rank_acc);
		} else {
			pcap_close (netdev->handler);
			netdev->handler = NULL;
			switch (rank_acc) {
				case -1:
				case -2:
				case -3:
				default:
					printf("pcap_dispatch=%ld\n", rank_acc);
					break;
			}
		}
	}

	oryx_task_deregistry_id(pthread_self());
	return NULL;
}

static struct oryx_task_t netdev_task =
{
	.module = THIS,
	.sc_alias = "Netdev Task",
	.fn_handler = dp_libpcap_running_fn,
	.ul_lcore = INVALID_CORE,
	.ul_prio = KERNEL_SCHED,
	.argc = 0,
	.argv = &netdev,
	.ul_flags = 0,	/** Can not be recyclable. */
};

static __oryx_always_inline__ 
void dp_pcap_perf_tmr_handler(struct oryx_timer_t *tmr, int __oryx_unused__ argc, 
                char **argv)
{
	tmr = tmr;
	argc = argc;
	argv = argv;
}

void dp_start_pcap(struct vlib_main_t *vm) {

	char thrgp_name[128] = {0}; 
	u32 nb_lcores = MAX_LCORES;
	int i;
	
	ThreadVars *tv;
	DecodeThreadVars *dtv;
	PacketQueue *pq;
	
	printf ("Master Lcore @ %d/%d\n", 0,
		nb_lcores);
		
	for (i = 0; i < (int)nb_lcores; i ++) {
		tv = &g_tv[i];
		dtv = &g_dtv[i];
		pq = &g_pq[i];
		sprintf (thrgp_name, "dp[%u] hd-thread", i);
		tv->thread_group_name = strdup(thrgp_name);
		SC_ATOMIC_INIT(tv->flags);
		pthread_mutex_init(&tv->perf_private_ctx0.m, NULL);
		dp_register_perf_counters(dtv, tv);
	}
		
	uint32_t ul_perf_tmr_setting_flags = TMR_OPTIONS_PERIODIC | TMR_OPTIONS_ADVANCED;
	vm->perf_tmr = oryx_tmr_create (1, "dp_perf_tmr", ul_perf_tmr_setting_flags,
											  dp_pcap_perf_tmr_handler, 0, NULL, 3000);

	oryx_task_registry(&netdev_task);
}

void dp_end_pcap(struct vlib_main_t *vm) {
	printf("Closing netdev %s...", netdev.devname);
	printf(" Done\n");
	printf("Bye...\n");
}

