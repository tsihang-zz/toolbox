#include "oryx.h"
#include "dp_decode.h"
//#include "dpdk.h"

extern ThreadVars g_tv[];
extern DecodeThreadVars g_dtv[];
extern PacketQueue g_pq[];
extern bool force_quit;

extern void DecodeUpdateCounters(ThreadVars *tv,
                                const DecodeThreadVars *dtv, const Packet *p);
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

void dp_start_pcap(struct vlib_main_t *vm) {
	oryx_task_registry(&netdev_task);
}

void dp_end_pcap(struct vlib_main_t *vm) {
	printf("Closing netdev %s...", netdev.devname);
	printf(" Done\n");
	printf("Bye...\n");
}

