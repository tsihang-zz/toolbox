#ifndef ORYX_COUNTERS_H
#define ORYX_COUNTERS_H

#include "counter.h"

#define COUNTER_RANGE_START(ctx)	1
#define COUNTER_RANGE_END(ctx)	(atomic_read(&(ctx)->curr_id))
void
oryx_counter_init(void);

counter_id
oryx_register_counter(const char *name,
							const char *comments, struct CounterCtx *ctx);
void
oryx_release_counter(struct CounterCtx *ctx);

u64
oryx_counter_get(struct CounterCtx *ctx, counter_id id);

void
oryx_counter_add(struct CounterCtx *ctx, counter_id id, u64 x);

void
oryx_counter_inc(struct CounterCtx *ctx, counter_id id);

void
oryx_counter_set(struct CounterCtx *ctx, counter_id id, u64 x);

int
oryx_counter_get_array_range(counter_id s_id, counter_id e_id,
                                      struct CounterCtx *ctx);
#endif