#ifndef ORYX_TMR_H
#define ORYX_TMR_H

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
		IN uint32_t ul_setting_flags,
		IN void (*handler)(IN struct oryx_timer_t *, IN int, IN char **),
		IN int argc,
		IN char **argv,
		IN uint32_t nr_mseconds
	)
);

ORYX_DECLARE (
	struct oryx_timer_t *oryx_tmr_create_loop (
		IN int module,
		IN const char *sc_alias,
		IN void (*handler)(IN struct oryx_timer_t *, IN int, IN char **),
		IN int argc,
		IN char **argv,
		IN uint32_t nr_mseconds
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
		IN int __oryx_unused_param__	argc,
		IN char __oryx_unused_param__	*argv
	)
);

#endif
