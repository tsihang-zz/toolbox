/*!
 * @file oryx_lq.h
 * @date 2017/08/29
 *
 * TSIHANG (haechime@gmail.com)
 */

#ifndef __ORYX_LQ_H__
#define __ORYX_LQ_H__

#include "lq.h"

ORYX_DECLARE (
	int oryx_lq_new (
		IN const char *fq_name,
		IN uint32_t fq_cfg,
		OUT void ** lq
	)
);

ORYX_DECLARE (
	void oryx_lq_destroy (
		IN void * lq
	)
);

ORYX_DECLARE (
	void oryx_lq_dump (
		IN void * lq
	)
);

#endif
