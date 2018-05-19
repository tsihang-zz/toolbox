#ifndef DPDK_INIT_H
#define DPDK_INIT_H

#define MAX_PKT_BURST 32
#define NB_MBUF   8192
#define BURST_TX_DRAIN_US 100 /* TX drain every ~100us */
#define MEMPOOL_CACHE_SIZE 256

struct lcore_queue_conf {
	unsigned n_rx_port;
	unsigned rx_port_list[MAX_RX_QUEUE_PER_LCORE];
} __rte_cache_aligned;

#endif
