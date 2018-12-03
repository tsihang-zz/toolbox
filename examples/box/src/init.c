#include "oryx.h"
#include "dpdk.h"
#include "config.h"
#include "iface.h"
#include "dp.h"

vlib_main_t vlib_main;

static const char short_options[] =
	"p:"  /* portmask */
	"P"   /* promiscuous */
	"L"   /* enable long prefix match */
	"E"   /* enable exact match */
	;

#define CMD_LINE_OPT_CONFIG "config"
#define CMD_LINE_OPT_NO_NUMA "no-numa"
#define CMD_LINE_OPT_IPv6 "ipv6"
#define CMD_LINE_OPT_ENABLE_JUMBO "enable-jumbo"
#define CMD_LINE_OPT_HASH_ENTRY_NUM "hash-entry-num"

enum {
	/* long options mapped to a short option */

	/* first long only option value must be >= 256, so that we won't
	 * conflict with short options */
	CMD_LINE_OPT_MIN_NUM = 256,
	CMD_LINE_OPT_CONFIG_NUM,
	CMD_LINE_OPT_NO_NUMA_NUM,
	CMD_LINE_OPT_IPv6_NUM,
	CMD_LINE_OPT_ENABLE_JUMBO_NUM,
	CMD_LINE_OPT_HASH_ENTRY_NUM_NUM,
};

static const struct option lgopts[] = {
	{CMD_LINE_OPT_CONFIG, 1, 0, CMD_LINE_OPT_CONFIG_NUM},
	{CMD_LINE_OPT_NO_NUMA, 0, 0, CMD_LINE_OPT_NO_NUMA_NUM},
	{CMD_LINE_OPT_IPv6, 0, 0, CMD_LINE_OPT_IPv6_NUM},
	{CMD_LINE_OPT_ENABLE_JUMBO, 0, 0, CMD_LINE_OPT_ENABLE_JUMBO_NUM},
	{CMD_LINE_OPT_HASH_ENTRY_NUM, 1, 0, CMD_LINE_OPT_HASH_ENTRY_NUM_NUM},
	{NULL, 0, 0, 0}
};

static struct lcore_params lcore_params_array[MAX_LCORE_PARAMS];
static struct lcore_params lcore_params_array_default[] = {
/* start lcore 1 */	
	{0, 0, 1},
	{0, 1, 1},
	{0, 2, 1},
/* start lcore 2 */	
	{1, 0, 2},
	{1, 1, 2},
	{1, 2, 2},
/* start lcore 3 */	
	{2, 0, 3},
	{2, 1, 3},
	{2, 2, 3},
};

static struct lcore_params * lcore_params = lcore_params_array_default;
static uint16_t nb_lcore_params = sizeof(lcore_params_array_default) /
				sizeof(lcore_params_array_default[0]);

/**< Ports set in promiscuous mode off by default. */
static int promiscuous_on = 1;

static int numa_on = 1; /**< NUMA is enabled by default. */

/* display usage */
static void
usage(const char *prgname)
{
	fprintf (stdout, "%s [EAL options] --"
		" -p PORTMASK"
		" [-P]"
		" [-E]"
		" [-L]"
		" --config (port,queue,lcore)[,(port,queue,lcore)]"
		" [--enable-jumbo [--max-pkt-len PKTLEN]]"
		" [--no-numa]"
		" [--hash-entry-num]\n\n"

		"  -p PORTMASK: Hexadecimal bitmask of ports to configure\n"
		"  -P : Enable promiscuous mode\n"
		"  -E : Enable exact match\n"
		"  -L : Enable longest prefix match (default)\n"
		"  --config (port,queue,lcore): Rx queue configuration\n"
		"  --eth-dest=X,MM:MM:MM:MM:MM:MM: Ethernet destination for port X\n"
		"  --enable-jumbo: Enable jumbo frames\n"
		"  --max-pkt-len: Under the premise of enabling jumbo,\n"
		"                 maximum packet length in decimal (64-9600)\n"
		"  --no-numa: Disable numa awareness\n"
		"  --hash-entry-num: Specify the hash entry number in hexadecimal to be setup\n\n",
		prgname);
}
static void
print_ethaddr(const char *name, const struct ether_addr *eth_addr)
{
	char buf[ETHER_ADDR_FMT_SIZE];
	ether_format_addr(buf, ETHER_ADDR_FMT_SIZE, eth_addr);
	fprintf (stdout, "%s%s", name, buf);
}

static int
box_check_lcore_params(void)
{
	uint8_t queue, lcore;
	uint16_t i;
	int socketid;

	for (i = 0; i < nb_lcore_params; ++i) {
		queue = lcore_params[i].queue_id;
		if (queue >= MAX_RX_QUEUE_PER_PORT) {
			oryx_loge(-1, "invalid queue number: %hhu\n", queue);
			return -1;
		}
		lcore = lcore_params[i].lcore_id;
		if (!rte_lcore_is_enabled(lcore)) {
			oryx_loge(-1, "error: lcore %hhu is not enabled in lcore mask\n", lcore);
			return -1;
		}
		if ((socketid = rte_lcore_to_socket_id(lcore) != 0) &&
			(numa_on == 0)) {
			oryx_loge(-1, "warning: lcore %hhu is on socket %d with numa off \n",
				lcore, socketid);
		}
	}
	return 0;
}

static int
box_check_port_config(const uint32_t portmask, const unsigned nb_ports)
{
	unsigned portid;
	uint16_t i;

	for (i = 0; i < nb_lcore_params; ++i) {
		portid = lcore_params[i].port_id;
		
		if ((portmask & (1 << portid)) == 0) {
			fprintf(stdout,
				"port %u is not enabled in port mask\n", portid);
			return -1;
		}
		
		if (portid >= nb_ports) {
			fprintf (stdout, "port %u is not present on the board\n", portid);
			return -1;
		}
	}
	return 0;
}

static int
box_parse_max_pkt_len(const char *pktlen)
{
	char *end = NULL;
	unsigned long len;

	/* parse decimal string */
	len = strtoul(pktlen, &end, 10);
	if ((pktlen[0] == '\0') || (end == NULL) || (*end != '\0'))
		return -1;

	if (len == 0)
		return -1;

	return len;
}

/**
 * The ports to be used by the application are passed in
 * the form of a bitmask. This function parses the bitmask
 * and places the port numbers to be used into the port[]
 * array variable
 */
static int box_parse_portmask(const char *portmask)
{
	char *end = NULL;
	unsigned long pm = 0;

	if((portmask == NULL) || (portmask[0] == '\0'))
		return -1;
	
	/* parse hexadecimal string */
	pm = strtoul(portmask, &end, 16);
	if ((portmask[0] == '\0') || (end == NULL) || (*end != '\0'))
		return -1;

	return pm;
}

static int
box_parse_config(const char *q_arg)
{
	char s[256];
	const char *p, *p0 = q_arg;
	char *end;
	enum fieldnames {
		FLD_PORT = 0,
		FLD_QUEUE,
		FLD_LCORE,
		_NUM_FLD
	};
	unsigned long int_fld[_NUM_FLD];
	char *str_fld[_NUM_FLD];
	int i;
	unsigned size;

	nb_lcore_params = 0;

	while ((p = strchr(p0,'(')) != NULL) {
		++p;
		if((p0 = strchr(p,')')) == NULL)
			return -1;

		size = p0 - p;
		if(size >= sizeof(s))
			return -1;

		snprintf(s, sizeof(s), "%.*s", size, p);
		if (rte_strsplit(s, sizeof(s), str_fld, _NUM_FLD, ',') != _NUM_FLD)
			return -1;
		for (i = 0; i < _NUM_FLD; i++){
			errno = 0;
			int_fld[i] = strtoul(str_fld[i], &end, 0);
			if (errno != 0 || end == str_fld[i] || int_fld[i] > 255)
				return -1;
		}
		if (nb_lcore_params >= MAX_LCORE_PARAMS) {
			fprintf (stdout, "exceeded max number of lcore params: %hu\n",
				nb_lcore_params);
			return -1;
		}
		lcore_params_array[nb_lcore_params].port_id =
			(uint8_t)int_fld[FLD_PORT];
		lcore_params_array[nb_lcore_params].queue_id =
			(uint8_t)int_fld[FLD_QUEUE];
		lcore_params_array[nb_lcore_params].lcore_id =
			(uint8_t)int_fld[FLD_LCORE];
		++nb_lcore_params;
	}
	lcore_params = lcore_params_array;
	return 0;
}


/* Parse the argument given in the command line of the application */
static int box_parse_args(int argc, char **argv)
{
	int opt, ret;
	char **argvopt;
	int option_index;
	char *prgname = argv[0];
	dpdk_main_t *dm = &dpdk_main;
	dpdk_config_main_t *conf = dm->conf;
	struct rte_eth_conf *pcfg = &dpdk_eth_default_conf;

	argvopt = argv;

	/* Error or normal output strings. */
	const char *str1 = "Invalid portmask";
	const char *str5 = "Invalid config";
	const char *str6 = "NUMA is disabled";
	const char *str8 = "Jumbo frame is enabled - disabling simple TX path";
	const char *str9 = "Invalid packet length";
	const char *str10 = "Set jumbo frame max packet len to ";

	while ((opt = getopt_long(argc, argvopt, short_options,
				lgopts, &option_index)) != EOF) {

		switch (opt) {
		/* portmask */
		case 'p':
			conf->portmask = box_parse_portmask(optarg);
			if (conf->portmask == 0) {
				fprintf (stdout, "%s: %s\n", prgname, str1);
				usage(prgname);
				return -1;
			}
			break;

		case 'P':
			break;

		case 'E':
			break;

		case 'L':
			break;

		/* long options */
		case CMD_LINE_OPT_CONFIG_NUM:
			ret = box_parse_config(optarg);
			if (ret) {
				fprintf (stdout, "%s: %s\n", prgname, str5);
				usage(prgname);
				return -1;
			}
			break;

		case CMD_LINE_OPT_NO_NUMA_NUM:
			fprintf (stdout, "%s: %s\n", prgname, str6);
			numa_on = 0;
			break;

		case CMD_LINE_OPT_ENABLE_JUMBO_NUM: {
			struct option lenopts = {
				"max-pkt-len", required_argument, 0, 0
			};

			fprintf (stdout, "%s: %s\n", prgname, str8);
			pcfg->rxmode.jumbo_frame = 1;

			/*
			 * if no max-pkt-len set, use the default
			 * value ETHER_MAX_LEN.
			 */
			if (getopt_long(argc, argvopt, "",
					&lenopts, &option_index) == 0) {
				ret = box_parse_max_pkt_len(optarg);
				if ((ret < 64) ||
					(ret > MAX_JUMBO_PKT_LEN)) {
					fprintf (stdout, "%s: %s\n", prgname, str9);
					usage(prgname);
					return -1;
				}
				pcfg->rxmode.max_rx_pkt_len = ret;
			}
			fprintf (stdout, "%s: %s %u\n", prgname, str10,
				(unsigned int)pcfg->rxmode.max_rx_pkt_len);
			break;
		}

		case CMD_LINE_OPT_HASH_ENTRY_NUM_NUM:
			break;

		default:
			usage(prgname);
			return -1;
		}
	}
				
	if (optind >= 0)
		argv[optind-1] = prgname;

	ret = optind-1;
	optind = 1; /* reset getopt lib */
	return ret;
}
static uint8_t
box_nr_rx_queue_of_port(const uint8_t port)
{
	int queue = -1;
	uint16_t i;

	for (i = 0; i < nb_lcore_params; ++i) {
		if (lcore_params[i].port_id == port) {
			if (lcore_params[i].queue_id == queue + 1)
				queue = lcore_params[i].queue_id;
			else
				rte_exit(EXIT_FAILURE, "queue ids of the port %d must be"
						" in sequence and must start with 0\n",
						lcore_params[i].port_id);
		}
	}
	return (uint8_t)(++queue);
}

static int
box_init_lcore_rx_queues(void)
{
	uint16_t i, nb_rx_queue;
	uint8_t lcore;

	for (i = 0; i < nb_lcore_params; ++i) {
		lcore = lcore_params[i].lcore_id;
		nb_rx_queue = lcore_conf[lcore].n_rx_queue;
		if (nb_rx_queue >= MAX_RX_QUEUE_PER_LCORE) {
			fprintf (stdout, "error: too many queues (%u) for lcore: %u\n",
				(unsigned)nb_rx_queue + 1, (unsigned)lcore);
			return -1;
		} else {
			lcore_conf[lcore].rx_queue_list[nb_rx_queue].port_id =
				lcore_params[i].port_id;
			lcore_conf[lcore].rx_queue_list[nb_rx_queue].queue_id =
				lcore_params[i].queue_id;
			lcore_conf[lcore].n_rx_queue++;
		}
	}
	return 0;
}


static int
box_init_mem(void)
{
	dpdk_main_t *dm = &dpdk_main;
	dpdk_config_main_t *conf = dm->conf;
	int socketid;
	unsigned lcore_id;
	char s[64];

	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
		if (rte_lcore_is_enabled(lcore_id) == 0)
			continue;

		if (numa_on)
			socketid = rte_lcore_to_socket_id(lcore_id);
		else
			socketid = 0;

		if (socketid >= VLIB_SOCKETS) {
			rte_exit(EXIT_FAILURE,
				"Socket %d of lcore %u is out of range %d\n",
				socketid, lcore_id, VLIB_SOCKETS);
		}

		if (dm->pktmbuf_pool[socketid] == NULL) {
			snprintf(s, sizeof(s), "mbuf_pool_%d", socketid);
			dm->pktmbuf_pool[socketid] =
				rte_pktmbuf_pool_create(s,
							conf->nr_mbufs,
							DPDK_MEMPOOL_DEFAULT_CACHE_SIZE,
							conf->mempool_priv_size,
							DPDK_MEMPOOL_DEFAULT_BUFFER_SIZE,
							socketid);
			if (dm->pktmbuf_pool[socketid] == NULL)
				rte_exit(EXIT_FAILURE,
					"Cannot init mbuf pool on socket %d\n",
					socketid);
			else
				fprintf (stdout, "Allocated mbuf pool on socket %d\n",
					socketid);
			//classify_setup_em (socketid);
			//classify_setup_acl(socketid);
		}
	}
	return 0;
}


/**
 * Main init function for the multi-process server app,
 * calls subfunctions to do each stage of the initialisation.
 */
__oryx_always_extern__
int box_init
(
	IN int argc,
	IN char **argv
)
{
	int		err,
			retval;
	uint8_t portid,
			nb_rx_queue,
			queue,
			socketid;
	uint32_t nr_tx_queue;
	uint16_t queueid;
	unsigned lcore_id;

	struct lcore_conf *qconf;
	struct rte_eth_dev_info dev_info;
	struct rte_eth_txconf *txconf;

	vlib_main_t *vm = &vlib_main;
	dpdk_main_t *dm = &dpdk_main;
	dpdk_config_main_t *conf = dm->conf;
	struct rte_eth_conf *pcfg = &dpdk_eth_default_conf;
	dpdk_dev_t *dev;
	
	vm->progname = argv[0];

	dp_check_port(vm);

	/* init EAL, parsing EAL args */
	retval = rte_eal_init(argc, argv);
	if (retval < 0) {
		rte_exit(EXIT_FAILURE,
			"Invalid EAL parameters\n");
	}
	
	argc -= retval;
	argv += retval;

	vm->nb_lcores = rte_lcore_count();

	/* get total number of ports */
	vm->nr_sys_ports = rte_eth_dev_count();	
	if (vm->nr_sys_ports == 0) {
		oryx_panic(-1,
			"No Ethernet ports - bye\n");
	}

	/* parse application arguments (after the EAL ones) */
	err = box_parse_args(argc, argv);
	if (err < 0) {
		oryx_panic(-1,
			"Invalid eal parameters\n");
	}	

	oryx_logn("================ DPDK INIT ARGS =================");
	oryx_logn("%20s%15x", "ul_core_mask", conf->coremask);
	oryx_logn("%20s%15x", "ul_port_mask", conf->portmask);
	oryx_logn("%20s%15d", "n_pool_mbufs", conf->nr_mbufs);
	oryx_logn("%20s%15d", "rte_cachelin", RTE_CACHE_LINE_SIZE);
	oryx_logn("%20s%15d", "mp_data_room", DPDK_MEMPOOL_DEFAULT_BUFFER_SIZE);
	oryx_logn("%20s%15d", "mp_cache_siz", DPDK_MEMPOOL_DEFAULT_CACHE_SIZE);
	oryx_logn("%20s%15d", "mp_priv_size", conf->mempool_priv_size);
	oryx_logn("%20s%15d", "n_rx_desc", (int)conf->num_rx_desc);
	oryx_logn("%20s%15d", "n_tx_desc", (int)conf->num_tx_desc);
	oryx_logn("%20s%15d", "socket_id", rte_socket_id());

	if (box_check_lcore_params() < 0)
		rte_exit(EXIT_FAILURE, "check_lcore_params failed\n");

	err = box_init_lcore_rx_queues();
	if (err < 0)
		rte_exit(EXIT_FAILURE, "init_lcore_rx_queues failed\n");


	if (box_check_port_config(conf->portmask, vm->nr_sys_ports) < 0)
		rte_exit(EXIT_FAILURE, "check_port_config failed\n");

	fprintf(stdout,
		"Master Lcore %d, nb_lcores %d\n", rte_get_master_lcore(), vm->nb_lcores);

	/* initialize all ports */
	for (portid = 0; portid < vm->nr_sys_ports; portid++) {

		dev = &dm->devs[portid];
		
		/* skip ports that are not enabled */
		if ((conf->portmask & (1 << portid)) == 0) {
			fprintf(stdout,
				"\nSkipping disabled port %d\n", portid);
			continue;
		}

		/* init port */
		fprintf (stdout, "Initializing port %d ... ", portid);
		fflush(stdout);

		nb_rx_queue = box_nr_rx_queue_of_port(portid);
		BUG_ON(nb_rx_queue == 0);
		nr_tx_queue = vm->nb_lcores;
		if (nr_tx_queue > MAX_TX_QUEUE_PER_PORT)
			nr_tx_queue = MAX_TX_QUEUE_PER_PORT;
		fprintf (stdout, "Creating queues: nb_rxq=%d nb_txq=%u... ",
			nb_rx_queue, (unsigned)nr_tx_queue);
		err = rte_eth_dev_configure(portid, nb_rx_queue,
					(uint16_t)nr_tx_queue, pcfg);
		if (err)
			rte_exit(EXIT_FAILURE,
				"Cannot configure device: err=%d, port=%d\n",
				err, portid);

		rte_eth_macaddr_get(portid, &dev->eth_addr);
		print_ethaddr(" Address:", &dev->eth_addr);
		fprintf (stdout, "\n");

		/* init memory */
		err = box_init_mem();
		if (err)
			rte_exit(EXIT_FAILURE, "init_mem failed\n");

		/* init one TX queue per couple (lcore,port) */
		queueid = 0;
		for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
			if (rte_lcore_is_enabled(lcore_id) == 0)
				continue;

			if (numa_on)
				socketid =
				(uint8_t)rte_lcore_to_socket_id(lcore_id);
			else
				socketid = 0;

			fprintf (stdout, "txq=%u,%d,%d ", lcore_id, queueid, socketid);
			fflush(stdout);

			rte_eth_dev_info_get(portid, &dev_info);
			txconf = &dev_info.default_txconf;
			if (pcfg->rxmode.jumbo_frame)
				txconf->txq_flags = 0;
			err = rte_eth_tx_queue_setup(portid, queueid, RTE_TX_DESC_DEFAULT,
							 socketid, txconf);
			if (err)
				rte_exit(EXIT_FAILURE,
					"rte_eth_tx_queue_setup: err=%d, "
					"port=%d\n", err, portid);

			qconf = &lcore_conf[lcore_id];
			qconf->lcore_id = lcore_id;
			qconf->tx_queue_id[portid] = queueid;
			queueid++;

			qconf->tx_port_id[qconf->n_tx_port] = portid;
			qconf->n_tx_port++;
		}
		fprintf (stdout, "\n");
	}

	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
		if (rte_lcore_is_enabled(lcore_id) == 0)
			continue;
		qconf = &lcore_conf[lcore_id];
		fprintf (stdout, "\nInitializing rx queues on lcore %u ... ", lcore_id);
		fflush(stdout);
		/* init RX queues */
		for(queue = 0; queue < qconf->n_rx_queue; ++queue) {
			portid = qconf->rx_queue_list[queue].port_id;
			queueid = qconf->rx_queue_list[queue].queue_id;

			if (numa_on)
				socketid =
				(uint8_t)rte_lcore_to_socket_id(lcore_id);
			else
				socketid = 0;

			fprintf (stdout, "rxq=(%d,%d,%d) ", portid, queueid, lcore_id);
			fflush(stdout);

			err = rte_eth_rx_queue_setup(portid, queueid, RTE_RX_DESC_DEFAULT,
					socketid,
					NULL,
					dm->pktmbuf_pool[socketid]);
			if (err)
				rte_exit(EXIT_FAILURE,
				"rte_eth_rx_queue_setup: err=%d, port=%d\n",
				err, portid);
		}
	}

	fprintf (stdout, "\n");

	/* start ports */
	for (portid = 0; portid < vm->nr_sys_ports; portid++) {
		if ((conf->portmask & (1 << portid)) == 0) {
			continue;
		}
		/* Start device */
		err = rte_eth_dev_start(portid);
		if (err)
			rte_exit(EXIT_FAILURE,
				"rte_eth_dev_start: err=%d, port=%d\n",
				err, portid);

		/*
		 * If enabled, put device in promiscuous mode.
		 * This allows IO forwarding mode to forward packets
		 * to itself through 2 cross-connected	ports of the
		 * target machine.
		 */
		if (promiscuous_on)
			rte_eth_promiscuous_enable(portid);
	}

	fprintf (stdout, "\n");

	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
		if (rte_lcore_is_enabled(lcore_id) == 0)
			continue;
		qconf = &lcore_conf[lcore_id];
		for (queue = 0; queue < qconf->n_rx_queue; ++queue) {
			portid = qconf->rx_queue_list[queue].port_id;
			queueid = qconf->rx_queue_list[queue].queue_id;
#if defined(HAVE_DPDK_BUILT_IN_PARSER)
			if (prepare_ptype_parser(portid, queueid, (void*)qconf) == 0)
				rte_exit(EXIT_FAILURE, "ptype check fails\n");
#endif
		}
	}

	return 0;
}



