#include "oryx.h"
#include "dpdk.h"
#include "common_private.h"
#include "iface_private.h"

struct LcoreConfigContext lconf_ctx[RTE_MAX_LCORE];

struct rte_eth_conf dpdk_eth_default_conf = {
	.rxmode = {
		.mq_mode		= ETH_MQ_RX_RSS,
		.split_hdr_size = 0,
		.header_split   = 0, /**< Header Split disabled */
		.hw_ip_checksum = 1, /**< IP checksum offload enable */
		.hw_vlan_filter = 0, /**< VLAN filtering disabled */
		.jumbo_frame    = 0, /**< Jumbo Frame Support disabled */
		.hw_strip_crc   = 1, /**< CRC stripped by hardware */
	},
	.rx_adv_conf = {
		.rss_conf = {
			.rss_key = NULL,
			.rss_hf = ETH_RSS_IP | ETH_RSS_UDP | ETH_RSS_TCP,
		},
	},

	.txmode = {
		.mq_mode = ETH_MQ_TX_NONE,
	},
};
		
static dpdk_config_main_t dpdk_config_main = {
	.nchannels = 2, 	/** */		
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
	.ext_private = NULL
};


static char *eal_init_argv[1024] = {0};
static int eal_init_argc = 0;
static int eal_args_offset = 0;

extern bool force_quit;

#define MAX_TIMER_PERIOD 86400 /* 1 day max */
/* A tsc-based timer responsible for triggering statistics printout */
uint64_t timer_period = 10; /* default period is 10 seconds */

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
			if (force_quit)
				return;
			all_ports_up = 1;
			for (portid = 0; portid < port_num; portid++) {
				if (force_quit)
					return;
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

static void config_port(vlib_main_t *vm, u8 portid)
{
	dpdk_main_t *dm = &dpdk_main;
	dpdk_config_main_t *conf = dm->conf;
	struct ether_addr eth_addr;
	int ret;
	
	ret = rte_eth_dev_configure(portid, 1, 1, conf->ethdev_default_conf);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Cannot configure device: err=%d, port=%u\n",
			  ret, (unsigned) portid);
	
	rte_eth_macaddr_get(portid,&eth_addr);
	
	printf("\n\t\tMAC address: %02X:%02X:%02X:%02X:%02X:%02X\n",
			eth_addr.addr_bytes[0],
			eth_addr.addr_bytes[1],
			eth_addr.addr_bytes[2],
			eth_addr.addr_bytes[3],
			eth_addr.addr_bytes[4],
			eth_addr.addr_bytes[5]);

	fflush(stdout);

}

static void init_rx_queue(vlib_main_t *vm, u8 portid)
{
	dpdk_main_t *dm = &dpdk_main;
	dpdk_config_main_t *conf = dm->conf;
	int ret;
	
	ret = rte_eth_rx_queue_setup(portid, 0, conf->num_rx_desc,
					 rte_eth_dev_socket_id(portid),
					 NULL,
					 dm->pktmbuf_pools);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "rte_eth_rx_queue_setup:err=%d, port=%u\n",
			  ret, (unsigned) portid);
	
  	printf("\n\t\tsetup Rx queue ... \n");
	fflush(stdout);
}

void
dpdk_tx_buffer_error_callback(struct rte_mbuf **pkts, uint16_t unsent,
		void *userdata)
{
	unsigned i;
	struct iface_t *this = (struct iface_t *)userdata;
	
	oryx_logw(0, "overload. %d @ port %d", unsent, this->ul_id);
	for (i = 0; i < unsent; i++) {
		rte_pktmbuf_free(pkts[i]);
	}
}

static void init_tx_queue(vlib_main_t *vm, u8 portid)
{
	dpdk_main_t *dm = &dpdk_main;
	dpdk_config_main_t *conf = dm->conf;
	int ret;
	/* init one TX queue on each port */
	u16 tx_queue = 0;
	struct rte_eth_dev_info dev_info;
	struct rte_eth_txconf *txconf;
	vlib_port_main_t *vp = &vlib_port_main;
	struct iface_t *iface;

	iface_lookup_id(vp, portid, &iface);
	if(!iface) {
		oryx_panic(-1, "Cannot locate a tx_port[%d].", portid);
	}

	rte_eth_dev_info_get(portid, &dev_info);
	txconf = &dev_info.default_txconf;
	if (conf->ethdev_default_conf->rxmode.jumbo_frame)
		txconf->txq_flags = 0;
	
	ret = rte_eth_tx_queue_setup(portid, tx_queue, conf->num_tx_desc,
			  rte_eth_dev_socket_id(portid), txconf);
	if (ret < 0)
	  rte_exit(EXIT_FAILURE, "rte_eth_tx_queue_setup:err=%d, port=%u\n",
		  ret, (unsigned) portid);

	  /* Initialize TX buffers */
	  dm->tx_buffer[portid] = rte_zmalloc_socket("tx_buffer",
			  RTE_ETH_TX_BUFFER_SIZE(DPDK_MAX_TX_BURST), 0,
			  rte_eth_dev_socket_id(portid));
	  if (dm->tx_buffer[portid] == NULL)
		  rte_exit(EXIT_FAILURE, "Cannot allocate buffer for tx on port %u\n",
				  (unsigned) portid);
	  
	  rte_eth_tx_buffer_init(dm->tx_buffer[portid], DPDK_MAX_TX_BURST);
	  
	  ret = rte_eth_tx_buffer_set_err_callback(dm->tx_buffer[portid],
			  dpdk_tx_buffer_error_callback,
			  iface);
	  if (ret < 0)
			  rte_exit(EXIT_FAILURE, "Cannot set error callback for "
					  "tx buffer on port %u\n", (unsigned) portid);

  	printf("\n\t\tsetup Tx queue ... tx_burst %d\n", DPDK_MAX_TX_BURST);
	fflush(stdout);

}

static void start_port(vlib_main_t *vm, u8 portid)
{
	int ret;

	/* Start device */
	ret = rte_eth_dev_start(portid);
	if (ret < 0)
	  rte_exit(EXIT_FAILURE, "rte_eth_dev_start:err=%d, port=%u\n",
			ret, (unsigned) portid);

	rte_eth_promiscuous_enable(portid);	
}

void dpdk_env_setup(vlib_main_t *vm)
{
	struct oryx_fmt_buff_t setup_env = FMT_BUFF_INITIALIZATION;
	dpdk_main_t *dm = &dpdk_main;
	dpdk_config_main_t *conf = dm->conf;
	struct LcoreConfigContext *lconf;
	int ret;
	uint8_t nb_ports, nb_lcores;
	uint8_t nb_ports_available;
	uint8_t portid, last_port;
	unsigned lcore_id, rx_lcore_id;	
	
	dpdk_format_eal_args(vm);
	
	/* init EAL */
	ret = rte_eal_init(vm->argc, vm->argv);
	if (ret < 0) {
		log_usage();
		rte_exit(EXIT_FAILURE, "Invalid EAL arguments\n");
	}
	
	vm->argc -= ret;
	vm->argv += ret;

	nb_lcores = rte_lcore_count();
	nb_ports  = rte_eth_dev_count();
	if (nb_ports == 0) {
		log_usage();
		rte_exit(EXIT_FAILURE, "No Ethernet ports - bye\n");
	}

	vm->nb_lcores = nb_lcores;
	vm->nb_ports = nb_ports;
	/* convert to number of cycles */
	timer_period *= rte_get_timer_hz();

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

	/* create the mbuf pool */
	dm->pktmbuf_pools = rte_pktmbuf_pool_create("mbuf_pool", conf->num_mbufs, 
							conf->mempool_cache_size, conf->mempool_priv_size, conf->mempool_data_room_size, rte_socket_id());
	if(likely(dm->pktmbuf_pools == NULL))
		rte_exit(EXIT_FAILURE, "Cannot init mbuf pool\n");

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
		       lconf_ctx[rx_lcore_id].n_rx_port == conf->n_rx_q_per_lcore) {
			rx_lcore_id ++;
			if (rx_lcore_id >= RTE_MAX_LCORE)
				rte_exit(EXIT_FAILURE, "Not enough lcores\n");
			lconf = &lconf_ctx[rx_lcore_id];
		}
			   
	   if (lconf != &lconf_ctx[rx_lcore_id])
		   /* Assigned a new logical core in the loop above. */
		   lconf = &lconf_ctx[rx_lcore_id];
	   
	   lconf->rx_port_list[lconf->n_rx_port] = portid;
	   lconf->n_rx_port++;
	   printf("Lcore %u: RX port %u\n", rx_lcore_id, (unsigned) portid);

	}

	/* Initialise each port */
	for (portid = 0; portid < nb_ports; portid++) {
		/* skip ports that are not enabled */
		if ((conf->portmask & (1 << portid)) == 0)
			continue;
		
		printf("Initializing port %u... ", (unsigned) portid);
		fflush(stdout);

		/* init port */
		config_port(vm, portid);
		/* init one RX queue */
		init_rx_queue(vm, portid);
		/* init one TX queue */
		init_tx_queue(vm, portid);

		printf("done: \n");
		fflush(stdout);

		/* Start device */
		start_port(vm, portid);
	}

	check_linkstatus(nb_ports, conf->portmask);
}

