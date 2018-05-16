#include "oryx.h"
#include "threadvars.h"
#include "counters.h"

/* Time interval for syncing the local counters with the global ones */
#define STATS_WUT_TTS 3

/* Time interval at which the mgmt thread o/p the stats */
#define STATS_MGMTT_TTS 8

/**
 * \brief Different kinds of qualifier that can be used to modify the behaviour
 *        of the counter to be registered
 */
enum {
    STATS_TYPE_NORMAL = 1,
    STATS_TYPE_AVERAGE = 2,
    STATS_TYPE_MAXIMUM = 3,
    STATS_TYPE_FUNC = 4,

    STATS_TYPE_MAX = 5,
};

/**
 * \brief per thread store of counters
 */
typedef struct StatsThreadStore_ {
    /** thread name used in output */
    const char *name;

    StatsPublicThreadContext *ctx;

    StatsPublicThreadContext **head;
    uint32_t size;

    struct StatsThreadStore_ *next;
} StatsThreadStore;


/**
 * \brief Holds the output interface context for the counter api
 */
typedef struct StatsGlobalContext_ {
    /** list of thread stores: one per thread plus one global */
    StatsThreadStore *sts;
    os_lock_t sts_lock;
    int sts_cnt;

    //HashTable *counters_id_hash;
	struct oryx_htable_t *counters_id_hash;

    StatsPublicThreadContext global_counter_ctx;
} StatsGlobalContext;


static StatsGlobalContext *stats_ctx = NULL;

typedef struct CountersIdType_ {
    uint16_t id;
    const char *string;
} CountersIdType;

static uint16_t counters_global_id = 0;

static uint32_t CountersIdHashFunc(struct oryx_htable_t *ht, 
		void *data, uint32_t datalen)
{
    CountersIdType *t = (CountersIdType *)data;
    uint32_t hash = 0;
    int i = 0;

    int len = strlen(t->string);

    for (i = 0; i < len; i++)
        hash += tolower((unsigned char)t->string[i]);

    hash = hash % ht->array_size;

    return hash;
}

static int CountersIdHashCompareFunc(void *data1, uint32_t datalen1,
                               void *data2, uint32_t datalen2)
{
    CountersIdType *t1 = (CountersIdType *)data1;
    CountersIdType *t2 = (CountersIdType *)data2;
    int len1 = 0;
    int len2 = 0;

    if ((t1 == NULL || t2 == NULL) ||
		(t1->string == NULL || t2->string == NULL))
        return 1;

    len1 = strlen(t1->string);
    len2 = strlen(t2->string);

    if (len1 == len2 && memcmp(t1->string, t2->string, len1) == 0) {
        return 0;
    }

    return 1;
}

static void CountersIdHashFreeFunc(void *data)
{
    free(data);
}

static void StatsPublicThreadContextInit(StatsPublicThreadContext *t)
{
    pthread_mutex_init(&t->m, NULL);
}

/**
 * \brief Get the value of the local copy of the counter that hold this id.
 *
 * \param tv threadvars
 * \param id The counter id.
 *
 * \retval  0 on success.
 * \retval -1 on error.
 */
uint64_t StatsGetLocalCounterValue(ThreadVars *tv, uint16_t id)
{
    StatsPrivateThreadContext *pca = &tv->perf_private_ctx;
#ifdef DEBUG
    BUG_ON ((id < 1) || (id > pca->size));
#endif
    return pca->head[id].value;
}


/**
 * \brief Releases a counter
 *
 * \param pc Pointer to the StatsCounter to be freed
 */
static void StatsReleaseCounter(StatsCounter *pc)
{
    if (pc != NULL) {
        free(pc);
    }

    return;
}


/**
 * \brief Releases counters
 *
 * \param head Pointer to the head of the list of perf counters that have to
 *             be freed
 */
void StatsReleaseCounters(StatsCounter *head)
{
    StatsCounter *pc = NULL;

    while (head != NULL) {
        pc = head;
        head = head->next;
        StatsReleaseCounter(pc);
    }

    return;
}

/**
 * \brief Initializes the perf counter api.  Things are hard coded currently.
 *        More work to be done when we implement multiple interfaces
 */
void StatsInit(void)
{
    BUG_ON(stats_ctx != NULL);
    if ( (stats_ctx = malloc(sizeof(StatsGlobalContext))) == NULL) {
        oryx_loge(0,
			"Fatal error encountered in StatsInitCtx. Exiting...\n");
        exit(EXIT_FAILURE);
    }
    memset(stats_ctx, 0, sizeof(StatsGlobalContext));

    StatsPublicThreadContextInit(&stats_ctx->global_counter_ctx);
}

static void StatsPublicThreadContextCleanup(StatsPublicThreadContext *t)
{
    do_lock(&t->m);
    StatsReleaseCounters(t->head);
    t->head = NULL;
    t->perf_flag = 0;
    t->curr_id = 0;
    do_unlock(&t->m);
    do_destroy_lock(&t->m);
}

/**
 * \brief Adds a value of type uint64_t to the local counter.
 *
 * \param id  ID of the counter as set by the API
 * \param pca Counter array that holds the local counter for this TM
 * \param x   Value to add to this local counter
 */
void StatsAddUI64(ThreadVars *tv, uint16_t id, uint64_t x)
{
    StatsPrivateThreadContext *pca = &tv->perf_private_ctx;
#ifdef UNITTESTS
    if (pca->initialized == 0)
        return;
#endif
#ifdef DEBUG
    BUG_ON ((id < 1) || (id > pca->size));
#endif
    pca->head[id].value += x;
    pca->head[id].updates++;
    return;
}

/**
 * \brief Sets a value of type double to the local counter
 *
 * \param id  Index of the local counter in the counter array
 * \param pca Pointer to the StatsPrivateThreadContext
 * \param x   The value to set for the counter
 */
void StatsSetUI64(ThreadVars *tv, uint16_t id, uint64_t x)
{
    StatsPrivateThreadContext *pca = &tv->perf_private_ctx;
#ifdef UNITTESTS
    if (pca->initialized == 0)
        return;
#endif
#ifdef DEBUG
    BUG_ON ((id < 1) || (id > pca->size));
#endif

    if ((pca->head[id].pc->type == STATS_TYPE_MAXIMUM) &&
            (x > pca->head[id].value)) {
        pca->head[id].value = x;
    } else if (pca->head[id].pc->type == STATS_TYPE_NORMAL) {
        pca->head[id].value = x;
    }

    pca->head[id].updates++;

    return;
}

void StatsIncr(struct ThreadVars_ *tv, uint16_t id)
{
    StatsPrivateThreadContext *pca = &tv->perf_private_ctx;
#ifdef UNITTESTS
    if (pca->initialized == 0)
        return;
#endif
#ifdef DEBUG
    BUG_ON ((id < 1) || (id > pca->size));
#endif
    pca->head[id].value++;
    pca->head[id].updates++;
    return;
}


/**
 * \brief Registers a counter.
 *
 * \param name    Name of the counter, to be registered
 * \param tm_name  Thread module to which this counter belongs
 * \param pctx     StatsPublicThreadContext for this tm-tv instance
 * \param type_q   Qualifier describing the type of counter to be registered
 *
 * \retval the counter id for the newly registered counter, or the already
 *         present counter on success
 * \retval 0 on failure
 */
static uint16_t StatsRegisterQualifiedCounter(const char *name, 
					const char __oryx_unused__ *tm_name, StatsPublicThreadContext *pctx,
					int type_q, uint64_t (*fn)(void))
{
    StatsCounter **head = &pctx->head;
    StatsCounter *temp = NULL;
    StatsCounter *prev = NULL;
    StatsCounter *pc = NULL;

    if (name == NULL || pctx == NULL) {
        printf("Counter name, StatsPublicThreadContext NULL\n");
        return 0;
    }

    temp = prev = *head;
    while (temp != NULL) {
        prev = temp;

        if (strcmp(name, temp->name) == 0) {
            break;
        }

        temp = temp->next;
    }

    /* We already have a counter registered by this name */
    if (temp != NULL)
        return(temp->id);

    /* if we reach this point we don't have a counter registered by this name */
    if ( (pc = malloc(sizeof(StatsCounter))) == NULL)
        return 0;
    memset(pc, 0, sizeof(StatsCounter));

    /* assign a unique id to this StatsCounter.  The id is local to this
     * thread context.  Please note that the id start from 1, and not 0 */
    pc->id = ++(pctx->curr_id);
    pc->name = name;
    pc->type = type_q;
    pc->hook = fn;

    /* we now add the counter to the list */
    if (prev == NULL)
        *head = pc;
    else
        prev->next = pc;

    return pc->id;
}


/**
 * \brief Registers a normal, unqualified counter
 *
 * \param name Name of the counter, to be registered
 * \param tv    Pointer to the ThreadVars instance for which the counter would
 *              be registered
 *
 * \retval id Counter id for the newly registered counter, or the already
 *            present counter
 */
uint16_t StatsRegisterCounter(const char *name, struct ThreadVars_ *tv)
{
    uint16_t id = StatsRegisterQualifiedCounter(name,
                                                 (tv->thread_group_name != NULL) ? tv->thread_group_name : tv->printable_name,
                                                 &tv->perf_public_ctx,
                                                 STATS_TYPE_NORMAL, NULL);

    return id;
}

/**
 * \brief Registers a counter, whose value holds the average of all the values
 *        assigned to it.
 *
 * \param name Name of the counter, to be registered
 * \param tv    Pointer to the ThreadVars instance for which the counter would
 *              be registered
 *
 * \retval id Counter id for the newly registered counter, or the already
 *            present counter
 */
uint16_t StatsRegisterAvgCounter(const char *name, struct ThreadVars_ *tv)
{
    uint16_t id = StatsRegisterQualifiedCounter(name,
                                                 (tv->thread_group_name != NULL) ? tv->thread_group_name : tv->printable_name,
                                                 &tv->perf_public_ctx,
                                                 STATS_TYPE_AVERAGE, NULL);

    return id;
}

/**
 * \brief Registers a counter, whose value holds the maximum of all the values
 *        assigned to it.
 *
 * \param name Name of the counter, to be registered
 * \param tv    Pointer to the ThreadVars instance for which the counter would
 *              be registered
 *
 * \retval the counter id for the newly registered counter, or the already
 *         present counter
 */
uint16_t StatsRegisterMaxCounter(const char *name, struct ThreadVars_ *tv)
{
    uint16_t id = StatsRegisterQualifiedCounter(name,
                                                 (tv->thread_group_name != NULL) ? tv->thread_group_name : tv->printable_name,
                                                 &tv->perf_public_ctx,
                                                 STATS_TYPE_MAXIMUM, NULL);

    return id;
}

/**
 * \brief Registers a counter, which represents a global value
 *
 * \param name Name of the counter, to be registered
 * \param Func  Function Pointer returning a uint64_t
 *
 * \retval id Counter id for the newly registered counter, or the already
 *            present counter
 */
uint16_t StatsRegisterGlobalCounter(const char *name, uint64_t (*Func)(void))
{
#ifdef UNITTESTS
    if (stats_ctx == NULL)
        return 0;
#else
    BUG_ON(stats_ctx == NULL);
#endif
    uint16_t id = StatsRegisterQualifiedCounter(name, NULL,
                                                 &(stats_ctx->global_counter_ctx),
                                                 STATS_TYPE_FUNC,
                                                 Func);
    return id;
}


/** \internal
 *  \brief Returns a counter array for counters in this id range(s_id - e_id)
 *
 *  \param s_id Counter id of the first counter to be added to the array
 *  \param e_id Counter id of the last counter to be added to the array
 *  \param pctx Pointer to the tv's StatsPublicThreadContext
 *
 *  \retval a counter-array in this(s_id-e_id) range for this TM instance
 */
static int StatsGetCounterArrayRange(uint16_t s_id, uint16_t e_id,
                                      StatsPublicThreadContext *pctx,
                                      StatsPrivateThreadContext *pca)
{
    StatsCounter *pc = NULL;
    uint32_t i = 0;

    if (pctx == NULL || pca == NULL) {
        oryx_logd("pctx/pca is NULL");
        return -1;
    }

    if (s_id < 1 || e_id < 1 || s_id > e_id) {
        oryx_logd("error with the counter ids");
        return -1;
    }

    if (e_id > pctx->curr_id) {
        oryx_logd("end id is greater than the max id for this tv");
        return -1;
    }

    if ( (pca->head = malloc(sizeof(StatsLocalCounter) * (e_id - s_id  + 2))) == NULL) {
        return -1;
    }
    memset(pca->head, 0, sizeof(StatsLocalCounter) * (e_id - s_id  + 2));

    pc = pctx->head;
    while (pc->id != s_id)
        pc = pc->next;

    i = 1;
    while ((pc != NULL) && (pc->id <= e_id)) {
        pca->head[i].pc = pc;
        pca->head[i].id = pc->id;
        pc = pc->next;
        i++;
    }
    pca->size = i - 1;

    pca->initialized = 1;
    return 0;
}

/** \internal
 *  \brief Returns a counter array for all counters registered for this tm
 *         instance
 *
 *  \param pctx Pointer to the tv's StatsPublicThreadContext
 *
 *  \retval pca Pointer to a counter-array for all counter of this tm instance
 *              on success; NULL on failure
 */
static int StatsGetAllCountersArray(StatsPublicThreadContext *pctx, StatsPrivateThreadContext *private)
{
    if (pctx == NULL || private == NULL)
        return -1;

    return StatsGetCounterArrayRange(1, pctx->curr_id, pctx, private);
}



/** \internal
 *  \brief Adds a TM to the clubbed TM table.  Multiple instances of the same TM
 *         are stacked together in a PCTMI container.
 *
 *  \param tm_name Name of the tm to be added to the table
 *  \param pctx    StatsPublicThreadContext associated with the TM tm_name
 *
 *  \retval 1 on success, 0 on failure
 */
static int StatsThreadRegister(const char *thread_name, StatsPublicThreadContext *pctx)
{
    if (stats_ctx == NULL) {
        oryx_logd("Counter module has been disabled");
        return 0;
    }

    StatsThreadStore *temp = NULL;

    if (thread_name == NULL || pctx == NULL) {
        oryx_logd("supplied argument(s) to StatsThreadRegister NULL");
        return 0;
    }

    do_lock(&stats_ctx->sts_lock);
    if (stats_ctx->counters_id_hash == NULL) {
        stats_ctx->counters_id_hash = oryx_htable_init(256, CountersIdHashFunc,
                                                              CountersIdHashCompareFunc,
                                                              CountersIdHashFreeFunc, 0);
        BUG_ON(stats_ctx->counters_id_hash == NULL);
    }
    StatsCounter *pc = pctx->head;
    while (pc != NULL) {
        CountersIdType t = { 0, pc->name }, *id = NULL;
        id = oryx_htable_lookup(stats_ctx->counters_id_hash, &t, sizeof(t));
        if (id == NULL) {
            id = calloc(1, sizeof(*id));
            BUG_ON(id == NULL);
            id->id = counters_global_id++;
            id->string = pc->name;
            BUG_ON(oryx_htable_add(stats_ctx->counters_id_hash, id, sizeof(*id)) < 0);
        }
        pc->gid = id->id;
        pc = pc->next;
    }


    if ( (temp = malloc(sizeof(StatsThreadStore))) == NULL) {
        do_unlock(&stats_ctx->sts_lock);
        return 0;
    }
    memset(temp, 0, sizeof(StatsThreadStore));

    temp->ctx = pctx;
    temp->name = thread_name;

    temp->next = stats_ctx->sts;
    stats_ctx->sts = temp;
    stats_ctx->sts_cnt++;
    oryx_logd("stats_ctx->sts %p", stats_ctx->sts);

    do_unlock(&stats_ctx->sts_lock);
    return 1;
}

int StatsSetupPrivate(ThreadVars *tv)
{
    StatsGetAllCountersArray(&(tv)->perf_public_ctx, &(tv)->perf_private_ctx);

    StatsThreadRegister(tv->printable_name ? tv->printable_name : tv->name,
        &(tv)->perf_public_ctx);
    return 0;
}

/** \internal
 * \brief Registers a normal, unqualified counter
 *
 * \param name   Name of the counter, to be registered
 * \param tm_name Name of the engine module under which the counter has to be
 *                registered
 * \param type    Datatype of this counter variable
 * \param pctx    StatsPublicThreadContext corresponding to the tm_name key under which the
 *                key has to be registered
 *
 * \retval id Counter id for the newly registered counter, or the already
 *            present counter
 */
uint16_t RegisterCounter(const char *name, const char *tm_name,
                               StatsPublicThreadContext *pctx)
{
    uint16_t id = StatsRegisterQualifiedCounter(name, tm_name, pctx,
                                                STATS_TYPE_NORMAL, NULL);
    return id;
}

