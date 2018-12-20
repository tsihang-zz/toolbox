#ifndef UNIX_DOMAIN_CONFIG_H
#define UNIX_DOMAIN_CONFIG_H

#define VLIB_UNIX_DOMAIN_SHMKEY	0x1234567
#define VLIB_UNIX_DOMAIN		"/tmp/UNIX.domain"
#define VLIB_BUFSIZE			1024

extern int unix_domain_sock;
extern bool bypass;
extern int running;
extern struct oryx_task_t unix_domain_server;
extern struct oryx_task_t unix_domain_detector;
extern struct oryx_task_t unix_domain_client;

#define VLIB_UD_SERVER_STARTED		(1 << 1)
#define VLIB_UD_CLIENT_STARTED		(1 << 15)
typedef struct vlib_unix_domain_t {
	uint32_t ul_flags;
	uint64_t	tx_pkts,
				rx_pkts,
				tx_bytes,
				rx_bytes;
}vlib_unix_domain_t;

#endif
