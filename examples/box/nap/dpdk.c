#include "oryx.h"
#include "config.h"
#include "dpdk.h"

/* Global variables. */
struct rte_eth_conf dpdk_eth_default_conf = {
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
		
static dpdk_config_main_t dpdk_config_main = {
	.nr_channels = 2, 	/** */
	.nr_mbufs = 	DPDK_DEFAULT_NB_MBUF,
	.coremask = 0xF,	/** c1,c2,c3 lcores. */
	.portmask = 0x07,	/** 3 xe ports */
	.mempool_priv_size = 0,
	.num_rx_desc = RTE_RX_DESC_DEFAULT,
	.num_tx_desc = RTE_TX_DESC_DEFAULT,
};

dpdk_main_t dpdk_main = {
	.conf = &dpdk_config_main,
	.stat_poll_interval = DPDK_STATS_POLL_INTERVAL,
	.link_state_poll_interval = DPDK_LINK_POLL_INTERVAL,
};

struct lcore_conf lcore_conf[RTE_MAX_LCORE];

/* A tsc-based timer responsible for triggering statistics printout */
uint64_t timer_period = 10; /* default period is 10 seconds */

struct rte_mempool * pktmbuf_pool[VLIB_SOCKETS];

rte_xmm_t mask0;
rte_xmm_t mask1;
rte_xmm_t mask2;

