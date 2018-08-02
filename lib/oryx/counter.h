#ifndef COUNTER_H
#define COUNTER_H

typedef uint32_t counter_id;

#if defined(COUNTER_USE_ATOMIC)
typedef atomic64_t cu64;
#else
typedef uint64_t cu64;
#endif


/**
 * \brief Different kinds of qualifier that can be used to modify the behaviour
 *        of the counter to be registered
 */
enum {
    STATS_TYPE_Q_NORMAL = 1,
    STATS_TYPE_Q_AVERAGE = 2,
    STATS_TYPE_Q_MAXIMUM = 3,
    STATS_TYPE_Q_FUNC = 4,

    STATS_TYPE_Q_MAX = 5,
};

struct counter_t {
	const char		*sc_name;		/* alias for this counter. */
    int				type;			/* counter type */
    counter_id		id;				/* local id for this counter in this thread */
    counter_id		gid;			/* global id, used in output */

    /* counter value(s). thread-safety. */
    cu64 value;     /**< sum of updates/increments, or 'set' value */
    cu64 updates;   /**< number of updates (for avg) */

	uint64_t		(*hook)(void);	/* when using type STATS_TYPE_Q_FUNC this function is called once.
								 	 * to get the counter value, regardless of how many threads there are. */
	struct counter_t *next;			/* the next perfcounter instance. */

};

struct CounterCtx {
	int					h_size;
    atomic_t			curr_id;	/* holds the total no of counters already assigned for this perf context */
	os_mutex_t			m;			/* lock for head. */	
    uint32_t			size;		/* size of head array in elements */
	struct counter_t	*head;		/* array range, worked with get_array_range. */
	struct counter_t	*hhead;		/* pointer to the head of a list of counters assigned under this context */	
};


#endif

