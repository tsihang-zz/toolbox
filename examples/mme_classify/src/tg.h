#ifndef TIME_GRID_H
#define TIME_GRID_H

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
}vlib_tm_grid_t;

/* find a tm grid with a gaven tv_sec_now.
 * the tm grid is based on interval */
static __oryx_always_inline__
int calc_tm_grid(vlib_tm_grid_t *vtg, int interval, const time_t tv_sec)
{
	int nr_blocks = (24 * 60) / interval;
	const int block_size = interval * 60;
	struct tm tm, tm0;
	time_t t0 = 0, ts = 0, tv = tv_sec;
	
	memset (&tm0, 0, sizeof(struct tm));
	
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
	
	return 0;
}

#endif
