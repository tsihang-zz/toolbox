#ifndef ORYX_COUNTERS_H
#define ORYX_COUNTERS_H

#include "counter.h"

void oryx_counter_init(void);
counter_id oryx_register_counter(const char *name,
							 struct CounterCtx *ctx);

#endif