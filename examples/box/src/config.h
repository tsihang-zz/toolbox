#ifndef CONFIG_H
#define CONFIG_H

enum {
	QUA_RX,
	QUA_TX,
	QUA_RXTX,
};

typedef struct vlib_iface_main_t {
	int						ul_n_ports;	/* dpdk_ports + sw_ports */
	uint32_t				ul_flags;
	struct oryx_timer_t		*link_detect_tmr;
	struct oryx_timer_t		*healthy_tmr;
	uint32_t				link_detect_tmr_interval;
	uint32_t				poll_interval;
	os_mutex_t				lock;
	oryx_vector				entry_vec;
	struct oryx_htable_t	*htable;
	void					*vm;
} vlib_iface_main_t;

extern vlib_iface_main_t vlib_iface_main;


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
