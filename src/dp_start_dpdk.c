#include "oryx.h"
#include "dp_decode.h"
#include "dpdk.h"

extern dp_private_t dp_private_main;
extern ThreadVars g_tv[];
extern DecodeThreadVars g_dtv[];
extern PacketQueue g_pq[];
extern bool force_quit;

extern struct lcore_queue_conf lcore_queue_conf[];

extern void
DecodeUpdateCounters(ThreadVars *tv,
                                const DecodeThreadVars *dtv, const Packet *p);

extern void
dp_register_perf_counters(DecodeThreadVars *dtv, ThreadVars *tv);


static dpdk_config_main_t dpdk_config_main = {
	.nchannels = 2,		/** */		
	.coremask = 0x0f,	/** 4 lcores. */
	.portmask = 0x07,	/** 3 xe ports */
	.n_rx_q_per_lcore = 1,
	.uio_driver_name = (char *)"vfio-pci",
	.num_mbufs =		DPDK_DEFAULT_NB_MBUF,
	.cache_size =		DPDK_DEFAULT_CACHE_SIZE,
	.data_room_size =	DPDK_DEFAULT_BUFFER_SIZE
};

static inline void dump_pkt(uint8_t *pkt, int len)
{
	int i = 0;
	
	for (i = 0; i < len; i ++){
		if (!(i % 16))
			printf ("\n");
		printf("%02x ", pkt[i]);
	}
	printf ("\n");
}

static int
dp_dpdk_running_fn(__attribute__((unused)) void *ptr_data)
{
	dpdk_main_t *dm = &dpdk_main;
	u32 lcore = rte_lcore_id();
	struct lcore_queue_conf *qconf;
	unsigned i, j, portid, nb_rx;
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
	struct rte_mbuf *m;

	ThreadVars *tv = &g_tv[lcore % RTE_MAX_LCORE];
	DecodeThreadVars *dtv = &g_dtv[lcore % RTE_MAX_LCORE];
	PacketQueue *pq = &g_pq[lcore % RTE_MAX_LCORE];

	qconf = &lcore_queue_conf[lcore];
	oryx_logn(".... %d rx_port(s) @ lcore %d", 
		qconf->n_rx_port, lcore);

	while (!force_quit) {

		/*
		 * Read packet from RX queues
		 */
		for (i = 0; i < qconf->n_rx_port; i++) {
			
			portid = qconf->rx_port_list[i];
			nb_rx = rte_eth_rx_burst((uint8_t) portid, 0,
						 pkts_burst, MAX_PKT_BURST);

			for (j = 0; j < nb_rx; j++) {
				m = pkts_burst[j];
				rte_prefetch0(rte_pktmbuf_mtod(m, void *));
				oryx_logd(".... %d packets, pktlen = %d", nb_rx, m->pkt_len);
				Packet *p = PacketGetFromAlloc();
				uint8_t *packet = (uint8_t *)rte_pktmbuf_mtod(m, void *);
				SET_PKT_LEN(p, m->pkt_len);
				//dump_pkt(packet, m->pkt_len);
				DecodeEthernet0(tv, dtv, p, packet, m->pkt_len, pq);
				DecodeUpdateCounters(tv, dtv, p);
			}
		}
	}

	oryx_logn ("dp_main[%d] exiting ...", lcore);
}

static char *eal_init_argv[1024] = {0};
static int eal_init_args = 0;
static int eal_args_offset = 0;

static void
dpdk_eal_args_format(const char *argv)
{
	char *t = kmalloc(strlen(argv) + 1, MPF_CLR, __oryx_unused_val__);
	sprintf (t, "%s%c", argv, 0);
	eal_init_argv[eal_init_args ++] = t;
}

static void
dpdk_eal_args_2string(char *format_buffer) {
	int i;
	int l = 0;
	
	for (i = 0; i < eal_init_args; i ++) {
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

	vm->argc = eal_init_args;	
	vm->argv = eal_init_argv;

	oryx_logn("eal args[%d]= %s", eal_init_args, eal_args_format_buffer);
}

static const struct rte_eth_conf port_conf = {
	.rxmode = {
		.split_hdr_size = 0,
		.header_split   = 0, /**< Header Split disabled */
		.hw_ip_checksum = 0, /**< IP checksum offload disabled */
		.hw_vlan_filter = 0, /**< VLAN filtering disabled */
		.jumbo_frame    = 0, /**< Jumbo Frame Support disabled */
		.hw_strip_crc   = 1, /**< CRC stripped by hardware */
	},
	.txmode = {
		.mq_mode = ETH_MQ_TX_NONE,
	},
};

/*
 * Configurable number of RX/TX ring descriptors
 */
#define RTE_TEST_RX_DESC_DEFAULT 128
#define RTE_TEST_TX_DESC_DEFAULT 512
static uint16_t nb_rxd = RTE_TEST_RX_DESC_DEFAULT;
static uint16_t nb_txd = RTE_TEST_TX_DESC_DEFAULT;


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

void dpdk_env_setup(vlib_main_t *vm)
{
	struct oryx_fmt_buff_t setup_env = FMT_BUFF_INITIALIZATION;
	dpdk_main_t *dm = &dpdk_main;
	dpdk_config_main_t *conf = dm->conf;
	
	/** run env-setup script. */
	oryx_format(&setup_env, "/bin/sh %s/%s", CONFIG_PATH, DPDK_SETUP_ENV_SH);
	oryx_logn("%s", FMT_DATA(setup_env));
	do_system (FMT_DATA(setup_env));

	/** prepare eal args for dpdk. */
	oryx_logn("%s", FMT_DATA(setup_env));
	oryx_format_free(&setup_env);
	dpdk_format_eal_args(vm);

	oryx_logn("================ DPDK INIT ARGS =================");
	oryx_logn("%20s%15x", "core_mask", conf->coremask);
	oryx_logn("%20s%15x", "port_mask", conf->portmask);
	oryx_logn("%20s%15d", "num_mbufs", conf->num_mbufs);
	oryx_logn("%20s%15d", "nxqslcore", conf->n_rx_q_per_lcore);
	oryx_logn("%20s%15d", "cachesize", conf->cache_size);
	oryx_logn("%20s%15d", "priv_size", vm->extra_priv_size);
	oryx_logn("%20s%15d", "cacheline", RTE_CACHE_LINE_SIZE);
	oryx_logn("%20s%15d", "datarmsiz", conf->data_room_size);
	oryx_logn("%20s%15d", "socket_id", rte_socket_id());

#if 0
	/** real initialization. */
	init_dpdk_env(vm);
#else
	struct lcore_queue_conf *qconf;
	struct rte_eth_dev_info dev_info;
	int ret;
	uint8_t nb_ports;
	uint8_t nb_ports_available;
	uint8_t portid, last_port;
	unsigned lcore_id, rx_lcore_id;


	/* init EAL */
	ret = rte_eal_init(vm->argc, vm->argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid EAL arguments\n");
	vm->argc -= ret;
	vm->argv += ret;

	/* create the mbuf pool */
	dm->pktmbuf_pools = rte_pktmbuf_pool_create("mbuf_pool", NB_MBUF,
		MEMPOOL_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE,
		rte_socket_id());
	if(likely(dm->pktmbuf_pools == NULL))
		rte_exit(EXIT_FAILURE, "Cannot init mbuf pool\n");
	
	nb_ports = rte_eth_dev_count();
	if (nb_ports == 0)
		rte_exit(EXIT_FAILURE, "No Ethernet ports - bye\n");

	for (portid = 0; portid < nb_ports; portid++) {
		/* skip ports that are not enabled */
		if (conf->portmask & (1 << portid) == 0)
			continue;
		rte_eth_dev_info_get(portid, &dev_info);
	}

	/** skip master lcore. */
	rx_lcore_id = 1;
	qconf = NULL;

	/* Initialize the port/queue configuration of each logical core */
	for (portid = 0; portid < nb_ports; portid++) {
		/* skip ports that are not enabled */
		if (conf->portmask & (1 << portid) == 0)
			continue;

		/* get the lcore_id for this port */
		while (rte_lcore_is_enabled(rx_lcore_id) == 0 ||
		       lcore_queue_conf[rx_lcore_id].n_rx_port ==
		       conf->n_rx_q_per_lcore) {
			rx_lcore_id ++;
			if (rx_lcore_id >= RTE_MAX_LCORE)
				rte_exit(EXIT_FAILURE, "Not enough cores\n");
			qconf = &lcore_queue_conf[rx_lcore_id];
		}
			   
	   if (qconf != &lcore_queue_conf[rx_lcore_id])
		   /* Assigned a new logical core in the loop above. */
		   qconf = &lcore_queue_conf[rx_lcore_id];
	   
	   qconf->rx_port_list[qconf->n_rx_port] = portid;
	   qconf->n_rx_port++;
	   printf("Lcore %u: RX port %u\n", rx_lcore_id, (unsigned) portid);

	}

	/* Initialise each port */
	for (portid = 0; portid < nb_ports; portid++) {
		/* skip ports that are not enabled */
		if (conf->portmask & (1 << portid) == 0)
			continue;
		
		/* init port */
		printf("Initializing port %u... ", (unsigned) portid);
		fflush(stdout);
		
		ret = rte_eth_dev_configure(portid, 1, 1, &port_conf);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "Cannot configure device: err=%d, port=%u\n",
				  ret, (unsigned) portid);
		
		//rte_eth_macaddr_get(portid,&l2fwd_ports_eth_addr[portid]);

		/* init one RX queue */
		fflush(stdout);
		
		ret = rte_eth_rx_queue_setup(portid, 0, nb_rxd,
						 rte_eth_dev_socket_id(portid),
						 NULL,
						 dm->pktmbuf_pools);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "rte_eth_rx_queue_setup:err=%d, port=%u\n",
				  ret, (unsigned) portid);

		/* init one TX queue on each port */
		fflush(stdout);
		ret = rte_eth_tx_queue_setup(portid, 0, nb_txd,
			  rte_eth_dev_socket_id(portid),
			  NULL);
		if (ret < 0)
		  rte_exit(EXIT_FAILURE, "rte_eth_tx_queue_setup:err=%d, port=%u\n",
			  ret, (unsigned) portid);


		/* Initialize TX buffers */
		dm->tx_buffer[portid] = rte_zmalloc_socket("tx_buffer",
			  RTE_ETH_TX_BUFFER_SIZE(MAX_PKT_BURST), 0,
			  rte_eth_dev_socket_id(portid));
		if (dm->tx_buffer[portid] == NULL)
		  rte_exit(EXIT_FAILURE, "Cannot allocate buffer for tx on port %u\n",
				  (unsigned) portid);

		rte_eth_tx_buffer_init(dm->tx_buffer[portid], MAX_PKT_BURST);

		ret = rte_eth_tx_buffer_set_err_callback(dm->tx_buffer[portid],
			  rte_eth_tx_buffer_count_callback,
			  NULL);
		if (ret < 0)
			  rte_exit(EXIT_FAILURE, "Cannot set error callback for "
					  "tx buffer on port %u\n", (unsigned) portid);

					  
		/* Start device */
		ret = rte_eth_dev_start(portid);
		if (ret < 0)
		  rte_exit(EXIT_FAILURE, "rte_eth_dev_start:err=%d, port=%u\n",
				ret, (unsigned) portid);

		printf("done: \n");

		rte_eth_promiscuous_enable(portid);
	}

	check_linkstatus(nb_ports, conf->portmask);

#endif
}


dpdk_main_t dpdk_main = {
	.conf = &dpdk_config_main,
	.stat_poll_interval = DPDK_STATS_POLL_INTERVAL,
	.link_state_poll_interval = DPDK_LINK_POLL_INTERVAL,
	.ext_private = &dp_private_main
};

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

void dp_start_dpdk(struct vlib_main_t *vm) {
	vm->dp_running_fn = dp_dpdk_running_fn;
	dpdk_main_t *dm = &dpdk_main;
	char thrgp_name[128] = {0}; 
	u32 max_lcores = rte_lcore_count();
	int i;
	
	ThreadVars *tv;
	DecodeThreadVars *dtv;
	PacketQueue *pq;

	dpdk_main.conf->priv_size = vm->extra_priv_size;

	printf ("Master Lcore @ %d/%d\n", rte_get_master_lcore(),
		max_lcores);
	
	for (i = 0; i < (int)max_lcores; i ++) {
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
											  dp_dpdk_perf_tmr_handler, 0, NULL, 3000);

	/* launch per-lcore init on every lcore */
	rte_eal_mp_remote_launch(vm->dp_running_fn, NULL, SKIP_MASTER);
}




