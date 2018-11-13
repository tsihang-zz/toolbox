#ifndef ORYX_UTILS_H
#define ORYX_UTILS_H

#include "rbtree.h"
#include "list.h"
#include "lru.h"
#include "fop.h"
#include "parser.h"
#include "memory.h"
#include "memcache.h"
#include "tq.h"
#include "mpool.h"
#include "ethtool.h"
#include "mdx.h"


#define u8_tolower(c) tolower((uint8_t)(c))

static __oryx_always_inline__ 
uint32_t next_rand_ (uint32_t *p)
{
	uint32_t seed = *p;

	seed = 1103515145 * seed + 12345;
	*p = seed;
	__os_rand = *p;
	return seed;
}

static __oryx_always_inline__
unsigned long timenow_(void)
{
	struct timeval tv = {0,0};
	
	gettimeofday(&tv, NULL);

	return tv.tv_sec * 1000000 + tv.tv_usec;
}

static __oryx_always_inline__
struct tm *oryx_localtime(time_t timep, struct tm *result)
{
    return localtime_r(&timep, result);
}

/** 1-100, 1:100, ... */
static __oryx_always_inline__
int format_range (const char *line, uint32_t lim, int base, char dlm,
		uint32_t *ul_start, uint32_t *ul_end)
{
	uint32_t val_start, val_end;
	char *end;
	char *next;

	BUG_ON(ul_start == NULL || ul_end == NULL);
	(*ul_start) = (*ul_end) = 0;
	
	val_start = strtoul((const char *)line, &end, base);
	if (errno == ERANGE || end[0] != dlm || val_start > lim){
		oryx_logn("%d %c %d lim %u (%s)", errno, end[0], val_start, lim, line);
		return -EINVAL;
	}

	next = end + 1; /** skip the dlm */
	
	val_end = strtoul((const char *)next, &end, base);
	if (errno == ERANGE || val_end > lim){
		oryx_logn("%d %c %d lim %u (%s)", errno, end[0], val_end, lim, next);
		return -EINVAL;
	}

	*ul_start = val_start;
	*ul_end = val_end;

	return 0;
}


#endif
