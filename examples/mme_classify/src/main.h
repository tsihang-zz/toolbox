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
	int			threshold;	/* in minutes. */
	const char *dictionary;
	const char *classdir;
	const char *savdir;
	const char *inotifydir;
	
	int			max_entries_per_file;

	struct oryx_htable_t	*mme_htable;

	enum {RR, HASH}dispatch_mode;

	uint64_t nr_rx_entries;
	uint64_t nr_rx_entries_without_imsi;
	uint64_t nr_rx_entries_undispatched;
	uint64_t nr_rx_entries_dispatched;

	uint64_t nr_rx_files;

	uint64_t nr_thread_eq_ticks;
	uint64_t nr_thread_dq_ticks;

} vlib_main_t;

extern vlib_main_t vlib_main;
extern int running;

#define MME_CSV_PREFIX	"DataExport.s1mme"

#include "fmgr.h"
#include "tg.h"
#include "mme.h"
#include "classify.h"

#endif

