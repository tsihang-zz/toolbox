#ifndef COUNTERS_H
#define COUNTERS_H

#include "counters_private.h"

/* forward declaration of the ThreadVars structure */
struct ThreadVars_;

#if 0
/**
 * \brief Container to hold the counter variable
 */
typedef struct StatsCounter_ {
    int type;

    /* local id for this counter in this thread */
    uint16_t id;

    /* global id, used in output */
    uint16_t gid;

    /* counter value(s): copies from the 'private' counter */
    uint64_t value;     /**< sum of updates/increments, or 'set' value */
    uint64_t updates;   /**< number of updates (for avg) */

    /* when using type STATS_TYPE_Q_FUNC this function is called once
     * to get the counter value, regardless of how many threads there are. */
    uint64_t (*hook)(void);

    /* name of the counter */
    const char *name;

    /* the next perfcounter for this tv's tm instance */
    struct StatsCounter_ *next;
} StatsCounter;


/**
 * \brief Stats Context for a ThreadVars instance
 */
typedef struct StatsPublicThreadContext_ {
    /* flag set by the wakeup thread, to inform the client threads to sync */
    uint32_t perf_flag;

    /* pointer to the head of a list of counters assigned under this context */
    StatsCounter *head;

    /* holds the total no of counters already assigned for this perf context */
    uint16_t curr_id;

    /* mutex to prevent simultaneous access during update_counter/output_stat */
    os_lock_t m;
} StatsPublicThreadContext;


/**
 * \brief Storage for local counters, with a link to the public counter used
 *        for syncs
 */
typedef struct StatsLocalCounter_ {
    /* pointer to the counter that corresponds to this local counter */
    StatsCounter *pc;

    /* local counter id of the above counter */
    uint16_t id;

    /* total value of the adds/increments, or exact value in case of 'set' */
    uint64_t value;

    /* no of times the local counter has been updated */
    uint64_t updates;
} StatsLocalCounter;

/**
 * \brief used to hold the private version of the counters registered
 */
typedef struct StatsPrivateThreadContext_ {
    /* points to the array holding local counters */
    StatsLocalCounter *head;

    /* size of head array in elements */
    uint32_t size;

    int initialized;
} StatsPrivateThreadContext;
#endif

void StatsInit(void);
void StatsIncr(struct ThreadVars_ *tv, uint16_t id);
int StatsSetupPrivate(struct ThreadVars_ *tv);
uint64_t StatsGetLocalCounterValue(struct ThreadVars_ *tv, uint16_t id);
void StatsReleaseCounters(StatsCounter *head);
uint16_t StatsRegisterCounter(const char *name, struct ThreadVars_ *tv);
uint16_t StatsRegisterAvgCounter(const char *name, struct ThreadVars_ *tv);
uint16_t StatsRegisterMaxCounter(const char *name, struct ThreadVars_ *tv);
uint16_t StatsRegisterGlobalCounter(const char *name, uint64_t (*Func)(void));
uint16_t RegisterCounter(const char *name, const char *tm_name,
                               StatsPublicThreadContext *pctx);


#endif
