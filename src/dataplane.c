#include "oryx.h"
#include "et1500.h"
#include "dataplane.h"

#define DATAPLANE_DEBUG	0
#define INIT_PQ(pq) (pq = pq)

extern vlib_main_t vlib_main;

struct suricata_private_t {
	ThreadVars tv;
	DecodeThreadVars dtv;
	PacketQueue pq;
};

struct _3rd_pktvar_t {
	struct port_t from;
	struct list_head node;
	void *packet;	/** packet this pktvar belongs to. */
};

static LIST_HEAD_DECLARE(packet_list);
static INIT_MUTEX(packet_list_lock);

/** in us. */
u64 longest_cost = 0;
u64 shortest_cost = 10000000;

static dpdk_config_main_t dpdk_config_main = {
	.nchannels = 2,		/** */		
	.coremask = 0x0f,	/** 4 lcores. */
	.portmask = 0x03,	/** 2 xe ports */
	.n_rx_q_per_lcore = 1,
	.uio_driver_name = (char *)"uio_pci_generic",
	.num_mbufs = ET1500_DPDK_DEFAULT_NB_MBUF,
	.cache_size = ET1500_DPDK_DEFAULT_CACHE_SIZE,
	.data_room_size = ET1500_DPDK_DEFAULT_BUFFER_SIZE
};

static int
lookup_mapped_port (struct map_t *map, struct port_t *port, u8 from_to)
{

	vector vector_;
	struct port_t *tp;
	
	vector_ = map->port_list[from_to % MAP_N_PORTS];

	tp = vector_lookup (vector_, port->ul_id);
	if (unlikely (!tp)) {
		return 0;
	} else {
		/** THIS port is in this map's from vector. */
		if (tp->ul_id == port->ul_id) {
			return 1;
		}
	}

	/** port specified by src is not in $map. */
	return 0;
	
}

static void 
dump_mpm_result (struct map_t *map)
{

	int j;

	struct pattern_t *pt;
	struct udp_t *u;
	
	printf ("MPM total hits %u times for %s\n", map->pmq.rule_id_array_cnt, map_alias(map));

	for (j = 0; j < (int)map->pmq.rule_id_array_cnt; j ++) {
		u32 ul_sid, ul_udp_id, ul_pattern_id;
		ul_sid = map->pmq.rule_id_array[j];
		ul_udp_id = split_udp_id(ul_sid);
		ul_pattern_id = split_pattern_id(ul_sid);
		
		u = udp_entry_lookup_id(ul_udp_id);
		if (u) {
			pt = udp_entry_pattern_lookup_id(u, ul_pattern_id);
			if (pt) {
				printf ("	\"%s\"----> [%u] %s\n", udp_alias(u), pt->ul_id, pt->sc_val);
				/** if udp in map's pass set or drop set. */
			}
		}

	}
}

static int
do_find_nearest_port (u32 h, vector v, struct port_t **dpo)
{
	u32 c0 = h % vector_active(v);

	(*dpo) = vector_lookup (v, h);
	while (!(*dpo)) {
		(*dpo) = vector_lookup (v, (c0++) % vector_active(v));
	}

	return 0;
}

static void 
do_hit_proc (struct map_t *map, 
	struct port_t __oryx_unused__ *port, Packet *packet)
{

	int j;
	struct udp_t *u;

	packet = packet;
	
	for (j = 0; j < (int)map->pmq.rule_id_array_cnt; j ++) {
		u32 ul_sid, ul_udp_id;
		vector v0, v1;
		u32 ul_pattern_id;
		struct pattern_t *pt;

		ul_sid = map->pmq.rule_id_array[j];

		ul_udp_id = split_udp_id(ul_sid);
		u = udp_entry_lookup_id(ul_udp_id);
		if (!u) continue;
		
		ul_pattern_id = split_pattern_id(ul_sid);
		pt = udp_entry_pattern_lookup_id(u, ul_pattern_id);
		if (pt) {
			PATTERN_STATS_INC_BYTE(pt, packet->pktlen);
			PATTERN_STATS_INC_PKT(pt, 1);
		}

		/******************** ACTION for THIS HIT ***************************/

		v0 = u->qua[QUA_DROP];
		v1 = u->qua[QUA_PASS];

		/** check if this udp is in drop quadrant. drop has higher priority than pass one.  */
		if (vector_lookup (v0, map_slot(map))) {
			/** DROP THIS PACKET */
			PORT_DROPPKT_INC (port, 1);
			PORT_DROPBYTES_INC(port, packet->payload_len);
		} else {
			v1 = v1;
			/** caculate output port here. */
			u32  h = hash_data (packet->payload, packet->payload_len);
			/** get this map's output port */
			struct port_t *dpo;
			do_find_nearest_port (h, map->port_list[MAP_N_PORTS_TO], &dpo);
			if(dpo) {
				printf ("-------> send to port %s\n", dpo->sc_alias);
				PORT_OUTPKT_INC (dpo, 1);
				PORT_OUTBYTES_INC(dpo, packet->payload_len);
			}
		}
	}
}

static int 
do_pattern_search (struct map_t *map, struct pattern_t *pt, Packet *packet)
{

	u32 depth = 0;

	if (pt->us_offset > packet->payload_len) return 0;
	else {
		if ((pt->us_offset + pt->us_depth) < packet->payload_len) {
			depth = pt->us_depth;
		} else {
			depth = packet->payload_len - pt->us_offset;
		}
	}
	
	return MpmSearch (map->mpm_runtime_ctx, &map->mpm_thread_ctx, &map->pmq,
	                           packet->payload + pt->us_offset, depth);

}

static __oryx_always_inline__
void ingress_frame_proc_udp (Packet *packet, struct map_t *map, struct port_t *port)
{

	int i;
	vector pattern_vec;
	struct udp_t *u;
	vector udp_vec;
	u32 udp_has_patterns = 0;
	
	udp_vec = map->udp_set;

	vector_foreach_element (udp_vec, i, u) {
		if (u && (vector_active (u->patterns))) 
			udp_has_patterns = 1;

		if (!udp_has_patterns) continue;
		
		int j;
		struct pattern_t *pt;
		
		pattern_vec = u->patterns;

		vector_foreach_element (pattern_vec, j, pt) 
		{
			if (!pt) continue;

			struct  timeval  start;
			struct  timeval  end;
			u32 hits;
			u64 t;

			gettimeofday(&start,NULL);
			hits = do_pattern_search (map, pt, packet);
			gettimeofday(&end,NULL);
			
			t = tm_elapsed_us (&start, &end);
			longest_cost = MAX(longest_cost, t);
			shortest_cost = MIN(shortest_cost, t);

			printf("hits %u, cost %llu us (shortest=%llu, longest=%llu)\n", 
				hits,  t, shortest_cost, longest_cost);

			if (hits) {
				dump_mpm_result (map);
				do_hit_proc (map, port, packet);
				/** Reset. */
				PmqReset(&map->pmq);
			}
		}
	}
}

static void 
ingress_frame_proc (Packet *packet, struct map_t *map, struct port_t *port)
{
	
	u32 enable_pattern_lookup = 0;
	
	/** UDP lookup?? */
	/** pattern lookup will be enabled while udp has been set for this map. */
	if (likely(map->ul_mpm_has_been_setup) && 
		map->udp_set && vector_active (map->udp_set))
		enable_pattern_lookup = 1;

	if (enable_pattern_lookup)
		ingress_frame_proc_udp (packet, map, port);
}

static void *
PacketProducer (void __oryx_unused__ *args)
{

#define payload_	"1234567890abcdefghjiklmnopqrstuvwxyz1234567890\n"
	Packet *p = NULL;
	u8 *payload;
	u32 port = 0;
	uint16_t payload_size;
	struct _3rd_pktvar_t *pktvar;
	static u64 pktvar_id = 0;
	
	FOREVER {
		p = NULL;
		payload   = kmalloc(128, MPF_CLR, __oryx_unused_val__);
		ASSERT((payload));
		
		payload_size = strlen (payload_);
		memcpy (payload, payload_, payload_size);

		//p = UTHBuildPacket(payload, payload_size, IPPROTO_TCP);
		if (!p) {
			kfree (payload);
		} else {

			pktvar   = kmalloc(sizeof(*pktvar), MPF_CLR, __oryx_unused_val__);
			ASSERT((pktvar));
			next_rand_ (&port);
			
			/** INIT list Node. */
			INIT_LIST_HEAD (&pktvar->node);
			/** Arrived at which port */
			pktvar->from.ul_id = (port % PANEL_N_PORTS);
			pktvar->packet = p;
			PktVarAdd(p, pktvar_id ++, (uint8_t *)pktvar, sizeof(*pktvar));
			
			/** Add to Packet queue. */
			oryx_thread_mutex_lock(&packet_list_lock);
			list_add (&pktvar->node, &packet_list);
			oryx_thread_mutex_unlock(&packet_list_lock);
			if (DATAPLANE_DEBUG)
				printf (".\n");
			
		} 
		usleep (500000);
	}
	
	oryx_task_deregistry_id (pthread_self());
	return NULL;
	
}

static void traffic_dpo (Packet *packet, struct map_t *map, struct port_t *port)
{
	struct traffic_dpo_t *dpo = &map->dpo;
	
	if (dpo->dpo_fn) {
		dpo->dpo_fn (packet, map);
	}

	port = port;
}

static void packet_proc (struct port_t *port, Packet *packet)
{
	int foreach_element;
	struct map_t *map = NULL;

	PORT_INPKT_INC(port, 1);
	PORT_INBYTES_INC(port, packet->pktlen);
	
	/** Lookup for valid maps. Foreach maps upon this port. 
	 *  Lookup maps from highest priority map to lowest priority map.*/
	vector_foreach_element (port->belong_maps, foreach_element, map) {

		if (unlikely(!map)) {
			if (DATAPLANE_DEBUG)
				printf ("unmapped port %s\n", port->sc_alias);
			continue;
		}
		
		/** map exsit ?? */
		if (!map_table_entry_lookup_i(map->ul_id))
			continue;
		
		if (map->ul_flags & MAP_TRAFFIC_TRANSPARENT) {
			/** skip all criterias, and forward traffic right now. */
			traffic_dpo (packet, map, port);
			continue;
		}else {
			/** not an ingress port or an egress port. */
			u8 is_an_ingress_port = -1;

			if (lookup_mapped_port (map, port, MAP_N_PORTS_FROM))
				is_an_ingress_port = 1;
			if (lookup_mapped_port (map, port, MAP_N_PORTS_TO))
				is_an_ingress_port = 0;

			/** if the port is a "from" port ??? */
			if (is_an_ingress_port == 1) {
				ingress_frame_proc (packet, map, port);
			}else {
				/** if the port is a "to" port ??? */
				if (is_an_ingress_port == 0) {
					/** Forward by matched applications. */
				}
			}
		}
	}
}

/** \brief handle flow for packet
 *
 *  Handle flow creation/lookup
 */
static inline TmEcode FlowUpdate0(Packet *p)
{
    /** FlowHandlePacketUpdate(p->flow, p); */

    int state = SC_ATOMIC_GET(p->flow->flow_state);
    switch (state) {
        case FLOW_STATE_CAPTURE_BYPASSED:
        case FLOW_STATE_LOCAL_BYPASSED:
            return TM_ECODE_DONE;
        default:
            return TM_ECODE_OK;
    }
}

static void
netdev_pkt_handler(u_char *argv,
		const struct pcap_pkthdr *pcaphdr,
		const u_char *packet)
{

	struct netdev_t *netdev = (struct netdev_t *)argv;
	struct suricata_private_t *priv;

	priv = (struct suricata_private_t *)netdev->private;
	ThreadVars *tv = &priv->tv;
	DecodeThreadVars *dtv = &priv->dtv;
	PacketQueue *pq = &priv->pq;
	
	SCLogDebug("(2) netdev= %p, priv=%p, tv=%p, dtv=%p", netdev, priv, tv, dtv);
	
	Packet *p = PacketGetFromAlloc();
	if(likely(p)) {
		p->datalink = LINKTYPE_ETHERNET;
	}

#if defined(HAVE_STATS_COUNTERS)
	DecodeUpdatePacketCounters(tv, dtv, p);
#endif

/** physical port. */
	int port_id = random() % PANEL_N_PORTS;
	struct port_t *port;
	struct prefix_t lp = {
		.cmd = LOOKUP_ID,
		.v = (void *)&port_id,
		.s = __oryx_unused_val__,
	};
	
	port_table_entry_lookup (&lp, &port);
	if (unlikely(!port)) {
		SCLogNotice("Unknown port %d", port_id);
		goto finish;
	}

	PORT_INPKT_INC(port, 1);
	PORT_INBYTES_INC(port, pcaphdr->len);
	
	//PktVarAdd (p, 0, ..., ...);

	if(likely(p)) {
		switch(p->datalink) {
			case LINKTYPE_ETHERNET:
				DecodeEthernet0(tv, dtv, p, 
					packet, pcaphdr->len, pq);
				break;
			default:
				break;
		}
	}

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

	SCLogDebug("packet %"PRIu64" has flow? %s", p->pcap_cnt, p->flow ? "yes" : "no");

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
        SCFree(p);
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

#if 0
static __oryx_always_inline__ 
void perf_tmr_handler(struct oryx_timer_t *tmr, int __oryx_unused__ argc, 
                char **argv)
{
	struct netdev_t *nd = (struct netdev_t *)argv;
	struct suricata_private_t *priv = nd->private;
	ThreadVars *tv;
	DecodeThreadVars *dtv;
	PacketQueue *pq;
#define format_buf_size 4096
	char buf[format_buf_size] = {0};
	size_t step = 0;
	
	tv = &priv->tv;
	dtv = &priv->dtv;
	pq = &priv->pq;

	step += snprintf (buf + step, format_buf_size - step, 
			"\"%15s\"		%lu\n", "pkts:", StatsGetLocalCounterValue(tv, dtv->counter_pkts));
	step += snprintf (buf + step, format_buf_size - step, 
			"\"%15s\"		%lu\n", "bytes:", StatsGetLocalCounterValue(tv, dtv->counter_bytes));
	step += snprintf (buf + step, format_buf_size - step, 
			"\"%15s\"		%lu\n", "eth:", StatsGetLocalCounterValue(tv, dtv->counter_eth));
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

	SCLogNotice("\n%s", buf);
	
}

void perf_tmr_init()
{
	struct oryx_timer_t *perf_tmr;
	uint32_t ul_perf_tmr_setting_flags = TMR_OPTIONS_PERIODIC | TMR_OPTIONS_ADVANCED;
	
	perf_tmr = oryx_tmr_create (1, "perf_tmr", ul_perf_tmr_setting_flags,
											  perf_tmr_handler, 0, (char **)&netdev, 3000);
	oryx_tmr_start(perf_tmr);
}
#endif

void
notify_dp(int signum)
{
	dpdk_main_t *dm = &dpdk_main;
	if (signum == SIGINT || signum == SIGTERM) {
		printf("\n\nSignal %d received, preparing to exit...\n",
				signum);
		dm->force_quit = true;
	}
}

typedef struct _dp_main_t {
	ThreadVars tv;
	DecodeThreadVars dtv;
	PacketQueue pq;
}dp_main_t;

dp_main_t dp_main[RTE_MAX_LCORE];

static int
dp_start(__attribute__((unused)) void *ptr_data)
{
	dpdk_main_t *dm = &dpdk_main;
	dp_main_t *dp;
	ThreadVars *tv;
	DecodeThreadVars *dtv;
	PacketQueue *pq;
	char thrgp_name[128] = {0};

	dp = &dp_main[rte_lcore_id() % RTE_MAX_LCORE];
	tv = &dp->tv;
	dtv = &dp->dtv;
	pq = &dp->pq;

	sprintf (thrgp_name, "dp[%u] hd-thread", rte_lcore_id());
	tv->thread_group_name = strdup(thrgp_name);
	SC_ATOMIC_INIT(tv->flags);
	SCMutexInit(&tv->perf_public_ctx.m, NULL);

	INIT_PQ(pq);
	
#if defined(HAVE_STATS_COUNTERS)
	SCLogDebug("(1) tv=%p, dtv=%p", tv, dtv);
	DecodeRegisterPerfCounters(dtv, tv);
	/** setup private. */
	StatsSetupPrivate(tv);
#endif

	while (!dm->force_quit) {
		/** rx */
	}

	SCLogNotice ("dp_main[%d] exiting ...", rte_lcore_id());
}

dpdk_main_t dpdk_main = {
	.vlib_main = &vlib_main,
	.conf = &dpdk_config_main,
	.stat_poll_interval = ET1500_DPDK_STATS_POLL_INTERVAL,
	.link_state_poll_interval = ET1500_DPDK_LINK_POLL_INTERVAL,
	.dp_fn = dp_start,
	.force_quit = false,
};

void dataplane_start(void) {
	dpdk_main_t *dm = &dpdk_main;
	printf ("Master Lcore %d\n", rte_get_master_lcore());

	/* launch per-lcore init on every lcore */
	rte_eal_mp_remote_launch(dm->dp_fn, NULL, SKIP_MASTER);
}

void dataplane_terminal(void)
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
