#ifndef __CONFIG_H__
#define __CONFIG_H__


#define VLIB_NETPERF_LOGFILE	"/tmp/netperf.log"
#define VLIB_SHM_BASE_KEY	0x4E455450	/* netp */

#define NDEV_MAX	12
typedef struct vlib_ndev_t {
	char ethname[32];
	int  sock;

	uint64_t	nr_rx_pkts;
	uint64_t	nr_tx_pkts;
	uint64_t	nr_rx_bytes;
	uint64_t	nr_tx_bytes;
} vlib_ndev_t;

#define VLIB_QUIT				(1 << 0)
#define VLIB_STATS_TMR_RUNNING	(1 << 1)
typedef struct vlib_main_t {
	time_t		start_time;
	uint32_t	ul_flags;
	vlib_ndev_t ndev[NDEV_MAX];
	int	nr_ndevs;
} vlib_main_t;

#define netperf_is_quit(vm)\
	(((vm)->ul_flags & VLIB_QUIT))


#endif

