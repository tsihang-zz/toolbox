#ifndef ORYX_LQ_H
#define ORYX_LQ_H

#include "lq.h"

ORYX_DECLARE(
	int oryx_lq_new(const char *fq_name, uint32_t fq_cfg, void ** lq)
);
ORYX_DECLARE(
	void oryx_lq_destroy(void *fq)
);
ORYX_DECLARE(
	void oryx_lq_dump(void *fq)
);


#endif
