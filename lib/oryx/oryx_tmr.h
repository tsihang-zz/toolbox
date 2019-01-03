/*!
 * @file oryx_tmr.h
 * @date 2017/08/29
 *
 * TSIHANG (haechime@gmail.com)
 */

#ifndef __ORYX_TMR_H__
#define __ORYX_TMR_H__

#include "tmr.h"

ORYX_DECLARE (
	int oryx_tmr_initialize (
		void
	)
);

ORYX_DECLARE (
	void oryx_tmr_start (
		IN struct oryx_timer_t *tmr
	)
);

ORYX_DECLARE (
	void oryx_tmr_stop (
		IN struct oryx_timer_t *tmr
	)
);

ORYX_DECLARE (
	struct oryx_timer_t *oryx_tmr_create (
		IN int module,
		IN const char *sc_alias,
		IN uint32_t cfg,
		IN void (*handler)(IN struct oryx_timer_t *, IN int, IN char **),
		IN int argc,
		IN char **argv,
		IN uint32_t nr_micro_sec
	)
);

ORYX_DECLARE (
	struct oryx_timer_t *oryx_tmr_create_loop (
		IN int module,
		IN const char *sc_alias,
		IN void (*handler)(IN struct oryx_timer_t *, IN int, IN char **),
		IN int argc,
		IN char **argv,
		IN uint32_t nr_micro_sec
	)
);

ORYX_DECLARE (
	void oryx_tmr_destroy (
		IN struct oryx_timer_t *tmr
	)
);

ORYX_DECLARE (
	void oryx_tmr_default_handler (
		IN struct oryx_timer_t *tmr,
		IN int __oryx_unused__	argc,
		IN char __oryx_unused__	**argv
	)
);

#endif
