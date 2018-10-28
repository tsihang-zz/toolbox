#ifndef UNIX_DOMAIN_CONFIG_H
#define UNIX_DOMAIN_CONFIG_H

#define VLIB_UNIX_DOMAIN		"/tmp/UNIX.domain"
#define VLIB_BUFSIZE			1024

extern int running;
extern struct oryx_task_t unix_domain_server;
extern struct oryx_task_t unix_domain_client;

#define VLIB_UD_SERVER_STARTED	(1 << 0)
typedef struct vlib_main_t{
	uint32_t ul_flags;
	uint64_t	tx_pkts,
				rx_pkts,
				tx_bytes,
				rx_bytes;
}vlib_main_t;

#define VLIB_MAIN_SHM_KEY	0x1234567

#endif
