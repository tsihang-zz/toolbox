#ifndef ORYX_IPC_H
#define ORYX_IPC_H

#include "ipc.h"

ORYX_DECLARE (
	oryx_status_t oryx_thread_mutex_create(oryx_thread_mutex_t **m));
ORYX_DECLARE (
	oryx_status_t oryx_thread_cond_create(oryx_thread_cond_t **c));
ORYX_DECLARE (
	oryx_status_t oryx_thread_mutex_destroy(oryx_thread_mutex_t *m));



#endif
