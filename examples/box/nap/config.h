#ifndef __BOX_CONFIG_H__
#define __BOX_CONFIG_H__

#define VLIB_MAX_LCORES		4
#define VLIB_SOCKETS        8

enum {
	QUA_RX,
	QUA_TX,
	QUA_RXTX,
};


/** THIS IS a FIXED structure for EM  LPM and ACL. DO NOT CHANGE. */
/* BIG ENDIAN */
struct ipv4_5tuple {
	uint32_t ip_dst;
	uint32_t ip_src;
	uint16_t port_dst;
	uint16_t port_src;
	uint8_t  proto;
	union {
		uint16_t  rx_port_id: 4;
		uint16_t  vlan: 12;
		uint16_t  pad0;
	}u;
} __attribute__((__packed__));

#define IPv6_ADDR_LEN 16
struct ipv6_5tuple {
	uint8_t  ip_dst[IPv6_ADDR_LEN];
	uint8_t  ip_src[IPv6_ADDR_LEN];
	uint16_t port_dst;
	uint16_t port_src;
	uint8_t  proto;
	union {
		uint16_t  rx_port_id: 4;
		uint16_t  vlan: 12;
		uint16_t  pad0;
	}u;
} __attribute__((__packed__));

struct acl_route {
	union {
		struct ipv4_5tuple		k4;
		struct ipv6_5tuple		k6;
	}u;
	uint32_t					ip_src_mask: 8;
	uint32_t					ip_dst_mask: 8;
	uint32_t					ip_next_proto_mask: 8;
	uint32_t					pad0: 8;
	uint32_t					port_src_mask: 16;
	uint32_t					port_dst_mask: 16;
	uint32_t					appid;
	uint32_t					prio;
	uint32_t					map_mask;/** map_mask is a traffic direction
										  *  which has a set of in_ports and out_ports.
										  */
};


typedef struct vlib_main_t {
	int					argc;
	char				**argv;
	const char			*progname;			/* Name for e.g. syslog. */
	int					log_level;
	char				*log_path;
	void 				(*sighandler)(int signum);

#define VLIB_DPDK_EAL_INITIALIZED		(1 << 0)
#define VLIB_DPDK_MEMPOOL_INITIALIZED	(1 << 1)
#define VLIB_PORT_INITIALIZED			(1 << 2)
#define VLIB_MAP_INITIALIZED			(1 << 3)
#define VLIB_UDP_INITIALIZED			(1 << 4)
#define VLIB_APP_INITIALIZED			(1 << 5)
#define VLIB_MPM_INITIALIZED			(1 << 6)
#define VLIB_DP_INITIALIZED				(1 << 10)
#define VLIB_DP_SYNC					(1 << 11)
#define VLIB_DP_SYNC_ACL				(1 << 12)
#define VLIB_QUIT						(1 << 31)
	volatile uint32_t	ul_flags;

/** lcore0 is not used as a work core. */
#define VLIB_ALL_WORK_CORES				((1 << 1) | (1 << 2) | (1 << 3))

	volatile uint32_t	ul_core_mask;
	int					(*dp_running_fn)(void *);
	int					(*dp_terminal_fn)(void *);
	volatile bool		force_quit;

	int					nb_lcores;
	uint8_t nr_ports,
			nr_sys_ports;

	struct oryx_timer_t	*perf_tmr;
	
} vlib_main_t;

extern vlib_main_t vlib_main;

/* All lcores will set its own bit on vm->ul_core_mask.
 * */
static __oryx_always_inline__
void lock_lcores
(
	IN vlib_main_t *vm
)
{
	vm->ul_flags |= VLIB_DP_SYNC;
	while(vm->ul_core_mask != VLIB_ALL_WORK_CORES);
	oryx_logn("locked lcores %08x", vm->ul_core_mask);
}

static __oryx_always_inline__
void unlock_lcores
(
	IN vlib_main_t *vm
)
{
	vm->ul_flags &= ~VLIB_DP_SYNC;
	while(vm->ul_core_mask != 0);
	oryx_logn("unlocked lcores %08x", vm->ul_core_mask);
}


#define CONFIG_PATH	"/usr/local/etc/nap"
#define CONFIG_PATH_YAML CONFIG_PATH"/settings.yaml"
#define	SW_CPU_XAUI_PORT_ID	(0)
#define ET1500_N_XE_PORTS (2 + 1)
#define ET1500_N_GE_PORTS 8
#define MAX_PORTS (ET1500_N_XE_PORTS + ET1500_N_GE_PORTS)
/* SW port */
#define SW_PORT_OFFSET	(ET1500_N_XE_PORTS - 1)

#endif	/* endof CONFIG_H */
