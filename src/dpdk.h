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

#include "main.h"

/** These two macro means a frame. */
#define DPDK_BUFFER_PRE_DATA_SIZE	RTE_PKTMBUF_HEADROOM		//(128)
#define DPDK_BUFFER_DATA_SIZE		RTE_MBUF_DEFAULT_DATAROOM	//(2048)

/** 
 * default macros and can be overwrite by settings.yaml. 
 */
#define	DPDK_DEFAULT_BUFFER_SIZE	\
	(DPDK_BUFFER_DATA_SIZE + DPDK_BUFFER_PRE_DATA_SIZE)

#define DPDK_DEFAULT_NB_MBUF   (16 << 10)
#define DPDK_DEFAULT_CACHE_SIZE	256
#define DPDK_STATS_POLL_INTERVAL      (10.0)
#define DPDK_MIN_STATS_POLL_INTERVAL  (0.001)	/* 1msec */
#define DPDK_LINK_POLL_INTERVAL       (3.0)
#define DPDK_MIN_LINK_POLL_INTERVAL   (0.001)	/* 1msec */

/** DO not change DEFAULT_HUGE_DIR. see dpdk-mount-hugedir.sh in conf/ */
#define DPDK_DEFAULT_HUGE_DIR "/mnt/huge"
#define DPDK_DEFAULT_RUN_DIR "/run/et1500"

#define MAX_PORTS (ET1500_N_XE_PORTS + ET1500_N_GE_PORTS)

#define MAX_RX_QUEUE_PER_LCORE 16
#define MAX_TX_QUEUE_PER_PORT 16


typedef union {
	struct {
		u16 domain; 
		u8 bus; 
		u8 slot: 5; 
		u8 function: 3;
	};
	u32 as_u32;
} __attribute__ ((packed)) vlib_pci_addr_t;

typedef struct {
	/* /sys/bus/pci/devices/... directory name for this device. */
	u8 *dev_dir_name;

	/* Resource file descriptors. */
	int *resource_fds;

	/* File descriptor for config space read/write. */
	int config_fd;

	/* File descriptor for /dev/uio%d */
	int uio_fd;

	/* Minor device for uio device. */
	u32 uio_minor;

	/* Index given by unix_file_add. */
	u32 unix_file_index;

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
	u32 global_id;

	/* Enabled or disabled by configurations */
	u8 enable;

	/* PCI address */
	vlib_pci_addr_t pci_addr;

	/* */
	u8 vlan_strip_offload;

#define DPDK_DEVICE_VLAN_STRIP_DEFAULT 0
#define DPDK_DEVICE_VLAN_STRIP_OFF 1
#define DPDK_DEVICE_VLAN_STRIP_ON  2
#define _(x) uword x;
	foreach_dpdk_device_config_item
#undef _
} dpdk_device_config_t;

typedef struct {
	char *uio_driver_name;
	u8 no_multi_seg;
	u8 enable_tcp_udp_checksum;

	/* Required config parameters */
	u8 coremask_set_manually;
	u8 nchannels_set_manually;

	int device_config_index_by_pci_addr;

	u32 coremask;
	u32 portmask;
	u32 nchannels;
	u32 num_mbufs;
	u32 cache_size;
	u32 priv_size;
	u32 data_room_size;
	u32 n_rx_q_per_lcore;

	/*
	* format interface names ala xxxEthernet%d/%d/%d instead of
	* xxxEthernet%x/%x/%x.
	*/
	u8 interface_name_format_decimal;

	/* per-device config */
	dpdk_device_config_t default_devconf;
	dpdk_device_config_t *dev_confs;

} dpdk_config_main_t;

typedef struct {
	/* control interval of dpdk link state and stat polling */
	f64 link_state_poll_interval;

	f64 stat_poll_interval;
	
	/* Sleep for this many usec after each device poll */
	u32 poll_sleep_usec;

	dpdk_config_main_t *conf;
	
	/* mempool */
	struct rte_mempool *pktmbuf_pools;

	u32 n_lcores;
	u32 n_ports;
	u32 master_lcore;

	int (*dp_fn)(void *);
	volatile bool force_quit;
	
	vlib_main_t *vm;
} dpdk_main_t;

#include "dpdk_init.h"

extern void dpdk_init (vlib_main_t * vm);

#endif