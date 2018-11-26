#ifndef ORYX_IPC_H
#define ORYX_IPC_H

#include "ipc.h"

ORYX_DECLARE (
	int oryx_tm_create (
		OUT os_mutex_t **m
	)
);

ORYX_DECLARE (
	int oryx_tm_destroy (
		IN os_mutex_t *m
	)
);

ORYX_DECLARE (
	int oryx_tc_create (
		OUT os_cond_t **c
	)
);

#endif
