#ifndef ORYX_IPC_H
#define ORYX_IPC_H

#include "ipc.h"

ORYX_DECLARE (
	int oryx_thread_mutex_create(os_mutex_t **m));
ORYX_DECLARE (
	int oryx_thread_cond_create(os_cond_t **c));
ORYX_DECLARE (
	int oryx_thread_mutex_destroy(os_mutex_t *m));



#endif
