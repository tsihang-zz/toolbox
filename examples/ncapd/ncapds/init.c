#include "dpdk.h"
#include "oryx.h"
#include "config.h"

#define DECLARE_STATIC_VARIABLE
static uint16_t lcore_rx_queue[RTE_MAX_LCORE];
static const struct rte_eth_rxconf rx_conf_default = {
	.rx_thresh = {
		.pthresh = VLIB_DEFAULT_PTHRESH,
		.hthresh = VLIB_DEFAULT_RX_PTHRESH,
		.wthresh = VLIB_DEFAULT_WTHRESH,
	},
};

static const struct rte_eth_txconf tx_conf_default = {
	.tx_thresh = {
		.pthresh = VLIB_DEFAULT_PTHRESH,
		.hthresh = VLIB_DEFAULT_TX_PTHRESH,
		.wthresh = VLIB_DEFAULT_WTHRESH,
	},
	.tx_free_thresh = 0, /* Use PMD default values */
	.tx_rs_thresh = 0, /* Use PMD default values */
};

/* The mbuf pool for packet rx */
static struct rte_mempool *pktmbuf_pool;

#define DECLARE_EXTERNAL_VARIBALE
vlib_queue_t	*vlib_queues;
vlib_main_t *vlib_main;
const char *progname;


/**
 * Prints out usage information to stdout
 */
static void
usage(void)
{
	printf (
	    "%s [EAL options] -- -p PORTMASK\n"
	    " -p PORTMASK: hexadecimal bitmask of ports to use\n"
	    , progname);
}

/**
 * The ports to be used by the application are passed in
 * the form of a bitmask. This function parses the bitmask
 * and places the port numbers to be used into the port[]
 * array variable
 */
static int
ncapd_args_parse_portmask
(
	IN uint8_t max_ports,
	IN const char *portmask
)
{
	char *end = NULL;
	unsigned long pm;
	uint8_t count = 0,
			nr_ports = 0;
	vlib_main_t *vm = vlib_main;

	if (portmask == NULL || *portmask == '\0')
		return -1;

	/* convert parameter to a number and verify */
	pm = strtoul(portmask, &end, 16);
	if (end == NULL || *end != '\0' || pm == 0)
		return -1;

	/* loop through bits of the mask and mark ports */
	while (pm != 0){
		if (pm & 0x01){ /* bit is set in mask, use port */
			if (count >= max_ports)
				fprintf(stdout, "WARNING: requested port %u not present"
				" - ignoring\n", (unsigned)count);
			else
			    vm->id[nr_ports++] = count;
		}
		pm = (pm >> 1);
		count++;
	}

	/* reset nr_ports to its real value. */
	vm->nr_ports = nr_ports;
	
	return 0;
}

/**
 * The application specific arguments follow the DPDK-specific
 * arguments which are stripped by the DPDK init. This function
 * processes these application arguments, printing usage info
 * on error.
 */
static int
ncapd_args_parse
(
	IN int argc,
	IN char **argv
)
{
	int	option_index,
		opt;
	char **argvopt = argv;
	static struct option lgopts[] = { /* no long options */
		{NULL, 0, 0, 0 }
	};
	vlib_main_t *vm = vlib_main;

	while ((opt = getopt_long(argc, argvopt, "p:", lgopts,
		&option_index)) != EOF){
		switch (opt){
			case 'p':
				if (ncapd_args_parse_portmask(vm->nr_sys_ports, optarg) != 0) {
					return -1;
				}
				break;
			default:
				printf("ERROR: Unknown option '%c'\n", opt);
				return -1;
		}
	}

	if (vm->nr_ports == 0) {
		return -1;
	}

	return 0;
}

static void
ncapd_init_lcore_rx_queues (void)
{
	unsigned i;
	unsigned lcore_master = rte_get_master_lcore();
	uint16_t queue_id = 0;

	for (i = 0; i < RTE_MAX_LCORE; i++) {
		if (rte_lcore_is_enabled(i) && i != lcore_master) {
			lcore_rx_queue[i] = queue_id ++;
		}
	}
}

/**
 * Initialise the mbuf pool for packet reception for the NIC, and any other
 * buffer pools needed by the app - currently none.
 */
static int
ncapd_init_mbuf_pools (void)
{
	vlib_main_t *vm = vlib_main;
	
	const unsigned num_mbufs = (vm->nr_ports *
			(VLIB_RING_SIZE + VLIB_MBUFS_PER_PORT));

	/* don't pass single-producer/single-consumer flags to mbuf create as it
	 * seems faster to use a cache instead */
	fprintf(stdout, "[ncapds] Creating mbuf pool '%s' [%u mbufs, mbuf_size=%lu] ...\n",
			VLIB_MBUF_POOL_NAME, num_mbufs , VLIB_MBUF_SIZE);
	
	pktmbuf_pool = rte_mempool_create(VLIB_MBUF_POOL_NAME, num_mbufs,
			VLIB_MBUF_SIZE, VLIB_MBUF_CACHE_SIZE,
			sizeof(struct rte_pktmbuf_pool_private), rte_pktmbuf_pool_init,
			NULL, rte_pktmbuf_init, NULL, rte_socket_id(), 0);

	return (pktmbuf_pool == NULL); /* 0  on success */
}

/**
 * Initialise an individual port:
 * - configure number of rx and tx rings
 * - set up each rx ring, to pull from the main mbuf pool
 * - set up each tx ring
 * - start the port and report its status to stdout
 */
static int
ncapd_init_port(uint8_t port_num)
{
	/* for port configuration all features are off by default */
	const struct rte_eth_conf port_conf = {
		.rxmode = {
			.mq_mode = ETH_MQ_RX_RSS,
			.max_rx_pkt_len = VLIB_JUMBO_FRAME_SIZE,
			.split_hdr_size = 0,
			.header_split   = 0, /**< Header Split disabled */
			.hw_ip_checksum = 0, /**< IP checksum offload enabled */
			.hw_vlan_filter = 0, /**< VLAN filtering disabled */
			.jumbo_frame    = 1, /**< Jumbo Frame Support disabled */
			.hw_strip_crc   = 0, /**< CRC stripped by hardware */
		}
	};

	const uint16_t	nr_rx_rings = 1,
					nr_tx_rings = 1,
					rx_ring_size = (VLIB_DEFAULT_RX_DESC * 2),
					tx_ring_size = VLIB_DEFAULT_TX_DESC;
	uint16_t		q;
	int err;

	fflush(stdout);

	/* Standard DPDK port initialisation - config port, then set up
	 * rx and tx rings */
	 err = rte_eth_dev_configure(port_num, nr_rx_rings, nr_tx_rings, &port_conf);
	if (err)
		return err;

	for (q = 0; q < nr_rx_rings; q++) {
		err = rte_eth_rx_queue_setup(port_num, q, rx_ring_size,
				rte_eth_dev_socket_id(port_num), &rx_conf_default, pktmbuf_pool);
		if (err) return err;
	}

	for ( q = 0; q < nr_tx_rings; q ++ ) {
		err = rte_eth_tx_queue_setup(port_num, q, tx_ring_size,
				rte_eth_dev_socket_id(port_num), &tx_conf_default);
		if (err) return err;
	}

	rte_eth_promiscuous_enable(port_num);

	err  = rte_eth_dev_start(port_num);
	if (err) return err;

	return 0;
}

/* Check the link status of all ports in up to 9s, and print them finally */
static void
ncapd_check_all_ports_link_status(uint8_t port_num, uint32_t port_mask)
{
#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 90 /* 9s (90 * 100ms) in total */
	uint8_t portid, count, all_ports_up, print_flag = 0;
	struct rte_eth_link link;
	vlib_main_t *vm = vlib_main;

	printf("\nChecking link status");
	fflush(stdout);
	for (count = 0; count <= MAX_CHECK_TIME; count++) {
		all_ports_up = 1;
		for (portid = 0; portid < port_num; portid++) {

			uint8_t id = vm->id[portid % ARRAY_LENGTH(vm->id)];
			if ((port_mask & (1 << id)) == 0)
				continue;
			memset(&link, 0, sizeof(link));
			rte_eth_link_get_nowait(id, &link);
			/* print link status if flag set */
			if (print_flag == 1) {
				if (link.link_status)
					printf("Port %d Link Up - speed %u "
						"Mbps - %s\n", id,
						(unsigned)link.link_speed,
				(link.link_duplex == ETH_LINK_FULL_DUPLEX) ?
					("full-duplex") : ("half-duplex\n"));
				else
					printf("Port %d Link Down\n",
						id);
				continue;
			}
			/* clear all_ports_up flag if any link down */
			if (link.link_status == 0) {
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

/**
 * Set up the DPDK rings which will be used to pass packets, via
 * pointers, between the multi-process server and ncapdc processes.
 * Each ncapdc needs one RX queue.
 */
static int
ncapd_init_rings(void)
{
	int			i,
				socket_id;
	const char * q_name;
	const unsigned ringsize = VLIB_RING_SIZE;
	vlib_main_t	*vm = vlib_main;
	vlib_queue_t *q;

	fprintf(stdout, "%s: Creating %u ncapdc(s)\n", progname, vm->nr_ports);
	
	vlib_queues = rte_malloc("ncapdc details",
						sizeof(vlib_queue_t) * vm->nr_ports, 0);
	if (vlib_queues == NULL) {
		rte_exit(EXIT_FAILURE,
			"Cannot allocate memory for ncapdc program details\n");
	}

	for (i = 0; i < vm->nr_ports; i++) {
		q = &vlib_queues[i];
		
		/* Create an RX queue for each ncapdc */
		socket_id = rte_socket_id();
		q_name = ncapd_get_rx_queue_name(i);

		fprintf(stdout, "[ncapds] Creating RING %s [ringsize=%u] ...\n",
			q_name, ringsize);

		q->rx_q = rte_ring_create(q_name,
				ringsize, socket_id,
				RING_F_SP_ENQ | RING_F_SC_DEQ); /* single prod, single cons */
		if (q->rx_q == NULL) {
			rte_exit(EXIT_FAILURE,
				"Cannot create rx ring queue for ncapdc %u\n", i);
		}
	}

	return 0;
}

/**
 * Main init function for the multi-process server app,
 * calls subfunctions to do each stage of the initialisation.
 */
__oryx_always_extern__
int ncapds_init
(
	IN int argc,
	IN char **argv
)
{
	int		i,
			id,
			err,
			retval;
	
	vlib_main_t *vm = vlib_main;
	
	progname = argv[0];
	
	/* init EAL, parsing EAL args */
	retval = rte_eal_init(argc, argv);
	if (retval < 0) {
		rte_exit(EXIT_FAILURE,
			"Invalid EAL parameters\n");
	}
	argc -= retval;
	argv += retval;

	/* get total number of ports */
	vm->nr_sys_ports = rte_eth_dev_count();

	/* parse additional, application arguments */
	err = ncapd_args_parse(argc, argv);
	if (err){		
		usage();
		rte_exit(EXIT_FAILURE,
			"Invalid APP parameters\n");
	}

	fprintf(stdout, "nr_ports %u/%u\n", vm->nr_ports, vm->nr_sys_ports);

	ncapd_init_lcore_rx_queues();

	/* initialise mbuf pools */
	err = ncapd_init_mbuf_pools();
	if (err) {
		rte_exit(EXIT_FAILURE,
			"Cannot create needed mbuf pools\n");
	}

	/* now initialise the ports we will use */
	for (i = 0; i < vm->nr_ports; i++) {
		id = vm->id[i % ARRAY_LENGTH(vm->id)];
		fprintf(stdout, "[ncapds] Port %u init ... ", id);
		err = ncapd_init_port(id);
		if (err) {
			rte_exit(EXIT_FAILURE,
				"Cannot initialise port %u\n", (unsigned)i);
		}
		fprintf(stdout, "%s", "done\n");
	}

	ncapd_check_all_ports_link_status(vm->nr_ports, (~0x0));

	/* initialise the ncapdc queues/rings for inter-eu comms */
	ncapd_init_rings();

	vm->ul_flags |= NCPAD_SERV_INIT_DONE;
	return 0;
}


