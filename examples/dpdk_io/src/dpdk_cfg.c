#include "oryx.h"
#include "dpdk_cfg.h"

#define RTE_CHECK_INTERVAL 100 /* 100ms */
#define RTE_MAX_CHECK_TIME 90 /* 9s (90 * 100ms) in total */

/* allow max jumbo frame 9 KB */
#define RTE_JUMBO_FRAME_MAX_SIZE 0x2400

#define RTE_RX_DESC_DEFAULT 512
#define RTE_TX_DESC_DEFAULT 512

#define RTE_RX_MBUF_DATA_SIZE	2048
#define RTE_MBUF_OVERHEAD	(sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM)
#define RTE_MBUF_SIZE (RTE_RX_MBUF_DATA_SIZE + RTE_MBUF_OVERHEAD)

#define RTE_MBUFS_PER_PORT	1024
#define RTE_MBUFS		(512 * RTE_MBUFS_PER_PORT)

/*
 * RX and TX Prefetch, Host, and Write-back threshold values should be
 * carefully set for optimal performance. Consult the network
 * controller's datasheet and supporting DPDK documentation for guidance
 * on how these parameters should be set.
 */
/* Default configuration for rx and tx thresholds etc. */
/*
 * These default values are optimized for use with the Intel(R) 82599 10 GbE
 * Controller and the DPDK ixgbe PMD. Consider using other values for other
 * network controllers and/or network drivers.
 */
#define RTE_DEFAULT_PTHRESH 36
#define RTE_DEFAULT_RX_HTHRESH 8
#define RTE_DEFAULT_TX_HTHRESH 0
#define RTE_DEFAULT_WTHRESH 0

static const struct rte_eth_rxconf rx_conf_default = {
		.rx_thresh = {
				.pthresh = RTE_DEFAULT_PTHRESH,
				.hthresh = RTE_DEFAULT_RX_HTHRESH,
				.wthresh = RTE_DEFAULT_WTHRESH,
		},
};

static const struct rte_eth_txconf tx_conf_default = {
		.tx_thresh = {
				.pthresh = RTE_DEFAULT_PTHRESH,
				.hthresh = RTE_DEFAULT_TX_HTHRESH,
				.wthresh = RTE_DEFAULT_WTHRESH,
		},
		.tx_free_thresh = 0, /* Use PMD default values */
		.tx_rs_thresh = 0, /* Use PMD default values */
};

struct dpdk_cfg_t dpdk_main_cfg = {
	.nr_ports = 0,
	.pktmbuf_pool_name = "MBUFS_POOL",
};

/**
 * Initialise an individual port:
 * - configure number of rx and tx rings
 * - set up each rx ring, to pull from the main mbuf pool
 * - set up each tx ring
 * - start the port and report its status to stdout
 */
static int init_one_port(uint8_t portid)
{
	/* for port configuration all features are off by default */
	const struct rte_eth_conf port_conf = {
		.rxmode = {
			.mq_mode = ETH_MQ_RX_RSS,
			.max_rx_pkt_len = ETHER_MAX_LEN,
			.split_hdr_size = 0,
			.header_split	= 0, /**< Header Split disabled */
			.hw_ip_checksum = 1, /**< IP checksum offload enabled */
			.hw_vlan_filter = 0, /**< VLAN filtering disabled */
			.jumbo_frame	= 0, /**< Jumbo Frame Support disabled */
			.hw_strip_crc	= 1, /**< CRC stripped by hardware */
		},
		.rx_adv_conf = {
			.rss_conf = {
				.rss_key = NULL,
				.rss_hf = ETH_RSS_IP | ETH_RSS_TCP | ETH_RSS_UDP,
			},
		},
		.txmode = {
			.mq_mode = ETH_MQ_TX_NONE,
		},
	};

#if (1)
	const uint16_t rx_rings = 1, tx_rings = 1;
	const uint16_t rx_ring_size = (RTE_RX_DESC_DEFAULT * 2);
	const uint16_t tx_ring_size = (RTE_TX_DESC_DEFAULT);
#else
	const uint16_t rx_rings = nb_lcores - 1, tx_rings = 1;
	const uint16_t rx_ring_size = (RTE_RX_DESC_DEFAULT);
	const uint16_t tx_ring_size = (RTE_TX_DESC_DEFAULT);
#endif
	
	uint16_t q;
	int retval;

	fflush(stdout);

	/* Standard DPDK port initialisation - config port, then set up
	 * rx and tx rings */
	retval = rte_eth_dev_configure(portid, rx_rings, tx_rings, &port_conf);
	if (retval < 0){
		fprintf (stdout,
			"Warning(%d): Cannot configure port %u!\n", retval, portid);
		return retval;
	}


	for (q = 0; q < rx_rings; q++) {
		retval = rte_eth_rx_queue_setup(portid, q, rx_ring_size,
							rte_eth_dev_socket_id(portid), &rx_conf_default, dpdk_main_cfg.pktmbuf_pool);
		if (retval < 0){
			fprintf (stdout,
			 "Warning(%d): Cannot setup Rx queue for port %u!\n", retval, portid);
			continue;
		}

	}

	for (q = 0; q < tx_rings; q ++) {
		retval = rte_eth_tx_queue_setup(portid, q, tx_ring_size,
				rte_eth_dev_socket_id(portid), &tx_conf_default);
		if (retval < 0){
			fprintf (stdout,
			 "Warning(%d): Cannot setup Tx queue for port %u!\n", retval, portid);
			continue;
		}
	}

	rte_eth_promiscuous_enable(portid);

	retval	= rte_eth_dev_start(portid);
	if (retval < 0) {
		fprintf (stdout,
			 "Warning(%d): Cannot start port %d!\n", retval, portid);
		return retval;
	}

	return 0;
}


/* Check the link status of all ports in up to 9s, and print them finally */
static void check_all_ports_link_status(uint8_t nr_ports, uint32_t nr_port_mask)
{
	int i;
	uint8_t portid, count, all_ports_up, print_flag = 0;
	struct rte_eth_link link;
	struct dpdk_port_cfg_t *pcfg;
	
	fprintf (stdout,
		"\nChecking link status");
	fflush(stdout);
	for (count = 0; count <= RTE_MAX_CHECK_TIME; count++) {
		all_ports_up = 1;
		for (i = 0; i < nr_ports; i ++) {
			pcfg	= &dpdk_main_cfg.port[i];
			portid	= pcfg->portid;
			if ((nr_port_mask & (1 << portid)) == 0)
				continue;
			memset(&link, 0, sizeof(link));
			rte_eth_link_get_nowait(portid, &link);
			/* print link status if flag set */
			if (print_flag == 1) {
				if (link.link_status)
					fprintf (stdout,
						"Port %d Link Up - speed %u "
						"Mbps - %s\n", portid,
						(unsigned)link.link_speed,
				(link.link_duplex == ETH_LINK_FULL_DUPLEX) ?
					("full-duplex") : ("half-duplex\n"));
				else
					fprintf (stdout,
						"Port %d Link Down\n", portid);
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
			fprintf (stdout,".");
			fflush(stdout);
			rte_delay_ms(RTE_CHECK_INTERVAL);
		}

		/* set the print_flag if all ports up or timeout */
		if (all_ports_up == 1 || count == (RTE_MAX_CHECK_TIME - 1)) {
			print_flag = 1;
			fprintf (stdout,"done\n");
		}
	}
}

/**
 * The ports to be used by the application are passed in
 * the form of a bitmask. This function parses the bitmask
 * and places the port numbers to be used into the port[]
 * array variable
 */
int dpdk_parse_portmask (uint8_t max_ports, const char *portmask)
{
	char *end = NULL;
	unsigned long pm;
	uint8_t count = 0;
	struct dpdk_port_cfg_t *pcfg;

	BUG_ON((portmask == NULL) || (*portmask == '\0'));
	
	/* convert parameter to a number and verify */
	pm = strtoul(portmask, &end, 16);
	if (end == NULL || *end != '\0' || pm == 0)
		oryx_logn("Warning: invalid port mask %s\n", portmask);

	dpdk_main_cfg.nr_port_mask = pm;
	
	/* loop through bits of the mask and mark ports */
	while (pm != 0) {
		if (pm & 0x01) { /* bit is set in mask, use port */
 			if (count >= max_ports) {
				oryx_logn("WARNING: requested port %u not present"
				" - ignoring\n", (unsigned)count);
 			}
			else {
			 	pcfg = &dpdk_main_cfg.port[dpdk_main_cfg.nr_ports ++];
			 	pcfg->portid = count;
			}
		}
		
		pm >>= 1;
		count ++;
	}
	
	return 0;
}

void dpdk_init_lcore_rx_queues(void)
{
	unsigned i;
	unsigned lcore_master = rte_get_master_lcore();
	uint16_t queue_id = 0;
	struct dpdk_lcore_rx_queue_cfg_t *lq_cfg;
	
	for (i = 0; i < RTE_MAX_LCORE; i++) {
		if (rte_lcore_is_enabled(i) && i != lcore_master) {
			lq_cfg = &dpdk_main_cfg.lcore_rx_queue[i];
			lq_cfg->rx_queue_id = queue_id ++;
		}
	}
}

/**
 * Initialise the mbuf pool for packet reception for the NIC, and any other
 * buffer pools needed by the app - currently none.
 */
void dpdk_init_mbuf_pools(void)
{
	const unsigned num_mbufs = (dpdk_main_cfg.nr_ports * RTE_MBUFS);

	/* don't pass single-producer/single-consumer flags to mbuf create as it
	 * seems faster to use a cache instead */
	fprintf(stdout, "Creating mbuf pool '%s' [%u mbufs, mbuf_size %lu] ...\n",
			dpdk_main_cfg.pktmbuf_pool_name, num_mbufs , RTE_MBUF_SIZE);

	dpdk_main_cfg.pktmbuf_pool = rte_mempool_create(
										dpdk_main_cfg.pktmbuf_pool_name,
										num_mbufs,
										RTE_MBUF_SIZE,
										512,
										sizeof(struct rte_pktmbuf_pool_private),
										rte_pktmbuf_pool_init,
										NULL,
										rte_pktmbuf_init,
										NULL,
										rte_socket_id(),
										0);
	BUG_ON(dpdk_main_cfg.pktmbuf_pool == NULL);
}

void dpdk_init_ports(void)
{
	int i;
	int retval;
	struct dpdk_port_cfg_t *pcfg;
	
	/* now initialise the ports we will use */
	for (i = 0; i < dpdk_main_cfg.nr_ports; i++) {
		pcfg = &dpdk_main_cfg.port[i];
		fprintf(stdout, "Port %u init ... ", pcfg->portid);
		retval = init_one_port(i);
		if (retval != 0)
			rte_exit(EXIT_FAILURE, "Cannot initialise port %u\n", pcfg->portid);
		fprintf(stdout, "%s", "done\n");
	}

	check_all_ports_link_status(dpdk_main_cfg.nr_ports, dpdk_main_cfg.nr_port_mask);
}
