/*!
 * @file oryx_ipc.h
 * @date 2017/08/29
 *
 * TSIHANG (haechime@gmail.com)
 */

#ifndef __ORYX_IPC_H__
#define __ORYX_IPC_H__

#include "ipc.h"

ORYX_DECLARE (
	void oryx_sys_mutex_create (
		OUT sys_mutex_t *mtx
	)
);

ORYX_DECLARE (
	int oryx_sys_mutex_lock (
		IN sys_mutex_t *mtx
	)
);

ORYX_DECLARE (
	int oryx_sys_mutex_unlock (
		IN sys_mutex_t *mtx
	)
);

ORYX_DECLARE (
	int oryx_sys_mutex_trylock (
		IN sys_mutex_t *mtx
	)
);

ORYX_DECLARE (
	int oryx_sys_mutex_timedlock (
		IN sys_mutex_t * mtx,
		IN long abs_sec,
		IN long abs_nsec
	)
);


ORYX_DECLARE (
	int oryx_sys_mutex_destroy (
		IN sys_mutex_t *mtx
	)
);

ORYX_DECLARE (
	void oryx_sys_cond_create (
		IO sys_cond_t *c
	)
);

ORYX_DECLARE (
	int oryx_sys_cond_destroy (
		IN sys_cond_t *c
	)
);

ORYX_DECLARE (
	int oryx_sys_cond_wait (
		IO sys_cond_t *c,
		IN sys_mutex_t *m
	)
);

ORYX_DECLARE (
	int oryx_sys_cond_wake (
		IO sys_cond_t *c
	)
);

ORYX_DECLARE (
	int oryx_sys_cond_broadcast (
		IO sys_cond_t *c
	)
);


#endif
