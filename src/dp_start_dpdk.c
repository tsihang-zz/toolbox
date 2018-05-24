#include "oryx.h"
#include "dp_decode.h"
#include "iface_private.h"
#include "dpdk.h"

extern dp_private_t dp_private_main;
extern ThreadVars g_tv[];
extern DecodeThreadVars g_dtv[];
extern PacketQueue g_pq[];
extern bool force_quit;

extern void
DecodeUpdateCounters(ThreadVars *tv,
                                const DecodeThreadVars *dtv, const Packet *p);

extern void
dp_register_perf_counters(DecodeThreadVars *dtv, ThreadVars *tv);

static char *eal_init_argv[1024] = {0};
static int eal_init_argc = 0;
static int eal_args_offset = 0;

static dpdk_config_main_t dpdk_config_main = {
	.nchannels = 2,		/** */		
	.coremask = 0x0f,	/** 4 lcores. */
	.portmask = 0x07,	/** 3 xe ports */
	.n_rx_q_per_lcore = 1,
	.uio_driver_name = (char *)"vfio-pci",
	.num_mbufs =		DPDK_DEFAULT_NB_MBUF,
	.mempool_cache_size =	DPDK_DEFAULT_MEMPOOL_CACHE_SIZE,
	.mempool_data_room_size = DPDK_DEFAULT_BUFFER_SIZE,	
	.mempool_priv_size = 0,
	.num_rx_desc = RTE_RX_DESC_DEFAULT,
	.num_tx_desc = RTE_TX_DESC_DEFAULT,
	.num_rx_queues = 1,
	.num_tx_queues = 1,
	.ethdev_default_conf = &dpdk_eth_default_conf,
};

dpdk_main_t dpdk_main = {
	.conf = &dpdk_config_main,
	.stat_poll_interval = DPDK_STATS_POLL_INTERVAL,
	.link_state_poll_interval = DPDK_LINK_POLL_INTERVAL,
	.ext_private = &dp_private_main
};

#if 1
/* Send burst of packets on an output interface */
static inline int
__forward_burst(ThreadVars *tv, DecodeThreadVars *dtv, 
				struct lcore_conf *lconf, uint16_t n, uint8_t port)
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
#endif

static inline int forward_out(ThreadVars *tv, DecodeThreadVars *dtv, 
				Packet *p, struct rte_mbuf *mbuf, u8 rx_port, u8 tx_port, PacketQueue *pq)
{	
	uint16_t n_tx_try_burst;
	int n_tx_burst = 0;	
	struct rte_mbuf **m;
	struct lcore_conf *lconf = &lcore_conf_ctx[tv->lcore];
	int qua = QUA_COUNTER_TX;
	struct iface_t *iface;
	vlib_port_main_t *vp = &vlib_port_main;
#if 0
	iface_lookup_id(vp, tx_port, &iface);
	if(!iface) {
		oryx_panic(-1, "Cannot locate a tx_port[%d].", tx_port);
	}
#endif

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

#define NS_PER_US 1000
#define US_PER_MS 1000
#define MS_PER_S 1000
#define US_PER_S (US_PER_MS * MS_PER_S)

static int
dp_dpdk_running_fn(void *ptr_data)
{
	vlib_main_t *vm = (vlib_main_t *)ptr_data;
	dpdk_main_t *dm = &dpdk_main;
	struct lcore_conf *lconf;
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
	const uint64_t drain_tsc = (rte_get_tsc_hz() + US_PER_S - 1) /
		US_PER_S * BURST_TX_DRAIN_US;

	tv->lcore = rte_lcore_id();
	lconf = &lcore_conf_ctx[tv->lcore];
	
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
#if 0
		cur_tsc = rte_rdtsc();
		/*
		 * TX burst queue drain
		 */
		diff_tsc = cur_tsc - prev_tsc;
		if (unlikely(diff_tsc > drain_tsc)) {

			for (i = 0; i < lconf->n_tx_port; ++i) {
				portid = lconf->tx_port_id[i];
				if (lconf->tx_mbufs[portid].len == 0)
					continue;
				__forward_burst(lconf,
					lconf->tx_mbufs[portid].len,
					portid);
				lconf->tx_mbufs[portid].len = 0;
			}

			prev_tsc = cur_tsc;
		}
#endif
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

static void
dpdk_eal_args_format(const char *argv)
{
	char *t = kmalloc(strlen(argv) + 1, MPF_CLR, __oryx_unused_val__);
	sprintf (t, "%s%c", argv, 0);
	eal_init_argv[eal_init_argc ++] = t;
}

static void
dpdk_eal_args_2string(char *format_buffer) {
	int i;
	int l = 0;
	
	for (i = 0; i < eal_init_argc; i ++) {
		l += sprintf (format_buffer + l, "%s ", eal_init_argv[i]);
	}
}

/**
 * DPDK simple EAL init args : $prgname -c 0x0c -n 2 -- -p 3 -q 1 CT0
 */
static void
dpdk_format_eal_args (vlib_main_t *vm)
{
	int i;
	dpdk_config_main_t *conf = dpdk_main.conf;
	char argv_buf[128] = {0};
	const char *socket_mem = "256";
	const char *file_prefix = "et1500";
	const char *prgname = "et1500";
	
	/** ARGS: < APPNAME >  */
	memset(argv_buf, 0, 128);
	sprintf (argv_buf, "%s", prgname);
	dpdk_eal_args_format(argv_buf);

	/** ARGS: < -c $COREMASK > */
	memset(argv_buf, 0, 128);
	sprintf (argv_buf, "-c");
	dpdk_eal_args_format(argv_buf);
	memset(argv_buf, 0, 128);
	sprintf (argv_buf, "0x%x", conf->coremask);	/** [core2:p0] [core3:p1] */
	dpdk_eal_args_format(argv_buf);

	/** ARGS: < -n $NCHANNELS >  */
	memset(argv_buf, 0, 128);
	sprintf (argv_buf, "-n");
	dpdk_eal_args_format(argv_buf);
	memset(argv_buf, 0, 128);
	sprintf (argv_buf, "%d", conf->nchannels);
	dpdk_eal_args_format(argv_buf);

	/** ARGS: < -- > */
	memset(argv_buf, 0, 128);
	sprintf (argv_buf, "--");
	dpdk_eal_args_format(argv_buf);

	/** ARGS: < -p $PORTMASK > */
	memset(argv_buf, 0, 128);
	sprintf (argv_buf, "-p");
	dpdk_eal_args_format(argv_buf);
	memset(argv_buf, 0, 128);
	sprintf (argv_buf, "0x%x", conf->portmask);	/** Port0 Port1 */
	dpdk_eal_args_format(argv_buf);

	/** ARGS: < -q $QUEUE_PER_LOCRE > */
	memset(argv_buf, 0, 128);
	sprintf (argv_buf, "-q");
	dpdk_eal_args_format(argv_buf);
	memset(argv_buf, 0, 128);
	sprintf (argv_buf, "%d", conf->n_rx_q_per_lcore);	/** nx_queues_per_lcore */
	dpdk_eal_args_format(argv_buf);

	char eal_args_format_buffer[1024] = {0};
	dpdk_eal_args_2string(eal_args_format_buffer);

	vm->argc = eal_init_argc;	
	vm->argv = eal_init_argv;

	oryx_logn("eal args[%d]= %s", eal_init_argc, eal_args_format_buffer);
}

/* Check the link status of all ports in up to 9s, and print them finally */
static void
check_linkstatus(uint8_t port_num, uint32_t port_mask)
{
#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 90 /* 9s (90 * 100ms) in total */
	uint8_t portid, count, all_ports_up, print_flag = 0;
	struct rte_eth_link link;

	printf("\nChecking link status");
	fflush(stdout);
	for (count = 0; count <= MAX_CHECK_TIME; count++) {

		all_ports_up = 1;
		for (portid = 0; portid < port_num; portid++) {

			if ((port_mask & (1 << portid)) == 0)
				continue;
			
			memset(&link, 0, sizeof(link));
			
			rte_eth_link_get_nowait(portid, &link);
			/* print link status if flag set */
			if (print_flag == 1) {
				if (link.link_status)
					printf("Port %d Link Up - speed %u "
						"Mbps - %s\n", (uint8_t)portid,
						(unsigned)link.link_speed,
				(link.link_duplex == ETH_LINK_FULL_DUPLEX) ?
					("full-duplex") : ("half-duplex\n"));
				else
					printf("Port %d Link Down\n",
						(uint8_t)portid);
				continue;
			}
			/* clear all_ports_up flag if any link down */
			if (link.link_status == ETH_LINK_DOWN) {
				all_ports_up = 0;
				break;
			}
		}
		/* after finally printing all link status, get out */
		if (print_flag == 1)
			break;

		if (all_ports_up == 0) {
			printf(".");
			fflush(stdout);
			rte_delay_ms(CHECK_INTERVAL);
		}

		/* set the print_flag if all ports up or timeout */
		if (all_ports_up == 1 || count == (MAX_CHECK_TIME - 1)) {
			print_flag = 1;
			printf("done\n");
		}
	}
}

static inline void log_usage(void)
{
	/** run env-setup script. bind vfio-pci and mount hugedir.*/
	oryx_loge(0,
		"Make sure that you have run %s to setup dpdk enviroment before startup this application.", DPDK_SETUP_ENV_SH);
	sleep(1);
}

const char *enp5s0fx[] = {
	"enp5s0f1",
	"enp5s0f2",
	"enp5s0f3"
};

void dpdk_env_setup(vlib_main_t *vm)
{
	struct oryx_fmt_buff_t setup_env = FMT_BUFF_INITIALIZATION;
	dpdk_main_t *dm = &dpdk_main;
	dpdk_config_main_t *conf = dm->conf;
	struct iface_t *this;
	vlib_port_main_t *vp = &vlib_port_main;
	int n_ports_now = vec_count(vp->entry_vec);
	
	dpdk_format_eal_args(vm);

	oryx_logn("================ DPDK INIT ARGS =================");
	oryx_logn("%20s%15x", "ul_core_mask", conf->coremask);
	oryx_logn("%20s%15x", "ul_port_mask", conf->portmask);
	oryx_logn("%20s%15d", "ul_priv_size", vm->extra_priv_size);
	oryx_logn("%20s%15d", "n_pool_mbufs", conf->num_mbufs);
	oryx_logn("%20s%15d", "n_rx_q_lcore", conf->n_rx_q_per_lcore);
	oryx_logn("%20s%15d", "rte_cachelin", RTE_CACHE_LINE_SIZE);
	oryx_logn("%20s%15d", "mp_data_room", conf->mempool_data_room_size);
	oryx_logn("%20s%15d", "mp_cache_siz", conf->mempool_cache_size);
	oryx_logn("%20s%15d", "mp_priv_size", conf->mempool_priv_size);
	oryx_logn("%20s%15d", "n_rx_desc", (int)conf->num_rx_desc);
	oryx_logn("%20s%15d", "n_tx_desc", (int)conf->num_tx_desc);
	oryx_logn("%20s%15d", "n_rx_queu", (int)conf->num_rx_queues);
	oryx_logn("%20s%15d", "n_tx_queu", (int)conf->num_tx_queues);
	oryx_logn("%20s%15d", "socket_id", rte_socket_id());

	struct lcore_conf *lconf;
	struct rte_eth_dev_info dev_info;
	struct rte_eth_txconf *txconf;
	int ret;
	uint8_t nb_ports;
	uint8_t nb_ports_available;
	uint8_t portid, last_port;
	unsigned lcore_id, rx_lcore_id;

	/* init EAL */
	ret = rte_eal_init(vm->argc, vm->argv);
	if (ret < 0) {
		log_usage();
		rte_exit(EXIT_FAILURE, "Invalid EAL arguments\n");
	}
	
	vm->argc -= ret;
	vm->argv += ret;

	/* create the mbuf pool */
	dm->pktmbuf_pools = rte_pktmbuf_pool_create("mbuf_pool", conf->num_mbufs, 
							conf->mempool_cache_size, conf->mempool_priv_size, conf->mempool_data_room_size, rte_socket_id());
	if(likely(dm->pktmbuf_pools == NULL))
		rte_exit(EXIT_FAILURE, "Cannot init mbuf pool\n");
	
	nb_ports = rte_eth_dev_count();
	if (nb_ports == 0) {
		log_usage();
		rte_exit(EXIT_FAILURE, "No Ethernet ports - bye\n");
	}

	/* register to vlib_port_main. */
	for (portid = 0; portid < nb_ports; portid ++) {
		iface_lookup_id(vp, portid, &this);
		if(!this) {
			oryx_panic(-1, "no such ethdev.");
		}
		if(strcmp(this->sc_alias_fixed, enp5s0fx[portid])) {
			oryx_panic(-1, "no such ethdev named %s.", this->sc_alias_fixed);
		}
	}
	
	/** skip master lcore. */
	rx_lcore_id = 1;
	lconf = NULL;

	/* Initialize the port/queue configuration of each logical core */
	for (portid = 0; portid < nb_ports; portid ++) {
		/* skip ports that are not enabled */
		if ((conf->portmask & (1 << portid)) == 0)
			continue;

		/* get the lcore_id for this port */
		while (rte_lcore_is_enabled(rx_lcore_id) == 0 ||
		       lcore_conf_ctx[rx_lcore_id].n_rx_port == conf->n_rx_q_per_lcore) {
			rx_lcore_id ++;
			if (rx_lcore_id >= RTE_MAX_LCORE)
				rte_exit(EXIT_FAILURE, "Not enough cores\n");
			lconf = &lcore_conf_ctx[rx_lcore_id];
		}
			   
	   if (lconf != &lcore_conf_ctx[rx_lcore_id])
		   /* Assigned a new logical core in the loop above. */
		   lconf = &lcore_conf_ctx[rx_lcore_id];
	   
	   lconf->rx_port_list[lconf->n_rx_port] = portid;
	   lconf->n_rx_port++;
	   printf("Lcore %u: RX port %u\n", rx_lcore_id, (unsigned) portid);

	}

	/* Initialise each port */
	for (portid = 0; portid < nb_ports; portid++) {
		/* skip ports that are not enabled */
		if ((conf->portmask & (1 << portid)) == 0)
			continue;
		
		/* init port */
		printf("Initializing port %u... ", (unsigned) portid);
		fflush(stdout);
		
		ret = rte_eth_dev_configure(portid, 1, 1, conf->ethdev_default_conf);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "Cannot configure device: err=%d, port=%u\n",
				  ret, (unsigned) portid);

		struct ether_addr eth_addr;
		rte_eth_macaddr_get(portid,&eth_addr);

		/* init one RX queue */
		fflush(stdout);
		
		ret = rte_eth_rx_queue_setup(portid, 0, conf->num_rx_desc,
						 rte_eth_dev_socket_id(portid),
						 NULL,
						 dm->pktmbuf_pools);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "rte_eth_rx_queue_setup:err=%d, port=%u\n",
				  ret, (unsigned) portid);

		fflush(stdout);

		rte_eth_dev_info_get(portid, &dev_info);
		txconf = &dev_info.default_txconf;
		if (conf->ethdev_default_conf->rxmode.jumbo_frame)
			txconf->txq_flags = 0;
		
		/* init one TX queue on each port */
		u16 tx_queue = 0;
		ret = rte_eth_tx_queue_setup(portid, tx_queue, conf->num_tx_desc,
				  rte_eth_dev_socket_id(portid), txconf);
		if (ret < 0)
		  rte_exit(EXIT_FAILURE, "rte_eth_tx_queue_setup:err=%d, port=%u\n",
			  ret, (unsigned) portid);

		/* Start device */
		ret = rte_eth_dev_start(portid);
		if (ret < 0)
		  rte_exit(EXIT_FAILURE, "rte_eth_dev_start:err=%d, port=%u\n",
				ret, (unsigned) portid);

		printf("done: \n");

		rte_eth_promiscuous_enable(portid);
	}

	check_linkstatus(nb_ports, conf->portmask);
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

void dp_start_dpdk(struct vlib_main_t *vm) {
	vm->dp_running_fn = dp_dpdk_running_fn;
	dpdk_main_t *dm = &dpdk_main;
	char thrgp_name[128] = {0}; 
	int i;
	
	ThreadVars *tv;
	DecodeThreadVars *dtv;
	
	vm->max_lcores = rte_lcore_count();
	printf ("Master Lcore @ %d/%d\n", rte_get_master_lcore(),
		vm->max_lcores);
	
	for (i = 0; i < vm->max_lcores; i ++) {
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
	rte_eal_mp_remote_launch(vm->dp_running_fn, vm, SKIP_MASTER);
}




