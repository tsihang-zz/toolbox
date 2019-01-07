/*!
 * @file counter.h
 * @date 2017/08/29
 *
 * TSIHANG (haechime@gmail.com)
 */

#ifndef __COUNTER_H__
#define __COUNTER_H__

/* Allow the use in C++ code.  */
#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t oryx_counter_id_t;
typedef uint64_t uintx_t;

#define COUNTER_RANGE_START(ctx)	1
#define COUNTER_RANGE_END(ctx)	(atomic_read((ctx)->curr_id))

/**
 * \brief Different kinds of qualifier that can be used
 * to modify the behaviour of the counter to be registered
 */
enum {
    STATS_TYPE_Q_NORMAL = 1,
    STATS_TYPE_Q_AVERAGE = 2,
    STATS_TYPE_Q_MAXIMUM = 3,
    STATS_TYPE_Q_FUNC = 4,

    STATS_TYPE_Q_MAX = 5,
};

struct oryx_counter_t {
	const char	*sc_name;			/**< alias for this counter. */
	int		type;					/**< counter type */
	oryx_counter_id_t id;			/**< local id for this counter in this thread */
	oryx_counter_id_t gid;			/**< global id, used in output */
	
	/**< counter value(s). thread-safety. */
	uintx_t		value;				/**< sum of updates/increments, or 'set' value */
	uintx_t		updates;			/**< number of updates (for avg) */
	struct oryx_counter_t *next;	/**< the next perfcounter instance. */
	uint64_t	(*hook)(void);		/**< when using type STATS_TYPE_Q_FUNC this function is called once.
									 **< to get the counter value, regardless of how many threads there are. */
};

struct oryx_counter_ctx_t {
	int		h_size;
	uint32_t	size;				/**< size of head array in elements */
	struct oryx_counter_t	*head;	/**< array range, worked with get_array_range. */
	struct oryx_counter_t	*hhead;	/**< pointer to the head of a list of counters
									 **< assigned under this context */
	ATOMIC_DECLARE(oryx_counter_id_t, curr_id);	/**< holds the total no of counters
															**< already assigned for this perf context */
};

#ifdef __cplusplus
}
#endif /* C++ */

#endif

