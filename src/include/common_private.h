#ifndef COMMON_PRIVATE_H
#define COMMON_PRIVATE_H

#define CLI_UNDEF_VAL	"---"

/* Port is just a uint16_t */
typedef uint16_t Port;
#define SET_PORT(v, p) ((p) = (v))
#define COPY_PORT(a,b) ((b) = (a))
#define CMP_PORT(p1, p2) \
    ((p1) == (p2))

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

/** This also means the priotity.  */
#define foreach_criteria\
  _(DROP, 0, "Drop if hit.") \
  _(PASS, 1, "Forward current frame to output path if hit.")\
  _(TIMESTAMP, 2, "Enable or disable tag a frame with timestamp.")\
  _(SLICING, 3, "Enable or disable frame slicing.")				\
  _(DESENSITIZATION, 4, "Enable or disable packet desensitization.")	\
  _(DEDUPLICATION, 5, "Enable or disable packet duplication checking.")\
  _(RESV, 6, "Resv.")
  

enum criteria_flags_t {
#define _(f,o,s) CRITERIA_FLAGS_##f = (1<<o),
	foreach_criteria
#undef _
	CRITERIA_FLAGS,
};

struct stats_trunk_t {
	u64 bytes;
	u64 packets;
};

typedef struct vlib_main_t
{
	int argc;
	char **argv;
	/* Name for e.g. syslog. */
	const char *prgname;
	int log_level;
	char *log_path;

	void (*sighandler)(int signum);
	
	/** equal with sizeof (Packet) */
	u32 extra_priv_size;

#define VLIB_DPDK_EAL_INITIALIZED		(1 << 0)
#define VLIB_DPDK_MEMPOOL_INITIALIZED	(1 << 1)
#define VLIB_PORT_INITIALIZED			(1 << 2)
#define VLIB_MAP_INITIALIZED			(1 << 3)
#define VLIB_UDP_INITIALIZED			(1 << 4)
#define VLIB_APP_INITIALIZED			(1 << 5)
#define VLIB_DP_INITIALIZED				(1 << 6)
#define VLIB_QUIT						(1 << 31)
	u32 ul_flags;

	struct oryx_timer_t *perf_tmr;;
	int (*dp_running_fn)(void *);
	int (*dp_terminal_fn)(void *);
	volatile bool force_quit;

	int max_lcores;

} vlib_main_t;

//extern vlib_main_t vlib_main;

#define CONFIG_PATH	"conf"
#define CONFIG_PATH_YAML CONFIG_PATH"/settings.yaml"
#define ET1500_N_XE_PORTS (2 + 1)
#define ET1500_N_GE_PORTS 8
#define MAX_PORTS (ET1500_N_XE_PORTS + ET1500_N_GE_PORTS)
#define MAX_LCORES	4


#endif

