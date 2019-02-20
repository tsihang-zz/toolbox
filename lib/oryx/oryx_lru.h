/*!
 * @file oryx_lru.h
 * @date 2017/08/29
 *
 * TSIHANG (haechime@gmail.com)
 */


#ifndef __ORYX_LRU_H__
#define __ORYX_LRU_H__

#include "lru.h"

ORYX_DECLARE (
	void oryx_lc_new (
		IN const char *lc_name,
		IN size_t lc_max_elements,
		IN uint32_t lc_cfg,
		OUT void ** lc
	)
);

#endif
