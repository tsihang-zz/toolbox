#ifndef CONFIG_H
#define CONFIG_H

#define VLIB_SHM_BASE_KEY	0x4E434150	/* NCAP */

#define VLIB_MBUF_PER_NCAPDC 256*1024
#define VLIB_MBUFS_PER_PORT 1024
#define VLIB_MBUF_CACHE_SIZE 512

#define VLIB_MBUF_OVERHEAD (sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM)
#define VLIB_MBUF_DATA_SIZE 4096
#define VLIB_MBUF_SIZE (VLIB_MBUF_DATA_SIZE + VLIB_MBUF_OVERHEAD)

/* allow max jumbo frame 9 KB */
#define VLIB_JUMBO_FRAME_SIZE	0x2400

/* define common names for structures shared between server and client */
#define VLIB_NCAPDC_QUEUE_NAME		"VLIB_NCAPDC%u_RX_QUEUE"
#define VLIB_MBUF_POOL_NAME			"VLIB_MBUF_POOL"
#define VLIB_MAIN_SHM_ZONE			"VLIB_MAIN"

#define VLIB_DEFAULT_RX_DESC 512
#define VLIB_DEFAULT_TX_DESC 512
#define VLIB_RING_SIZE 512*1024

#define VLIB_NCAPDC_NUM		RTE_MAX_ETHPORTS
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
#define VLIB_DEFAULT_PTHRESH 36
#define VLIB_DEFAULT_RX_PTHRESH 8
#define VLIB_DEFAULT_TX_PTHRESH 0
#define VLIB_DEFAULT_WTHRESH 0

/*
 * When doing reads from the NIC or the client queues,
 * use this batch size
 */
#define VLIB_RX_BURST_SIZE 32

/*
 * Local buffers to put packets in, used to send packets in bursts to the
 * clients
 */
struct vlib_rx_buffer_t {
	struct rte_mbuf *buffer[VLIB_RX_BURST_SIZE];
	uint16_t count;
};


/*
 * Shared port info, including statistics information for display by server.
 * Structure will be put in a memzone.
 * - All port id values share one cache line as this data will be read-only
 * during operation.
 * - All rx statistic values share cache lines, as this data is written only
 * by the server process. (rare reads by stats display)
 * - The tx statistics have values for all ports per cache line, but the stats
 * themselves are written by the clients, so we have a distinct set, on different
 * cache lines for each client to use.
 */
struct vlib_rx_stats {
	uint64_t	rx,
				rxb;
} __rte_cache_aligned;

struct vlib_tx_stats {
	uint64_t tx[RTE_MAX_ETHPORTS];
	uint64_t txb[RTE_MAX_ETHPORTS];
	uint64_t fw_drop[RTE_MAX_ETHPORTS];
	uint64_t tx_drop[RTE_MAX_ETHPORTS];
	uint64_t tx_arp_resp[RTE_MAX_ETHPORTS];
	uint64_t free_arp[RTE_MAX_ETHPORTS];
} __rte_cache_aligned;

typedef struct vlib_queue_t {
	struct rte_ring *rx_q;
	unsigned id;
	/* these stats hold how many packets the client will actually receive,
	 * and how many packets were dropped because the client's queue was full.
	 * The port-info stats, in contrast, record how many packets were received
	 * or transmitted on an actual NIC port.
	 */
	struct {
		volatile uint64_t rx;
		volatile uint64_t rx_drop;
	} stats;
} vlib_queue_t;
vlib_queue_t	*vlib_queues;	/* array of info/queues for clients */

#define NCPAD_SERV_INIT_DONE	(1 << 0)
typedef struct vlib_main_t {
	uint8_t nr_ports,
			nr_sys_ports,
			id[RTE_MAX_ETHPORTS];

	struct vlib_rx_stats rx[RTE_MAX_ETHPORTS];
	struct vlib_tx_stats tx[RTE_MAX_ETHPORTS];
	struct vlib_rx_stats rx0[RTE_MAX_ETHPORTS];

	uint32_t ul_flags;
	
} vlib_main_t;

extern vlib_main_t *vlib_main;

extern const char *progname;

/*
 * Given the rx queue name template above, get the queue name
 */
static const char *
ncapd_get_rx_queue_name (unsigned id)
{
	/* buffer for return value. Size calculated by %u being replaced
	 * by maximum 3 digits (plus an extra byte for safety) */
	static char buffer[sizeof(VLIB_NCAPDC_QUEUE_NAME) + 2];

	snprintf(buffer, sizeof(buffer) - 1, VLIB_NCAPDC_QUEUE_NAME, id);
	return buffer;
}


#endif
