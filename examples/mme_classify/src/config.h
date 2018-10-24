#ifndef CLASSIFY_CONFIG_H
#define CLASSIFY_CONFIG_H

#define VLIB_MAX_LQ_NUM	8

#define ENQUEUE_LCORE_ID 0
#define DEQUEUE_LCORE_ID 1

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
	uint64_t nr_cost_usec;

	uint64_t nr_thread_eq_ticks;
	uint64_t nr_thread_dq_ticks;

} vlib_main_t;
extern vlib_main_t vlib_main;

typedef struct vlib_threadvar_t {
	struct oryx_lq_ctx_t *lq;
	uint32_t			unique_id;
	uint64_t			nr_unclassified_refcnt;
} vlib_threadvar_t;
extern vlib_threadvar_t vlib_tv_main[];

static __oryx_always_inline__
vlib_threadvar_t *vlib_alloc_tv(void)
{
	vlib_main_t *vm = &vlib_main;
	return &vlib_tv_main[vm->nr_mmes % vm->nr_threads];
}

extern int running;

#define MME_CSV_PREFIX	"DataExport.s1mme"
#define MME_CSV_HEADER \
	",,Event Start,Event Stop,Event Type,IMSI,IMEI,,,,,,,,eCell ID,,,,,,,,,,,,,,,,,,,,,,,,,,MME UE S1AP ID,eNodeB UE S1AP ID,eNodeB CP IP Address\n"


#include "fmgr.h"
#include "tg.h"
#include "mme.h"
#include "lqe.h"


#endif

