#ifndef __CONFIG_H__
#define __CONFIG_H__


#define VLIB_NETPERF_LOGFILE	"/tmp/netperf.log"
#define VLIB_SHM_BASE_KEY	0x4E455450	/* netp */

#define VLIB_QUIT		(1 << 0)
typedef struct vlib_main_t {
	//volatile uint64_t	nr__pkts;
	//volatile uint64_t	nr__size;
	//volatile uint64_t	nr_rx_pkts;	
	//volatile uint64_t	nr_rx_size;
	time_t		start_time;
	uint32_t	ul_flags;
} vlib_main_t;

#define netperf_is_quit(vm)\
	(((vm)->ul_flags & VLIB_QUIT))



#endif

