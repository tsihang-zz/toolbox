#ifndef ORYX_TMR_H
#define ORYX_TMR_H

#include "tmr.h"

ORYX_DECLARE(oryx_status_t oryx_tmr_initialize (void));
ORYX_DECLARE(void oryx_tmr_start (struct oryx_timer_t *tmr));
ORYX_DECLARE(void oryx_tmr_stop (struct oryx_timer_t *tmr));
ORYX_DECLARE(struct oryx_timer_t *oryx_tmr_create (int module,
                const char *sc_alias, uint32_t ul_setting_flags,
                void (*handler)(struct oryx_timer_t *, int, char **), int argc, char **argv,
                uint32_t n_mseconds));
ORYX_DECLARE(struct oryx_timer_t *oryx_tmr_create_loop (int module,
				const char *sc_alias,
				void (*handler)(struct oryx_timer_t *, int, char **), int argc, char **argv,
				uint32_t n_mseconds));

ORYX_DECLARE(void oryx_tmr_destroy (struct oryx_timer_t *tmr));
ORYX_DECLARE(void oryx_tmr_default_handler(struct oryx_timer_t *tmr, int __oryx_unused_param__ argc, 
                char __oryx_unused_param__**argv));

#endif
