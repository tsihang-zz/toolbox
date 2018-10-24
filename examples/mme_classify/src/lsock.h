#ifndef CFG_H
#define CFG_H

#define UNIX_DOMAIN "/tmp/UNIX.domain"
#define bufsize		1024

extern uint64_t rx_pkts, rx_bytes, tx_pkts, tx_bytes;

extern struct oryx_task_t lclient;
extern struct oryx_task_t lserver;

#endif