#ifndef MAIN_H
#define MAIN_H

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
#define VLIB_QUIT						(1 << 2)
	u32 ul_flags;
} vlib_main_t;

extern vlib_main_t vlib_main;

#define CONFIG_PATH	"conf"
#define CONFIG_PATH_YAML CONFIG_PATH"/settings.yaml"
#define ET1500_N_XE_PORTS 2
#define ET1500_N_GE_PORTS 8
#define MAX_LCORES	4

#endif

