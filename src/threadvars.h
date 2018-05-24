
#ifndef THREADVARS_H
#define THREADVARS_H

//#include "counters.h"

/** \brief Per thread variable structure */
typedef struct ThreadVars_ {
    pthread_t t;
	u32 lcore;
    char name[16];
    char *printable_name;
    char *thread_group_name;
    SC_ATOMIC_DECLARE(unsigned int, flags);

    /** local id */
    int id;

    /* counters for this thread. */
	struct CounterCtx perf_private_ctx0;

	/** free function for this thread to free a packet. */
	int (*free_fn)(void *);
	
	struct ThreadVars_ *next;
    struct ThreadVars_ *prev;

	/** hold a simple statistics. */
	u64 n_tx_packets_prov;
	u64 n_tx_bytes_prov;
	
	u64 n_tx_packets;
	u64 n_tx_bytes;
	u64 n_rx_packets;
	u64 n_rx_bytes;

}ThreadVars;

#endif
