#ifndef CLASSIFY_CONFIG_H
#define CLASSIFY_CONFIG_H

//#define HAVE_CLASSIFY_HOME
#define HAVE_SANTA_CPY2

//#define VLIB_ATOMIC_COUNTER
#define VLIB_MAX_LQ_NUM		8
#define VLIB_MAX_MME_NUM	1024

#define VLIB_ENQUEUE_HANDLER_EXITED		(1 << 0)
#define VLIB_ENQUEUE_HANDLER_STARTED	(1 << 1)
#define	VLIB_INIT_DONE					(1 << 2)

#define	VLIB_FIFO_NAME			"/root/classify_home/mme_fifo"
#define VLIB_BUFSIZE			4096

#if defined(VLIB_ATOMIC_COUNTER)
#define ATOMIC64_INC(v)	atomic64_inc((v))
#define ATOMIC64_ADD(v,i) atomic64_add((v), (i))
#define ATOMIC64_READ(v)	atomic64_read((v))
#else
#define ATOMIC64_INC(v)	((*(v)) += 1)
#define ATOMIC64_ADD(v,i)((*(v)) += i)
#define ATOMIC64_READ(v)	(*(v))
#endif

#define MME_CSV_PREFIX	"DataExport.s1mme"
#define MME_CSV_HEADER \
	",,Event Start,Event Stop,Event Type,IMSI,IMEI,,,,,,,,eCell ID,,,,,,,,,,,,,,,,,,,,,,,,,,MME UE S1AP ID,eNodeB UE S1AP ID,eNodeB CP IP Address\n"

#define ENQUEUE_LCORE_ID 0
#define DEQUEUE_LCORE_ID 1

#if defined(HAVE_TM_DEFINED) 
struct tm {
          int tm_sec;       /* 秒 – 取值区间为[0,59] */
          int tm_min;       /* 分 - 取值区间为[0,59] */
          int tm_hour;      /* 时 - 取值区间为[0,23] */
          int tm_mday;      /* 一个月中的日期 - 取值区间为[1,31] */
          int tm_mon;       /* 月份（从一月开始，0代表一月） - 取值区间为[0,11] */
          int tm_year;      /* 年份，其值等于实际年份减去1900 */
          int tm_wday;      /* 星期 – 取值区间为[0,6]，其中0代表星期天，1代表星期一，以此类推 */
          int tm_yday;      /* 从每年的1月1日开始的天数 – 取值区间为[0,365]，其中0代表1月1日，1代表1月2日，以此类推 */
          int tm_isdst;     /* 夏令时标识符，实行夏令时的时候，tm_isdst为正。不实行夏令时的进候，tm_isdst为0；不了解情况时，tm_isdst()为负。*/
};
#endif

typedef struct vlib_tm_grid_t {
	time_t start;
	time_t end;
	int block;
}vlib_tm_grid_t;

#define LQE_HAVE_IMSI	(1 << 0)
struct lq_element_t {
#define lqe_valen	256
	struct lq_prefix_t		lp;
	/* used to find an unique MME. */
	char					mme_ip[32],
							value[lqe_valen];
	size_t					iplen,
							valen,
							rawlen;

	time_t					tv_sec;

	uint32_t				ul_flags;
	void					*mme;
	vlib_tm_grid_t			vtg;
};

#define LQE_VALUE(lqe) \
	((lqe)->value)
#define LQE_VALEN(lqe) \
	((lqe)->valen)
	
#define lq_element_size	(sizeof(struct lq_element_t))

#define VLIB_MAIN_SHMKEY	0x1234567
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

	struct oryx_hashtab_t	*mme_htable;

	enum {RR, HASH}dispatch_mode;

	uint64_t nr_rx_entries;
	uint64_t nr_rx_entries_without_imsi;
	uint64_t nr_rx_entries_undispatched;
	uint64_t nr_rx_entries_dispatched;

	uint64_t nr_rx_files;
	uint64_t nr_cost_usec;

	uint64_t nr_thread_eq_ticks;
	uint64_t nr_thread_dq_ticks;

	uint64_t	tx_pkts,
			rx_pkts,
			tx_bytes,
			rx_bytes;


} vlib_main_t;

typedef struct vlib_threadvar_t {
	struct oryx_lq_ctx_t *lq;
	uint32_t			unique_id;
	uint64_t			nr_unclassified_refcnt;
} vlib_threadvar_t;

/* find a tm grid with a gaven tv_sec_now.
 * the tm grid is based on interval */
static __oryx_always_inline__
int calc_tm_grid(vlib_tm_grid_t *vtg, int interval, const time_t tv_sec)
{
	int nr_blocks = (24 * 60) / interval;
	const int block_size = interval * 60;
	struct tm	tm,
				tm0 = {
					.tm_sec	=	0,
					.tm_min	=	0,
					.tm_hour=	0,
					.tm_mday=	0,
					.tm_mon	=	0,
					.tm_year=	0,
					.tm_wday=	0,
					.tm_yday=	0,
					.tm_isdst=	0,};
	
	time_t	t0 = 0,
			ts = 0,
			tv = tv_sec;
	
	/* reset VTG to -1 */
	vtg->start = vtg->end = ~0;
	
	localtime_r(&tv_sec, &tm);

	tm0.tm_year	=	tm.tm_year;
	tm0.tm_mon	=	tm.tm_mon;
	tm0.tm_mday	=	tm.tm_mday;
	t0 = mktime(&tm0);

	ts = t0;
	tv -= ts;
	nr_blocks = tv / block_size;
	
	vtg->start = ts + block_size * nr_blocks;
	vtg->end   = vtg->start + block_size;
	vtg->block = nr_blocks;
	
	return 0;
}

static __oryx_always_inline__
struct lq_element_t *lqe_alloc(void)
{
	const size_t lqe_size = lq_element_size;
	struct lq_element_t *lqe;
	
	lqe = malloc(lqe_size);
	if(lqe) {
		lqe->lp.lnext = lqe->lp.lprev = NULL;
		lqe->valen = 0;
		lqe->ul_flags = 0;
		lqe->vtg.start = lqe->vtg.end = 0;
		memset((void *)&lqe->mme_ip[0], 0, 32);
		memset((void *)&lqe->value[0], 0, lqe_valen);
	}
	return lqe;
}

extern int running;
extern uint32_t	epoch_time_sec;
extern vlib_main_t vlib_main;
extern vlib_threadvar_t vlib_tv_main[];
extern char *inotify_home;
extern const char *classify_home;
extern char inotify_file[];

#include "fmgr.h"
#include "mme.h"
#include "classify.h"


#endif

