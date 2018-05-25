#ifndef COUNTER_H
#define COUNTER_H

typedef u32 counter_id;

#if defined(COUNTER_USE_ATOMIC)
typedef atomic64_t cu64;
#else
typedef u64 cu64;
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

	/** alias for this counter. */
	const char *sc_name;

	/** counter type */
    int type;

    /* local id for this counter in this thread */
    counter_id id;

    /* global id, used in output */
    counter_id gid;

    /* counter value(s). thread-safety. */
    cu64 value;     /**< sum of updates/increments, or 'set' value */
    cu64 updates;   /**< number of updates (for avg) */

    /* when using type STATS_TYPE_Q_FUNC this function is called once
     * to get the counter value, regardless of how many threads there are. */
	u64 (*hook)(void);

    /* the next perfcounter instance. */
	struct counter_t *next;

};

struct CounterCtx {

	int h_size;
	
    /* pointer to the head of a list of counters assigned under this context */	
	struct counter_t *hhead;

    /* holds the total no of counters already assigned for this perf context */
    atomic_t curr_id;

	/** lock for head. */
	os_lock_t m;

	/* size of head array in elements */
    uint32_t size;

	/** array range, worked with get_array_range. */
	struct counter_t *head;
};


struct StatsGlobalCtx {
	struct oryx_htable_t *counter_id_hash;
	os_lock_t lock;
};


#endif

