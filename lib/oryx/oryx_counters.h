#ifndef ORYX_COUNTERS_H
#define ORYX_COUNTERS_H

/** atomic is not an efficient api in fast-path calls. */

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

int
oryx_counter_get_array_range(counter_id s_id, counter_id e_id,
                                      struct CounterCtx *ctx);

/**
* \brief Get the value of the local copy of the counter that hold this id.
*/
static inline u64 oryx_counter_get(struct CounterCtx *ctx, counter_id id)
{
#if defined(BUILD_DEBUG)
	BUG_ON ((id < 1) || (id > ctx->size));
#endif
#if defined(COUNTER_USE_ATOMIC)
	return (u64)atomic64_read(&ctx->head[id].value);
#else
	return ctx->head[id].value;
#endif
}
#endif


/**
* \brief Increments the counter
*/
static inline void oryx_counter_inc(struct CounterCtx *ctx, counter_id id)
{
#if defined(BUILD_DEBUG)
  BUG_ON ((id < 1) || (id > ctx->size));
#endif
#if defined(COUNTER_USE_ATOMIC)
  atomic64_inc(&ctx->head[id].value);
  atomic64_inc(&ctx->head[id].updates);
#else
  ctx->head[id].value += 1;
  ctx->head[id].updates += 1;
#endif
}
/**
* \brief Adds a value of type uint64_t to the local counter.
*/
static inline void oryx_counter_add(struct CounterCtx *ctx, counter_id id, u64 x)
{
#if defined(BUILD_DEBUG)
  BUG_ON ((id < 1) || (id > ctx->size));
#endif

#if defined(COUNTER_USE_ATOMIC)
  atomic64_add(&ctx->head[id].value, x);
  atomic64_inc(&ctx->head[id].updates);
#else
  ctx->head[id].value += x;
  ctx->head[id].updates += 1;
#endif
  return;
}

/**
* \brief Sets a value of type double to the local counter
*/
static inline void oryx_counter_set(struct CounterCtx *ctx, counter_id id, u64 x)
{
#if defined(BUILD_DEBUG)
	  BUG_ON ((id < 1) || (id > ctx->size));
#endif

  if ((ctx->head[id].type == STATS_TYPE_Q_MAXIMUM) &&
		  (x > (u64)oryx_counter_get(ctx, id))) {
#if defined(COUNTER_USE_ATOMIC)
	  atomic64_set(&ctx->head[id].value, x);
#else
	  ctx->head[id].value = x;
#endif
  } else if (ctx->head[id].type == STATS_TYPE_Q_NORMAL) {
#if defined(COUNTER_USE_ATOMIC)
	  atomic64_set(&ctx->head[id].value, x);
#else
	  ctx->head[id].value = x;
#endif
  }

#if defined(COUNTER_USE_ATOMIC)
  atomic64_inc(&ctx->head[id].updates);
#else
  ctx->head[id].updates += 1;
#endif

  return;
}

