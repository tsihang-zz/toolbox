#ifndef ORYX_UTILS_H
#define ORYX_UTILS_H

#include "rbtree.h"
#include "list.h"
#include "lru.h"
#include "atomic.h"
#include "vec.h"
#include "fop.h"
#include "tq.h"
#include "parser.h"

#define u8_tolower(c) tolower((uint8_t)(c))

static __oryx_always_inline__ 
uint32_t next_rand_ (uint32_t *p)
{
	uint32_t seed = *p;

	seed = 1103515145 * seed + 12345;
	*p = seed;

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

#endif
