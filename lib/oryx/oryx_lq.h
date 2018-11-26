#ifndef ORYX_LQ_H
#define ORYX_LQ_H

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
