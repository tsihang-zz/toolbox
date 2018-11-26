
#ifndef THREADVARS_H
#define THREADVARS_H

/** \brief Per thread variable structure */
typedef struct ThreadVars_ {
    pthread_t t;
	uint32_t lcore;
    char name[16];
    char *printable_name;
    char *thread_group_name;
    atomic_declare(unsigned int, flags);

    /** local id */
    int id;

    /* counters for this thread. */
	struct oryx_counter_ctx_t perf_private_ctx0;

	/** free function for this thread to free a packet. */
	int (*free_fn)(void *);
	
	struct ThreadVars_ *next;
    struct ThreadVars_ *prev;

	uint64_t nr_mbufs_refcnt;
	uint64_t nr_mbufs_feedback;

	/* cur_tsc@lcore. */
	uint64_t	cur_tsc;
}threadvar_ctx_t;

#endif
