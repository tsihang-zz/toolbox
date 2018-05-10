#ifndef ET1500_DPDK_H
#define ET1500_DPDK_H

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


#define HAVE_DPDK_SCRIPT_MOUNT	1
#define HAVE_DPDK_SCRIPT_DEVBIND	1
#define DPDK_SCRIPT_MOUNT	"dpdk-mount-hugedir.sh"
#define DPDK_SCRIPT_DEVBIND	"dpdk-devbind-uio.sh"

#define MAX_PORTS RTE_MAX_ETHPORTS

/** DO not change DEFAULT_HUGE_DIR. see dpdk-mount-hugedir.sh in conf/ */
#define DEFAULT_HUGE_DIR "/mnt/huge"
#define ET1500_RUN_DIR "/run/et1500"

#define _PACKED(x)	x __attribute__ ((packed))
#define NB_MBUF   (16<<10)

typedef union {
	struct {
		u16 domain; 
		u8 bus; 
		u8 slot: 5; 
		u8 function: 3;
	};
	u32 as_u32;
} __attribute__ ((packed)) vlib_pci_addr_t;


typedef struct vlib_pci_device {
  /* Operating system handle for this device. */
  uword os_handle;

  vlib_pci_addr_t bus_address;

#if 0
  /* First 64 bytes of configuration space. */
  union
  {
    pci_config_type0_regs_t config0;
    pci_config_type1_regs_t config1;
    u8 config_data[256];
  };
#endif

  /* Interrupt handler */
  void (*interrupt_handler) (struct vlib_pci_device * dev);

  /* Driver name */
  u8 *driver_name;

  /* Numa Node */
  int numa_node;

  /* Device data */
  u16 device_class;
  u16 vendor_id;
  u16 device_id;

  /* Vital Product Data */
  u8 *product_name;
  u8 *vpd_r;
  u8 *vpd_w;

  /* Private data */
  uword private_data;

} vlib_pci_device_t;

typedef struct
{
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


typedef struct
{
  vlib_main_t *vlib_main;
  vlib_pci_device_t *pci_devs;
  //pci_device_registration_t *pci_device_registrations;
  uword *pci_dev_index_by_pci_addr;
} vlib_pci_main_t;


#define foreach_dpdk_device_config_item \
	_ (num_rx_queues) \
	_ (num_tx_queues) \
	_ (num_rx_desc) \
	_ (num_tx_desc) \
	_ (rss_fn)

typedef struct
{
	u8 enable;
	vlib_pci_addr_t pci_addr;
	u8 is_blacklisted;
	u8 vlan_strip_offload;

#define DPDK_DEVICE_VLAN_STRIP_DEFAULT 0
#define DPDK_DEVICE_VLAN_STRIP_OFF 1
#define DPDK_DEVICE_VLAN_STRIP_ON  2
#define _(x) uword x;
	foreach_dpdk_device_config_item
#undef _
	//clib_bitmap_t * workers;
	//u32 hqos_enabled;
	//dpdk_device_config_hqos_t hqos;
} dpdk_device_config_t;

typedef struct
{
	/* Config stuff */
	u8 **eal_init_args;
	u8 *eal_init_args_str;
	u8 *uio_driver_name;
	u8 no_multi_seg;
	u8 enable_tcp_udp_checksum;

	/* Required config parameters */
	u8 coremask_set_manually;
	u8 nchannels_set_manually;
	u32 coremask;
	u32 nchannels;
	u32 num_mbufs;

	/*
	* format interface names ala xxxEthernet%d/%d/%d instead of
	* xxxEthernet%x/%x/%x.
	*/
	u8 interface_name_format_decimal;

	/* per-device config */
	dpdk_device_config_t default_devconf;
	dpdk_device_config_t *dev_confs;
	uword device_config_index_by_pci_addr;

} dpdk_config_main_t;
dpdk_config_main_t dpdk_config_main;


typedef struct
{
#define DPDK_STATS_POLL_INTERVAL      (10.0)
#define DPDK_MIN_STATS_POLL_INTERVAL  (0.001)	/* 1msec */
	
#define DPDK_LINK_POLL_INTERVAL       (3.0)
#define DPDK_MIN_LINK_POLL_INTERVAL   (0.001)	/* 1msec */

	/* Devices */
	//dpdk_device_t *devices;
	//dpdk_device_and_queue_t **devices_by_hqos_cpu;
	/* control interval of dpdk link state and stat polling */
	f64 link_state_poll_interval;
	f64 stat_poll_interval;
	
	/* Sleep for this many usec after each device poll */
	u32 poll_sleep_usec;

	dpdk_config_main_t *conf;
	/* mempool */
	struct rte_mempool **pktmbuf_pools;

	/* API message ID base */
	u16 msg_id_base;

	vlib_main_t *vlib_main;
} dpdk_main_t;

extern void dpdk_init (vlib_main_t * vm);

#endif
