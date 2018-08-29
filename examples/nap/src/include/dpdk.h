#ifndef DPDK_H
#define DPDK_H

#include <rte_common.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_memzone.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_launch.h>
#include <rte_atomic.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_random.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_ring.h>
#include <rte_version.h>
#include <rte_ethdev.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_udp.h>
#include <rte_string_fns.h>
#include <rte_vect.h>

/** for exact match */
#include <rte_hash.h>
#include <rte_hash_crc.h>
/** for longest prefixe match */
#include <rte_lpm.h>
#include <rte_lpm6.h>
/** for access control list. */
#include <rte_acl.h>

#include "common_private.h"


#define DPDK_BUFFER_PRE_DATA_SIZE	RTE_PKTMBUF_HEADROOM		//(128)

/*
 * Max size of a single packet
 */
#define DPDK_BUFFER_DATA_SIZE		RTE_MBUF_DEFAULT_DATAROOM	//(2048)

/*
 * Size of the data buffer in each mbuf. These two macro means a frame
 */
#define	DPDK_MEMPOOL_DEFAULT_BUFFER_SIZE	\
	(DPDK_BUFFER_DATA_SIZE + DPDK_BUFFER_PRE_DATA_SIZE)

/*
 * This expression is used to calculate the number of mbufs needed
 * depending on user input, taking  into account memory for rx and
 * tx hardware rings, cache per lcore and mtable per port per lcore.
 * RTE_MAX is used to ensure that NB_MBUF never goes below a minimum
 * value of 8192
 */
/* Number of mbufs in mempool that is created */
#define DPDK_DEFAULT_NB_MBUF   (16 << 10)

#define MAX_RX_QUEUE_PER_LCORE 16
#define MAX_TX_QUEUE_PER_LCORE 16

#define MAX_TX_QUEUE_PER_PORT RTE_MAX_ETHPORTS
#define MAX_RX_QUEUE_PER_PORT 128

#define NB_SOCKETS        8

/*
 * How many objects (mbufs) to keep in per-lcore mempool cache
 */
#define DPDK_MEMPOOL_DEFAULT_CACHE_SIZE	256

#define NS_PER_US 1000
#define US_PER_MS 1000
#define MS_PER_S 1000
#define BURST_TX_DRAIN_US 100 /* TX drain every ~100us */

/*
 * Configurable number of RX/TX ring descriptors
 */
#define RTE_RX_DESC_DEFAULT 128
#define RTE_TX_DESC_DEFAULT 512

/*
 * How many packets to attempt to read from NIC in one go
 */
#define DPDK_MAX_RX_BURST 64
#define DPDK_MAX_TX_BURST 32


#define DPDK_STATS_POLL_INTERVAL      (10.0)
#define DPDK_MIN_STATS_POLL_INTERVAL  (0.001)	/* 1msec */
#define DPDK_LINK_POLL_INTERVAL       (3.0)
#define DPDK_MIN_LINK_POLL_INTERVAL   (0.001)	/* 1msec */

#define HASH_ENTRY_NUMBER_DEFAULT	4

struct mbuf_table {
	uint16_t len;
	struct rte_mbuf *m_table[DPDK_MAX_TX_BURST];
};

struct lcore_rx_queue {
	uint8_t port_id;
	uint8_t queue_id;
} __rte_cache_aligned;

struct lcore_conf {
	unsigned lcore_id;
	
	/** Count of rx port for this lcore, hold by rx_port_list */
	unsigned n_rx_port;
	/** Count of rx queue for this lcore, hold by rx_queue_list */
	uint16_t n_rx_queue;
	/** Rx ports list for this lcore */
	unsigned rx_port_list[RTE_MAX_ETHPORTS];
	/** Rx queues list for this lcore */
	struct lcore_rx_queue rx_queue_list[MAX_RX_QUEUE_PER_LCORE];

	/** Count of tx port for this lcore, hold by tx_port_list */
	uint16_t n_tx_port;
	/** Count of tx queue for this lcore, hold by tx_queue_list */
	uint16_t n_tx_queue;
	/** Tx ports list for this lcore */
	uint16_t tx_port_id[RTE_MAX_ETHPORTS];
	/** Tx queues list for this lcore */
	uint16_t tx_queue_id[MAX_TX_QUEUE_PER_PORT];

	/* Tx buffers. */
	struct mbuf_table tx_mbufs[RTE_MAX_ETHPORTS];

	void *ipv4_lookup_struct;
	void *ipv6_lookup_struct;

	void *tv;
	void *dtv;
	void *pq;
} __rte_cache_aligned;

typedef union {
	struct {
		uint16_t domain; 
		uint8_t bus; 
		uint8_t slot: 5; 
		uint8_t function: 3;
	};
	uint32_t as_u32;
} __attribute__ ((packed)) vlib_pci_addr_t;

typedef struct {
	/* /sys/bus/pci/devices/... directory name for this device. */
	uint8_t *dev_dir_name;

	/* Resource file descriptors. */
	int *resource_fds;

	/* File descriptor for config space read/write. */
	int config_fd;

	/* File descriptor for /dev/uio%d */
	int uio_fd;

	/* Minor device for uio device. */
	uint32_t uio_minor;

	/* Index given by unix_file_add. */
	uint32_t unix_file_index;

} linux_pci_device_t;

#define foreach_dpdk_device_config_item \
	_ (num_rx_queues) \
	_ (num_tx_queues) \
	_ (num_rx_desc) \
	_ (num_tx_desc) \
	_ (rss_fn)

typedef struct
{
	/* How to match with panel ID ??? */
	uint32_t global_id;

	/* Enabled or disabled by configurations */
	uint8_t enable;

	struct ether_addr eth_addr;
	
	/* PCI address */
	vlib_pci_addr_t pci_addr;

} dpdk_dev_t;

typedef struct {

	/* caculate checksum when a tcp or udp comes. */
	uint8_t enable_tcp_udp_checksum;

	/* maximum of bytes of a frame can be. */
	uint8_t	jumbo_frame_size;

	/* strip vlan */
	uint8_t vlan_strip_offload;

	/* mask of working logical cores */
	uint32_t coremask;

	/* mask of enabled ports. */
	uint32_t portmask;

	/* number of channels. */
	uint32_t nr_channels;

	/* number of buffer elements. */
	uint32_t nr_mbufs;

	/* */
	uint32_t mempool_priv_size;

#define DPDK_DEVICE_VLAN_STRIP_DEFAULT 0
#define DPDK_DEVICE_VLAN_STRIP_OFF 1
#define DPDK_DEVICE_VLAN_STRIP_ON  2
#define _(x) uword x;
	foreach_dpdk_device_config_item
#undef _
} dpdk_config_main_t;

typedef struct {
	/* control interval of dpdk link state and stat polling */
	f64 link_state_poll_interval;
	
	f64 stat_poll_interval;
	
	/* Sleep for this many usec after each device poll */
	uint32_t poll_sleep_usec;

	dpdk_config_main_t *conf;
	
	/* mempool */
	struct rte_mempool *pktmbuf_pool[NB_SOCKETS];

	/* Tx buffer. */
	struct rte_eth_dev_tx_buffer *tx_buffer[RTE_MAX_ETHPORTS];

	/* DPDK NIC. */
	dpdk_dev_t devs[RTE_MAX_ETHPORTS];

} dpdk_main_t;

struct lcore_params {
	uint8_t port_id;
	uint8_t queue_id;
	uint8_t lcore_id;
} __rte_cache_aligned;

extern dpdk_main_t dpdk_main;
extern struct lcore_conf lcore_conf[RTE_MAX_LCORE];
/* A tsc-based timer responsible for triggering statistics printout */
extern uint64_t timer_period;


#endif
