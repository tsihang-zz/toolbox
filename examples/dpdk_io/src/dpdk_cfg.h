#ifndef DPDK_CFG_H
#define DPDK_CFG_H

#include <rte_common.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_byteorder.h>
#include <rte_atomic.h>
#include <rte_launch.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_debug.h>
#include <rte_ring.h>
#include <rte_log.h>
#include <rte_mempool.h>
#include <rte_memcpy.h>
#include <rte_mbuf.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_malloc.h>
#include <rte_fbk_hash.h>
#include <rte_string_fns.h>
#include <rte_cycles.h>

struct dpdk_lcore_rx_queue_cfg_t {
	uint16_t		rx_queue_id;
};

struct dpdk_port_cfg_t {
	uint16_t	portid;
};

struct dpdk_cfg_t {
	uint8_t		nr_ports;
	struct dpdk_port_cfg_t port[RTE_MAX_ETHPORTS];
	const char	*pktmbuf_pool_name;
	/* The mbuf pool for packet rx */
	struct rte_mempool *pktmbuf_pool;
	
	struct dpdk_lcore_rx_queue_cfg_t lcore_rx_queue[RTE_MAX_LCORE];
};

extern struct dpdk_cfg_t dpdk_main_cfg;

extern int dpdk_parse_portmask (uint8_t max_ports, const char *portmask);
extern void dpdk_init_lcore_rx_queues(void);
extern void dpdk_init_mbuf_pools(void);
extern void dpdk_init_ports(void);

#endif
