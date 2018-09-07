#ifndef COMMON_PRIVATE_H
#define COMMON_PRIVATE_H

#define CLI_UNDEF_VAL	"---"

/* Port is just a uint16_t */
typedef uint16_t Port;
#define SET_PORT(v, p) ((p) = (v))
#define COPY_PORT(a,b) ((b) = (a))
#define CMP_PORT(p1, p2) \
    ((p1) == (p2))

enum {
	QUA_RX,
	QUA_TX,
	QUA_RXTX,
};

/* Address */
typedef struct Address_ {
    char family;
    union {
        uint32_t       address_un_data32[4]; /* type-specific field */
        uint16_t       address_un_data16[8]; /* type-specific field */
        uint8_t        address_un_data8[16]; /* type-specific field */
    } address;
} Address;

#define addr_data32 address.address_un_data32
#define addr_data16 address.address_un_data16
#define addr_data8  address.address_un_data8

#define COPY_ADDRESS(a, b) do {                    \
        (b)->family = (a)->family;                 \
        (b)->addr_data32[0] = (a)->addr_data32[0]; \
        (b)->addr_data32[1] = (a)->addr_data32[1]; \
        (b)->addr_data32[2] = (a)->addr_data32[2]; \
        (b)->addr_data32[3] = (a)->addr_data32[3]; \
    } while (0)
    
#define CMP_ADDR(a1, a2) \
		(((a1)->addr_data32[3] == (a2)->addr_data32[3] && \
		  (a1)->addr_data32[2] == (a2)->addr_data32[2] && \
		  (a1)->addr_data32[1] == (a2)->addr_data32[1] && \
		  (a1)->addr_data32[0] == (a2)->addr_data32[0]))

/* clear the address structure by setting all fields to 0 */
#define CLEAR_ADDR(a) do {       \
        (a)->family = 0;         \
        (a)->addr_data32[0] = 0; \
        (a)->addr_data32[1] = 0; \
        (a)->addr_data32[2] = 0; \
        (a)->addr_data32[3] = 0; \
    } while (0)


/** Quadrant */
enum {
	QUA_DROP,
	QUA_PASS,
	QUAS,
};

typedef struct vlib_main
{
	int					argc;
	char				**argv;
	const char			*prgname;			/* Name for e.g. syslog. */
	int					log_level;
	char				*log_path;
	void 				(*sighandler)(int signum);

#define VLIB_DPDK_EAL_INITIALIZED		(1 << 0)
#define VLIB_DPDK_MEMPOOL_INITIALIZED	(1 << 1)
#define VLIB_PORT_INITIALIZED			(1 << 2)
#define VLIB_MAP_INITIALIZED			(1 << 3)
#define VLIB_UDP_INITIALIZED			(1 << 4)
#define VLIB_APP_INITIALIZED			(1 << 5)
#define VLIB_DP_INITIALIZED				(1 << 6)
#define VLIB_DP_SYNC					(1 << 7)
#define VLIB_DP_SYNC_ACL				(1 << 8)
#define VLIB_QUIT						(1 << 31)
	volatile uint32_t	ul_flags;

/** lcore0 is not used as a work core. */
#define VLIB_ALL_WORK_CORES				((1 << 1) | (1 << 2) | (1 << 3))

	volatile uint32_t	ul_core_mask;
	int					(*dp_running_fn)(void *);
	int					(*dp_terminal_fn)(void *);
	volatile bool		force_quit;

	int					nb_lcores;

	struct oryx_timer_t	*perf_tmr;
	
} vlib_main_t;

/* All lcores will set its own bit on vm->ul_core_mask.
 * */
static __oryx_always_inline__
void lock_lcores(vlib_main_t *vm)
{
	vm->ul_flags |= VLIB_DP_SYNC;
	while(vm->ul_core_mask != VLIB_ALL_WORK_CORES);
	oryx_logn("locked lcores %08x", vm->ul_core_mask);
}

static __oryx_always_inline__
void unlock_lcores(vlib_main_t *vm)
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

#if defined(HAVE_DPDK)
#define MAX_LCORES	RTE_MAX_LCORE
#else
#define MAX_LCORES	4
#endif

/** Create IPv4 address */
#define IPv4(a,b,c,d) ((uint32_t)(((a) & 0xff) << 24) | \
					   (((b) & 0xff) << 16) | \
					   (((c) & 0xff) << 8)  | \
					   ((d) & 0xff))

#endif

