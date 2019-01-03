/*!
 * @file oryx_counter.h
 * @date 2017/08/29
 *
 * TSIHANG (haechime@gmail.com)
 */

#ifndef __ORYX_COUNTERS_H__
#define __ORYX_COUNTERS_H__

/** atomic is not an efficient api in fast-path calls. */

#include "counter.h"

#define COUNTER_RANGE_START(ctx)	1
#define COUNTER_RANGE_END(ctx)	(atomic_read((ctx)->curr_id))

ORYX_DECLARE (
	void oryx_counter_initialize (
		void
	)
);

ORYX_DECLARE (
	counter_id oryx_register_counter (
		IN const char *name,
		IN const char *comments,
		IN struct oryx_counter_ctx_t *ctx
	)
);

ORYX_DECLARE (
	void oryx_release_counter (
		IN struct oryx_counter_ctx_t *ctx
	)
);

ORYX_DECLARE (
	int oryx_counter_get_array_range (
		IN counter_id s_id,
		IN counter_id e_id,
		IN struct oryx_counter_ctx_t *ctx
	)
);

/**
* \brief Get the value of the local copy of the counter that hold this id.
*/
static __oryx_always_inline__
uint64_t oryx_counter_get
(
	IN struct oryx_counter_ctx_t *ctx,
	IN counter_id id
)
{
  BUG_ON ((id < 1) || (id > ctx->size));

#if defined(COUNTER_USE_ATOMIC)
  return (uint64_t)atomic_read(ctx->head[id].value);
#else
  return ctx->head[id].value;
#endif
}


#if defined(BUILD_DEBUG)

/**
* \brief Increments the counter
*/
static __oryx_always_inline__
void oryx_counter_inc
(
	IN struct oryx_counter_ctx_t *ctx,
	IN counter_id id
)
{
  BUG_ON ((id < 1) || (id > ctx->size));

#if defined(COUNTER_USE_ATOMIC)
  atomic_inc(ctx->head[id].value);
  atomic_inc(ctx->head[id].updates);
#else
  ctx->head[id].value += 1;
  ctx->head[id].updates += 1;
#endif
}
/**
* \brief Adds a value of type uint64_t to the local counter.
*/
static __oryx_always_inline__
void oryx_counter_add
(
	IN struct oryx_counter_ctx_t *ctx,
	IN counter_id id,
	IN uint64_t x
)
{
  BUG_ON ((id < 1) || (id > ctx->size));

#if defined(COUNTER_USE_ATOMIC)
  atomic_add(ctx->head[id].value, x);
  atomic_inc(ctx->head[id].updates);
#else
  ctx->head[id].value += x;
  ctx->head[id].updates += 1;
#endif
  return;
}

#else

#if defined(COUNTER_USE_ATOMIC)

	#define oryx_counter_inc(ctx,id)\
		atomic_inc((ctx)->head[(id)].value);\
		atomic_inc((ctx)->head[(id)].updates);

	#define oryx_counter_add(ctx,id,x)\
		atomic_add((ctx)->head[(id)].value, (x));\
		atomic_inc((ctx)->head[(id)].updates);

#else

	#define oryx_counter_inc(ctx,id)\
		(ctx)->head[(id)].value += 1;\
		(ctx)->head[(id)].updates += 1;

	#define oryx_counter_add(ctx,id,x)\
		(ctx)->head[(id)].value += (x);\
		(ctx)->head[(id)].updates += 1;
#endif

#endif

/**
* \brief Sets a value of type double to the local counter
*/
static __oryx_always_inline__
void oryx_counter_set
(
	IN struct oryx_counter_ctx_t *ctx,
	IN counter_id id,
	IN uint64_t x
)
{
	BUG_ON ((id < 1) || (id > ctx->size));

	if ((ctx->head[id].type == STATS_TYPE_Q_MAXIMUM) &&
	  				(x > (uint64_t)oryx_counter_get(ctx, id))) {
#if defined(COUNTER_USE_ATOMIC)
		atomic_set(ctx->head[id].value, x);
#else
		ctx->head[id].value = x;
#endif
	} else if (ctx->head[id].type == STATS_TYPE_Q_NORMAL) {
#if defined(COUNTER_USE_ATOMIC)
		atomic_set(ctx->head[id].value, x);
#else
		ctx->head[id].value = x;
#endif
	}

#if defined(COUNTER_USE_ATOMIC)
	atomic_inc(ctx->head[id].updates);
#else
	ctx->head[id].updates += 1;
#endif

	return;
}
#endif

