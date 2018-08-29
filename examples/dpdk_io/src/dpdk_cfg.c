#include "oryx.h"
#include "dpdk_cfg.h"

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
#if 0
	const struct rte_eth_conf port_conf = {
		.rxmode = {
			.mq_mode = ETH_MQ_RX_RSS,
			.max_rx_pkt_len = RTE_JUMBO_FRAME_MAX_SIZE,
			.split_hdr_size = 0,
			.header_split	= 0, /**< Header Split disabled */
			.hw_ip_checksum = 0, /**< IP checksum offload enabled */
			.hw_vlan_filter = 0, /**< VLAN filtering disabled */
			.jumbo_frame	= 1, /**< Jumbo Frame Support disabled */
			.hw_strip_crc	= 0, /**< CRC stripped by hardware */
		}
	};
#else
struct rte_eth_conf port_conf = {
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

#endif

#if (1)
	uint32_t nb_lcores = rte_lcore_count();
	const uint16_t rx_rings = 1, tx_rings = 1;
	const uint16_t rx_ring_size = (RTE_RX_DESC_DEFAULT * 2);
	const uint16_t tx_ring_size = (RTE_TX_DESC_DEFAULT);
#else
	uint32_t nb_lcores = rte_lcore_count();
	const uint16_t rx_rings = nb_lcores - 1, tx_rings = 1;
	const uint16_t rx_ring_size = (RTE_RX_DESC_DEFAULT);
	const uint16_t tx_ring_size = (RTE_TX_DESC_DEFAULT);
#endif
	
	uint16_t q;
	int retval;

	fflush(stdout);

#if 0
	retval = rte_eal_has_hugpages();
	if (retval < 0) {
		fprintf (stdout,
			"Warning(%d): Huge page is not configured!\n", retval);
		return retval;
	}
#endif
	/* Standard DPDK port initialisation - config port, then set up
	 * rx and tx rings */
	retval = rte_eth_dev_configure(portid, rx_rings, tx_rings, &port_conf);
	if (retval < 0){
		fprintf (stdout,
			"Warning(%d): port %u configure failed!\n", retval, portid);
		return retval;
	}


	for (q = 0; q < rx_rings; q++) {
		retval = rte_eth_rx_queue_setup(portid, q, rx_ring_size,
							rte_eth_dev_socket_id(portid), &rx_conf_default, dpdk_main_cfg.pktmbuf_pool);
		if (retval < 0){
			fprintf (stdout,
			 "Warning(%d): port %u set Rx queue failed!\n", retval, portid);
			continue;
		}

	}
#if 1
	for (q = 0; q < tx_rings; q ++) {
		retval = rte_eth_tx_queue_setup(portid, q, tx_ring_size,
				rte_eth_dev_socket_id(portid), &tx_conf_default);
		if (retval < 0){
			fprintf (stdout,
			 "Warning(%d): port %u set Tx queue failed!\n", retval, portid);
			continue;
		}
	}
#endif
	rte_eth_promiscuous_enable(portid);

	retval	= rte_eth_dev_start(portid);
	if (retval < 0) {
		fprintf (stdout,
			 "Warning(%d): Can not start port %d!\n", retval, portid);
		return retval;
	}

	return 0;
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

	BUG_ON((portmask == NULL) || (*portmask == '\0'));
	
	/* convert parameter to a number and verify */
	pm = strtoul(portmask, &end, 16);
	if (end == NULL || *end != '\0' || pm == 0)
		oryx_logn(-1,
			"Warning: invalid port mask %s\n", portmask);

	/* loop through bits of the mask and mark ports */
	while (pm != 0) {
		if (pm & 0x01) { /* bit is set in mask, use port */
 			if (count >= max_ports) {
				oryx_logn("WARNING: requested port %u not present"
				" - ignoring\n", (unsigned)count);
 			}
			else {
			 //   ncapd_share_info->id[ncapd_share_info->nr_ports++] = count;
			 	struct dpdk_port_cfg_t *pcfg = &dpdk_main_cfg.port[dpdk_main_cfg.nr_ports ++];
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

}
