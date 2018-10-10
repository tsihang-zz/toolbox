#ifndef MAIN_H
#define MAIN_H

#define VLIB_ENQUEUE_HANDLER_EXITED		(1 << 0)
#define VLIB_ENQUEUE_HANDLER_STARTED	(1 << 1)

typedef struct vlib_main_t {
	int			argc;
	char			**argv;
	const char		*prgname; /* Name for e.g. syslog. */
	int			nr_mmes;
	
	volatile uint32_t	ul_flags;

	
	int			nr_threads;
	int			classify_threshold;	/* in minutes. */
	const char *mme_dictionary;
	const char *classify_warehouse;
	int			max_entries_per_file;

	struct oryx_htable_t	*mme_htable;

	uint64_t nr_rx_entries;
	uint64_t nr_rx_entries_without_imsi;
	uint64_t nr_rx_entries_dispatched;

} vlib_main_t;

extern vlib_main_t vlib_main;
extern int running;

#endif

