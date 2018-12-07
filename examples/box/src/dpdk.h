#ifndef __DPDK_INC_H__
#define __DPDK_INC_H__

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
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_udp.h>
#include <rte_fbk_hash.h>
#include <rte_string_fns.h>
#include <rte_cycles.h>
#include <rte_vect.h>

#if defined(HAVE_DPDK)
#define MAX_LCORES	RTE_MAX_LCORE
#else
#define MAX_LCORES	4
#endif

#define MAX_LCORE_PARAMS 1024

#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 90 /* 9s (90 * 100ms) in total */

#define MAX_JUMBO_PKT_LEN  9600
#define MEMPOOL_CACHE_SIZE 256


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

#define VLIB_SOCKETS        8

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

#define ALL_32_BITS 0xffffffff
#define BIT_8_TO_15 0x0000ff00
#define BIT_16_TO_23 0x00ff0000

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

struct parser_ctx_t {
	const uint8_t *data_ipv4[DPDK_MAX_RX_BURST];
	struct rte_mbuf *m_ipv4[DPDK_MAX_RX_BURST];
	uint32_t res_ipv4[DPDK_MAX_RX_BURST];
	int num_ipv4;

	const uint8_t *data_ipv6[DPDK_MAX_RX_BURST];
	struct rte_mbuf *m_ipv6[DPDK_MAX_RX_BURST];
	uint32_t res_ipv6[DPDK_MAX_RX_BURST];
	int num_ipv6;

	const uint8_t *data_notip[DPDK_MAX_RX_BURST];
	struct rte_mbuf *m_notip[DPDK_MAX_RX_BURST];
	int num_notip;
};

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
	struct rte_mempool *pktmbuf_pool[VLIB_SOCKETS];

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

union ipv4_5tuple_host {
	struct {
		uint8_t  pad0;
		uint8_t  proto;
		uint16_t pad1;
		uint32_t ip_src;
		uint32_t ip_dst;
		uint16_t port_src;
		uint16_t port_dst;
	};
	xmm_t xmm;
};

extern dpdk_main_t dpdk_main;
extern struct lcore_conf lcore_conf[RTE_MAX_LCORE];
/* A tsc-based timer responsible for triggering statistics printout */
extern uint64_t timer_period;
extern struct rte_eth_conf dpdk_eth_default_conf;
extern struct rte_mempool * pktmbuf_pool[];
extern rte_xmm_t mask0;
extern rte_xmm_t mask1;
extern rte_xmm_t mask2;

#if defined(__aarch64__)
static __oryx_always_inline__
xmm_t em_mask_key(void *key, xmm_t mask)
{
	int32x4_t data = vld1q_s32((int32_t *)key);

	return vandq_s32(data, mask);
}
#endif

#if defined(__x86_64__)
static __oryx_always_inline__
xmm_t em_mask_key(void *key, xmm_t mask)
{
	__m128i data = _mm_loadu_si128((__m128i *)(key));
	return _mm_and_si128(data, mask);
}
#endif

#endif

