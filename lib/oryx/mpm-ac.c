#include "oryx.h"
#include "mpm-ac.h"

void ACinitCtx(MpmCtx *);
void ACInitThreadCtx(MpmCtx *, MpmThreadCtx *);
void ACDestroyCtx(MpmCtx *);
void ACDestroyThreadCtx(MpmCtx *, MpmThreadCtx *);
int ACAddPatternCI(MpmCtx *, uint8_t *, uint16_t, uint16_t, uint16_t,
                     uint32_t, sig_id, uint8_t);
int ACAddPatternCS(MpmCtx *, uint8_t *, uint16_t, uint16_t, uint16_t,
                     uint32_t, sig_id, uint8_t);
int ACPreparePatterns(MpmCtx *mpm_ctx);
uint32_t ACSearch(MpmCtx *mpm_ctx, MpmThreadCtx *mpm_thread_ctx,
                    PrefilterRuleStore *pmq, const uint8_t *buf, uint16_t buflen);
void ACPrintInfo(MpmCtx *mpm_ctx);
void ACPrintSearchStats(MpmThreadCtx *mpm_thread_ctx);

/* a placeholder to denote a failure transition in the goto table */
#define SC_AC_FAIL (-1)

#define STATE_QUEUE_CONTAINER_SIZE 65536

#define AC_CASE_MASK    0x80000000
#define AC_PID_MASK     0x7FFFFFFF
#define AC_CASE_BIT     31

static int construct_both_16_and_32_state_tables = 0;

/**
 * \brief Helper structure used by AC during state table creation
 */
typedef struct StateQueue_ {
    int32_t store[STATE_QUEUE_CONTAINER_SIZE];
    int top;
    int bot;
} StateQueue;

/**
 * \internal
 * \brief Initialize the AC context with user specified conf parameters.  We
 *        aren't retrieving anything for AC conf now, but we will certainly
 *        need it, when we customize AC.
 */
static void ACGetConfig (void)
{
    //ConfNode *ac_conf;
    //const char *hash_val = NULL;

    //ConfNode *pm = ConfGetNode("pattern-matcher");

    return;
}

/**
 * \internal
 * \brief Initialize a new state in the goto and output tables.
 *
 * \param mpm_ctx Pointer to the mpm context.
 *
 * \retval The state id, of the newly created state.
 */
static inline int ACReallocState(SCACCtx *ctx, uint32_t cnt)
{
    void *ptmp;
    int64_t size = 0;

    /* reallocate space in the goto table to include a new state */
    size = cnt * ctx->single_state_size;
    ptmp = krealloc(ctx->goto_table, size, MPF_NOFLGS, __oryx_unused_val__);
	ASSERT (ptmp);
    if (ptmp == NULL) {
        kfree(ctx->goto_table);
        ctx->goto_table = NULL;
        printf ("Error allocating memory (%u, %u, %lu)\n", cnt, ctx->single_state_size, size);
	
        exit(EXIT_FAILURE);
    }
    ctx->goto_table = ptmp;

    /* reallocate space in the output table for the new state */
    int oldsize = ctx->state_count * sizeof(SCACOutputTable);
    size = cnt * sizeof(SCACOutputTable);
#ifdef MPM_DEBUG
    printf("oldsize %d size %d cnt %u ctx->state_count %u\n",
            oldsize, size, cnt, ctx->state_count);
#endif
    ptmp = krealloc(ctx->output_table, size, MPF_NOFLGS, __oryx_unused_val__);
ASSERT (ptmp);
    if (ptmp == NULL) {
        kfree(ctx->output_table);
        ctx->output_table = NULL;
	printf ("Error allocating memory (%lu)\n", size);
        exit(EXIT_FAILURE);
    }
    ctx->output_table = ptmp;

    memset(((uint8_t *)ctx->output_table + oldsize), 0, (size - oldsize));

    /* \todo using it temporarily now during dev, since I have restricted
     *       state var in SCACCtx->state_table to uint16_t. */
    //if (ctx->state_count > 65536) {
    //    printf("state count exceeded\n");
    //    exit(EXIT_FAILURE);
    //}

    return 0;//ctx->state_count++;
}

/** \internal
 *  \brief Shrink state after setup is done
 *
 *  Shrinks only the output table, goto table is freed after calling this
 */
static void ACShrinkState(SCACCtx *ctx)
{
    /* reallocate space in the output table for the new state */
#ifdef MPM_DEBUG
    int oldsize = ctx->allocated_state_count * sizeof(SCACOutputTable);
#endif
    int newsize = ctx->state_count * sizeof(SCACOutputTable);

#ifdef MPM_DEBUG
    printf("oldsize %d newsize %d ctx->allocated_state_count %u "
               "ctx->state_count %u: shrink by %d bytes\n", oldsize,
               newsize, ctx->allocated_state_count, ctx->state_count,
               oldsize - newsize);
#else
    printf("newsize %d ctx->allocated_state_count %u "
               "ctx->state_count %u\n",
               newsize, ctx->allocated_state_count, ctx->state_count);
#endif

    void *ptmp = krealloc(ctx->output_table, newsize, MPF_NOFLGS, __oryx_unused_val__);
ASSERT (ptmp);
    if (ptmp == NULL) {
        kfree(ctx->output_table);
        ctx->output_table = NULL;
	printf ("Error allocating memory (%d)\n", newsize);
        exit(EXIT_FAILURE);
    }
    ctx->output_table = ptmp;
}

static inline int SCACInitNewState(MpmCtx *mpm_ctx)
{
    SCACCtx *ctx = (SCACCtx *)mpm_ctx->ctx;;

    /* Exponentially increase the allocated space when needed. */
    if (ctx->allocated_state_count < ctx->state_count + 1) {
        if (ctx->allocated_state_count == 0)
            ctx->allocated_state_count = 256;
        else
            ctx->allocated_state_count *= 2;

        ACReallocState(ctx, ctx->allocated_state_count);

    }
#if 0
    if (ctx->allocated_state_count > 260) {
        SCACOutputTable *output_state = &ctx->output_table[260];
        printf("output_state %p %p %u", output_state, output_state->pids, output_state->no_of_entries);
    }
#endif
    int ascii_code = 0;
    /* set all transitions for the newly assigned state as FAIL transitions */
    for (ascii_code = 0; ascii_code < 256; ascii_code++) {
        ctx->goto_table[ctx->state_count][ascii_code] = SC_AC_FAIL;
    }

    return ctx->state_count++;
}

/**
 * \internal
 * \brief Adds a pid to the output table for a state.
 *
 * \param state   The state to whose output table we should add the pid.
 * \param pid     The pattern id to add.
 * \param mpm_ctx Pointer to the mpm context.
 */
static void SCACSetOutputState(int32_t state, uint32_t pid, MpmCtx *mpm_ctx)
{
    void *ptmp;
    SCACCtx *ctx = (SCACCtx *)mpm_ctx->ctx;
    SCACOutputTable *output_state = &ctx->output_table[state];
    uint32_t i = 0;

    for (i = 0; i < output_state->no_of_entries; i++) {
        if (output_state->pids[i] == pid)
            return;
    }

    output_state->no_of_entries++;
    ptmp = krealloc(output_state->pids,
                     output_state->no_of_entries * sizeof(uint32_t), MPF_NOFLGS, __oryx_unused_val__);
ASSERT (ptmp);
    if (ptmp == NULL) {
        kfree(output_state->pids);
        output_state->pids = NULL;
	printf ("Error allocating memory (%lu)\n", output_state->no_of_entries * sizeof(uint32_t));
        exit(EXIT_FAILURE);
    }
    output_state->pids = ptmp;

    output_state->pids[output_state->no_of_entries - 1] = pid;

    return;
}

/**
 * \brief Helper function used by SCACCreateGotoTable.  Adds a pattern to the
 *        goto table.
 *
 * \param pattern     Pointer to the pattern.
 * \param pattern_len Pattern length.
 * \param pid         The pattern id, that corresponds to this pattern.  We
 *                    need it to updated the output table for this pattern.
 * \param mpm_ctx     Pointer to the mpm context.
 */
static inline void SCACEnter(uint8_t *pattern, uint16_t pattern_len, uint32_t pid,
                             MpmCtx *mpm_ctx)
{
    SCACCtx *ctx = (SCACCtx *)mpm_ctx->ctx;
    int32_t state = 0;
    int32_t newstate = 0;
    int i = 0;
    int p = 0;

    /* walk down the trie till we have a match for the pattern prefix */
    state = 0;
    for (i = 0; i < pattern_len; i++) {
        if (ctx->goto_table[state][pattern[i]] != SC_AC_FAIL) {
            state = ctx->goto_table[state][pattern[i]];
        } else {
            break;
        }
    }

    /* add the non-matching pattern suffix to the trie, from the last state
     * we left off */
    for (p = i; p < pattern_len; p++) {
        newstate = SCACInitNewState(mpm_ctx);
        ctx->goto_table[state][pattern[p]] = newstate;
        state = newstate;
    }

    /* add this pattern id, to the output table of the last state, where the
     * pattern ends in the trie */
    SCACSetOutputState(state, pid, mpm_ctx);

    return;
}

/**
 * \internal
 * \brief Create the goto table.
 *
 * \param mpm_ctx Pointer to the mpm context.
 */
static inline void SCACCreateGotoTable(MpmCtx *mpm_ctx)
{
    SCACCtx *ctx = (SCACCtx *)mpm_ctx->ctx;
    uint32_t i = 0;

    /* add each pattern to create the goto table */
    for (i = 0; i < mpm_ctx->pattern_cnt; i++) {
        SCACEnter(ctx->parray[i]->ci, ctx->parray[i]->len,
                  ctx->parray[i]->id, mpm_ctx);
    }

    int ascii_code = 0;
    for (ascii_code = 0; ascii_code < 256; ascii_code++) {
        if (ctx->goto_table[0][ascii_code] == SC_AC_FAIL) {
            ctx->goto_table[0][ascii_code] = 0;
        }
    }

    return;
}

static inline void SCACDetermineLevel1Gap(MpmCtx *mpm_ctx)
{
    SCACCtx *ctx = (SCACCtx *)mpm_ctx->ctx;
    uint32_t u = 0;

    int map[256];
    memset(map, 0, sizeof(map));

    for (u = 0; u < mpm_ctx->pattern_cnt; u++)
        map[ctx->parray[u]->ci[0]] = 1;

    for (u = 0; u < 256; u++) {
        if (map[u] == 0)
            continue;
        int32_t newstate = SCACInitNewState(mpm_ctx);
        ctx->goto_table[0][u] = newstate;
    }

    return;
}

static inline int SCACStateQueueIsEmpty(StateQueue *q)
{
    if (q->top == q->bot)
        return 1;
    else
        return 0;
}

static inline void SCACEnqueue(StateQueue *q, int32_t state)
{
    int i = 0;

    /*if we already have this */
    for (i = q->bot; i < q->top; i++) {
        if (q->store[i] == state)
            return;
    }

    q->store[q->top++] = state;

    if (q->top == STATE_QUEUE_CONTAINER_SIZE)
        q->top = 0;

    if (q->top == q->bot) {
        printf ("Just ran out of space in the queue.  "
                      "Fatal Error.  Exiting.  Please file a bug report on this");
        exit(EXIT_FAILURE);
    }

    return;
}

static inline int32_t SCACDequeue(StateQueue *q)
{
    if (q->bot == STATE_QUEUE_CONTAINER_SIZE)
        q->bot = 0;

    if (q->bot == q->top) {
        printf ("StateQueue behaving weirdly.  "
                      "Fatal Error.  Exiting.  Please file a bug report on this");
        exit(EXIT_FAILURE);
    }

    return q->store[q->bot++];
}

/*
#define SCACStateQueueIsEmpty(q) (((q)->top == (q)->bot) ? 1 : 0)

#define SCACEnqueue(q, state) do { \
                                  int i = 0; \
                                             \
                                  for (i = (q)->bot; i < (q)->top; i++) { \
                                      if ((q)->store[i] == state)       \
                                      return; \
                                  } \
                                    \
                                  (q)->store[(q)->top++] = state;   \
                                                                \
                                  if ((q)->top == STATE_QUEUE_CONTAINER_SIZE) \
                                      (q)->top = 0;                     \
                                                                        \
                                  if ((q)->top == (q)->bot) {           \
                                  oryx_log_critical(ERRNO_FATAL, "Just ran out of space in the queue.  " \
                                                "Fatal Error.  Exiting.  Please file a bug report on this"); \
                                  exit(EXIT_FAILURE);                   \
                                  }                                     \
                              } while (0)

#define SCACDequeue(q) ( (((q)->bot == STATE_QUEUE_CONTAINER_SIZE)? ((q)->bot = 0): 0), \
                         (((q)->bot == (q)->top) ?                      \
                          (printf("StateQueue behaving "                \
                                         "weirdly.  Fatal Error.  Exiting.  Please " \
                                         "file a bug report on this"), \
                           exit(EXIT_FAILURE)) : 0), \
                         (q)->store[(q)->bot++])     \
*/

/**
 * \internal
 * \brief Club the output data from 2 states and store it in the 1st state.
 *        dst_state_data = {dst_state_data} UNION {src_state_data}
 *
 * \param dst_state First state(also the destination) for the union operation.
 * \param src_state Second state for the union operation.
 * \param mpm_ctx Pointer to the mpm context.
 */
static inline void SCACClubOutputStates(int32_t dst_state, int32_t src_state,
                                        MpmCtx *mpm_ctx)
{
    void *ptmp;
    SCACCtx *ctx = (SCACCtx *)mpm_ctx->ctx;
    uint32_t i = 0;
    uint32_t j = 0;

    SCACOutputTable *output_dst_state = &ctx->output_table[dst_state];
    SCACOutputTable *output_src_state = &ctx->output_table[src_state];

    for (i = 0; i < output_src_state->no_of_entries; i++) {
        for (j = 0; j < output_dst_state->no_of_entries; j++) {
            if (output_src_state->pids[i] == output_dst_state->pids[j]) {
                break;
            }
        }
        if (j == output_dst_state->no_of_entries) {
            output_dst_state->no_of_entries++;

            ptmp = krealloc(output_dst_state->pids,
                             (output_dst_state->no_of_entries * sizeof(uint32_t)), MPF_NOFLGS, __oryx_unused_val__);
	ASSERT (ptmp);
            if (ptmp == NULL) {
                kfree(output_dst_state->pids);
                output_dst_state->pids = NULL;
                printf ("Error allocating memory");
                exit(EXIT_FAILURE);
            }
            output_dst_state->pids = ptmp;

            output_dst_state->pids[output_dst_state->no_of_entries - 1] =
                output_src_state->pids[i];
        }
    }

    return;
}

/**
 * \internal
 * \brief Create the failure table.
 *
 * \param mpm_ctx Pointer to the mpm context.
 */
static inline void SCACCreateFailureTable(MpmCtx *mpm_ctx)
{
    SCACCtx *ctx = (SCACCtx *)mpm_ctx->ctx;
    int ascii_code = 0;
    int32_t state = 0;
    int32_t r_state = 0;

    StateQueue q;
    memset(&q, 0, sizeof(StateQueue));

    /* allot space for the failure table.  A failure entry in the table for
     * every state(SCACCtx->state_count) */
    ctx->failure_table = kmalloc(ctx->state_count * sizeof(int32_t), MPF_CLR, __oryx_unused_val__);
ASSERT (ctx->failure_table);
    if (ctx->failure_table == NULL) {
        printf ("Error allocating memory");
        exit(EXIT_FAILURE);
    }
	
    /* add the failure transitions for the 0th state, and add every non-fail
     * transition from the 0th state to the queue for further processing
     * of failure states */
    for (ascii_code = 0; ascii_code < 256; ascii_code++) {
        int32_t temp_state = ctx->goto_table[0][ascii_code];
        if (temp_state != 0) {
            SCACEnqueue(&q, temp_state);
            ctx->failure_table[temp_state] = 0;
        }
    }

    while (!SCACStateQueueIsEmpty(&q)) {
        /* pick up every state from the queue and add failure transitions */
        r_state = SCACDequeue(&q);
        for (ascii_code = 0; ascii_code < 256; ascii_code++) {
            int32_t temp_state = ctx->goto_table[r_state][ascii_code];
            if (temp_state == SC_AC_FAIL)
                continue;
            SCACEnqueue(&q, temp_state);
            state = ctx->failure_table[r_state];

            while(ctx->goto_table[state][ascii_code] == SC_AC_FAIL)
                state = ctx->failure_table[state];
            ctx->failure_table[temp_state] = ctx->goto_table[state][ascii_code];
            SCACClubOutputStates(temp_state, ctx->failure_table[temp_state],
                                 mpm_ctx);
        }
    }

    return;
}

/**
 * \internal
 * \brief Create the delta table.
 *
 * \param mpm_ctx Pointer to the mpm context.
 */
static inline void SCACCreateDeltaTable(MpmCtx *mpm_ctx)
{
    SCACCtx *ctx = (SCACCtx *)mpm_ctx->ctx;
    int ascii_code = 0;
    int32_t r_state = 0;

    if ((ctx->state_count < 32767) || construct_both_16_and_32_state_tables) {
        ctx->state_table_u16 = kmalloc(ctx->state_count *
                                        sizeof(SC_AC_STATE_TYPE_U16) * 256, MPF_CLR, __oryx_unused_val__);
		ASSERT (ctx->state_table_u16);
        if (ctx->state_table_u16 == NULL) {
            printf ("Error allocating memory");
            exit(EXIT_FAILURE);
        }

        mpm_ctx->memory_cnt++;
        mpm_ctx->memory_size += (ctx->state_count *
                                 sizeof(SC_AC_STATE_TYPE_U16) * 256);

        StateQueue q;
        memset(&q, 0, sizeof(StateQueue));

        for (ascii_code = 0; ascii_code < 256; ascii_code++) {
            SC_AC_STATE_TYPE_U16 temp_state = ctx->goto_table[0][ascii_code];
            ctx->state_table_u16[0][ascii_code] = temp_state;
            if (temp_state != 0)
                SCACEnqueue(&q, temp_state);
        }

        while (!SCACStateQueueIsEmpty(&q)) {
            r_state = SCACDequeue(&q);

            for (ascii_code = 0; ascii_code < 256; ascii_code++) {
                int32_t temp_state = ctx->goto_table[r_state][ascii_code];
                if (temp_state != SC_AC_FAIL) {
                    SCACEnqueue(&q, temp_state);
                    ctx->state_table_u16[r_state][ascii_code] = temp_state;
                } else {
                    ctx->state_table_u16[r_state][ascii_code] =
                        ctx->state_table_u16[ctx->failure_table[r_state]][ascii_code];
                }
            }
        }
    }

    if (!(ctx->state_count < 32767) || construct_both_16_and_32_state_tables) {
        /* create space for the state table.  We could have used the existing goto
         * table, but since we have it set to hold 32 bit state values, we will create
         * a new state table here of type SC_AC_STATE_TYPE(current set to uint16_t) */
        ctx->state_table_u32 = kmalloc(ctx->state_count *
                                        sizeof(SC_AC_STATE_TYPE_U32) * 256, MPF_CLR, __oryx_unused_val__);
	ASSERT (ctx->state_table_u32);
        if (ctx->state_table_u32 == NULL) {
            printf ("Error allocating memory");
            exit(EXIT_FAILURE);
        }

        mpm_ctx->memory_cnt++;
        mpm_ctx->memory_size += (ctx->state_count *
                                 sizeof(SC_AC_STATE_TYPE_U32) * 256);

        StateQueue q;
        memset(&q, 0, sizeof(StateQueue));

        for (ascii_code = 0; ascii_code < 256; ascii_code++) {
            SC_AC_STATE_TYPE_U32 temp_state = ctx->goto_table[0][ascii_code];
            ctx->state_table_u32[0][ascii_code] = temp_state;
            if (temp_state != 0)
                SCACEnqueue(&q, temp_state);
        }

        while (!SCACStateQueueIsEmpty(&q)) {
            r_state = SCACDequeue(&q);

            for (ascii_code = 0; ascii_code < 256; ascii_code++) {
                int32_t temp_state = ctx->goto_table[r_state][ascii_code];
                if (temp_state != SC_AC_FAIL) {
                    SCACEnqueue(&q, temp_state);
                    ctx->state_table_u32[r_state][ascii_code] = temp_state;
                } else {
                    ctx->state_table_u32[r_state][ascii_code] =
                        ctx->state_table_u32[ctx->failure_table[r_state]][ascii_code];
                }
            }
        }
    }

    return;
}

static inline void SCACClubOutputStatePresenceWithDeltaTable(MpmCtx *mpm_ctx)
{
    SCACCtx *ctx = (SCACCtx *)mpm_ctx->ctx;
    int ascii_code = 0;
    uint32_t state = 0;
    uint32_t temp_state = 0;

    if ((ctx->state_count < 32767) || construct_both_16_and_32_state_tables) {
        for (state = 0; state < ctx->state_count; state++) {
            for (ascii_code = 0; ascii_code < 256; ascii_code++) {
                temp_state = ctx->state_table_u16[state & 0x7FFF][ascii_code];
                if (ctx->output_table[temp_state & 0x7FFF].no_of_entries != 0)
                    ctx->state_table_u16[state & 0x7FFF][ascii_code] |= (1 << 15);
            }
        }
    }

    if (!(ctx->state_count < 32767) || construct_both_16_and_32_state_tables) {
        for (state = 0; state < ctx->state_count; state++) {
            for (ascii_code = 0; ascii_code < 256; ascii_code++) {
                temp_state = ctx->state_table_u32[state & 0x00FFFFFF][ascii_code];
                if (ctx->output_table[temp_state & 0x00FFFFFF].no_of_entries != 0)
                    ctx->state_table_u32[state & 0x00FFFFFF][ascii_code] |= (1 << 24);
            }
        }
    }

    return;
}

static inline void SCACInsertCaseSensitiveEntriesForPatterns(MpmCtx *mpm_ctx)
{
    SCACCtx *ctx = (SCACCtx *)mpm_ctx->ctx;
    uint32_t state = 0;
    uint32_t k = 0;

    for (state = 0; state < ctx->state_count; state++) {
        if (ctx->output_table[state].no_of_entries == 0)
            continue;

        for (k = 0; k < ctx->output_table[state].no_of_entries; k++) {
            if (ctx->pid_pat_list[ctx->output_table[state].pids[k]].cs != NULL) {
                ctx->output_table[state].pids[k] &= AC_PID_MASK;
                ctx->output_table[state].pids[k] |= ((uint32_t)1 << AC_CASE_BIT);
            }
        }
    }

    return;
}

#if 0
static void SCACPrintDeltaTable(MpmCtx *mpm_ctx)
{
    SCACCtx *ctx = (SCACCtx *)mpm_ctx->ctx;
    int i = 0, j = 0;

    printf("##############Delta Table##############\n");
    for (i = 0; i < ctx->state_count; i++) {
        printf("%d: \n", i);
        for (j = 0; j < 256; j++) {
            if (SCACGetDelta(i, j, mpm_ctx) != 0) {
                printf("  %c -> %d\n", j, SCACGetDelta(i, j, mpm_ctx));
            }
        }
    }

    return;
}
#endif

/**
 * \brief Process the patterns and prepare the state table.
 *
 * \param mpm_ctx Pointer to the mpm context.
 */
static void SCACPrepareStateTable(MpmCtx *mpm_ctx)
{
    SCACCtx *ctx = (SCACCtx *)mpm_ctx->ctx;

    /* create the 0th state in the goto table and output_table */
    SCACInitNewState(mpm_ctx);

    SCACDetermineLevel1Gap(mpm_ctx);

    /* create the goto table */
    SCACCreateGotoTable(mpm_ctx);
    /* create the failure table */
    SCACCreateFailureTable(mpm_ctx);
    /* create the final state(delta) table */
    SCACCreateDeltaTable(mpm_ctx);
    /* club the output state presence with delta transition entries */
    SCACClubOutputStatePresenceWithDeltaTable(mpm_ctx);

    /* club nocase entries */
    SCACInsertCaseSensitiveEntriesForPatterns(mpm_ctx);

    /* shrink the memory */
    ACShrinkState(ctx);

#if 0
    SCACPrintDeltaTable(mpm_ctx);
#endif

    /* we don't need these anymore */
    kfree(ctx->goto_table);
    ctx->goto_table = NULL;
    kfree(ctx->failure_table);
    ctx->failure_table = NULL;

    return;
}

/**
 * \brief Process the patterns added to the mpm, and create the internal tables.
 *
 * \param mpm_ctx Pointer to the mpm context.
 */
int ACPreparePatterns(MpmCtx *mpm_ctx)
{
    SCACCtx *ctx = (SCACCtx *)mpm_ctx->ctx;

    if (mpm_ctx->pattern_cnt == 0 || mpm_ctx->init_hash == NULL) {
        printf("no patterns supplied to this mpm_ctx\n");
        return 0;
    }

    /* alloc the pattern array */
    ctx->parray = (MpmPattern **)kmalloc(mpm_ctx->pattern_cnt *
                                           sizeof(MpmPattern *), MPF_CLR, __oryx_unused_val__);
    if (ctx->parray == NULL)
        goto error;
   
    mpm_ctx->memory_cnt++;
    mpm_ctx->memory_size += (mpm_ctx->pattern_cnt * sizeof(MpmPattern *));

    /* populate it with the patterns in the hash */
    uint32_t i = 0, p = 0;
    for (i = 0; i < MPM_INIT_HASH_SIZE; i++) {
        MpmPattern *node = mpm_ctx->init_hash[i], *nnode = NULL;
        while(node != NULL) {
            nnode = node->next;
            node->next = NULL;
            ctx->parray[p++] = node;
            node = nnode;
        }
    }

    /* we no longer need the hash, so free it's memory */
    kfree(mpm_ctx->init_hash);
    mpm_ctx->init_hash = NULL;

    /* the memory consumed by a single state in our goto table */
    ctx->single_state_size = sizeof(int32_t) * 256;

    /* handle no case patterns */
    ctx->pid_pat_list = kmalloc((mpm_ctx->max_pat_id + 1)* sizeof(SCACPatternList), MPF_CLR, __oryx_unused_val__);
ASSERT (ctx->pid_pat_list);
    if (ctx->pid_pat_list == NULL) {
        printf( "Error allocating memory\n");
        exit(EXIT_FAILURE);
    }

    for (i = 0; i < mpm_ctx->pattern_cnt; i++) {
        if (!(ctx->parray[i]->flags & MPM_PATTERN_FLAG_NOCASE)) {
            ctx->pid_pat_list[ctx->parray[i]->id].cs = kmalloc(ctx->parray[i]->len, MPF_CLR, __oryx_unused_val__);
	    ASSERT (ctx->pid_pat_list[ctx->parray[i]->id].cs);
            if (ctx->pid_pat_list[ctx->parray[i]->id].cs == NULL) {
                printf( "Error allocating memory\n");
                exit(EXIT_FAILURE);
            }
            memcpy(ctx->pid_pat_list[ctx->parray[i]->id].cs,
                   ctx->parray[i]->original_pat, ctx->parray[i]->len);
            ctx->pid_pat_list[ctx->parray[i]->id].patlen = ctx->parray[i]->len;
        }

        /* ACPatternList now owns this memory */
        ctx->pid_pat_list[ctx->parray[i]->id].sids_size = ctx->parray[i]->sids_size;
        ctx->pid_pat_list[ctx->parray[i]->id].sids = ctx->parray[i]->sids;

        ctx->parray[i]->sids_size = 0;
        ctx->parray[i]->sids = NULL;
    }

    /* prepare the state table required by AC */
    SCACPrepareStateTable(mpm_ctx);

#ifdef __SC_CUDA_SUPPORT__
    if (mpm_ctx->mpm_type == MPM_AC_CUDA) {
        int r = SCCudaMemAlloc(&ctx->state_table_u32_cuda,
                               ctx->state_count * sizeof(unsigned int) * 256);
        if (r < 0) {
            printf( "SCCudaMemAlloc failure.\n");
            exit(EXIT_FAILURE);
        }

        r = SCCudaMemcpyHtoD(ctx->state_table_u32_cuda,
                             ctx->state_table_u32,
                             ctx->state_count * sizeof(unsigned int) * 256);
        if (r < 0) {
            printf( "SCCudaMemcpyHtoD failure.\n");
            exit(EXIT_FAILURE);
        }
    }
#endif

    printf("Built %" PRIu32 " patterns into a database of size %" PRIu32
               " bytes\n", mpm_ctx->pattern_cnt, mpm_ctx->memory_size);

    /* free all the stored patterns.  Should save us a good 100-200 mbs */
    for (i = 0; i < mpm_ctx->pattern_cnt; i++) {
        if (ctx->parray[i] != NULL) {
            MpmFreePattern(mpm_ctx, ctx->parray[i]);
        }
    }
    kfree(ctx->parray);
    ctx->parray = NULL;
    mpm_ctx->memory_cnt--;
    mpm_ctx->memory_size -= (mpm_ctx->pattern_cnt * sizeof(MpmPattern *));

    ctx->pattern_id_bitarray_size = (mpm_ctx->max_pat_id / 8) + 1;


#if 0
    printf("ctx->pattern_id_bitarray_size =%u, mpm_ctx->max_pat_id =%u\n", 
		ctx->pattern_id_bitarray_size,
		mpm_ctx->max_pat_id);
#endif

    return 0;

error:
    return -1;
}

/**
 * \brief Init the mpm thread context.
 *
 * \param mpm_ctx        Pointer to the mpm context.
 * \param mpm_thread_ctx Pointer to the mpm thread context.
 * \param matchsize      We don't need this.
 */
void ACInitThreadCtx(MpmCtx __oryx_unused__ *mpm_ctx, MpmThreadCtx *mpm_thread_ctx)
{
    memset(mpm_thread_ctx, 0, sizeof(MpmThreadCtx));

    mpm_thread_ctx->ctx = kmalloc(sizeof(SCACThreadCtx), MPF_CLR, __oryx_unused_val__);
    if (mpm_thread_ctx->ctx == NULL) {
        exit(EXIT_FAILURE);
    }

    mpm_thread_ctx->memory_cnt++;
    mpm_thread_ctx->memory_size += sizeof(SCACThreadCtx);

    return;
}

/**
 * \brief Initialize the AC context.
 *
 * \param mpm_ctx       Mpm context.
 */
void ACinitCtx(MpmCtx *mpm_ctx)
{
    if (mpm_ctx->ctx != NULL)
        return;

    mpm_ctx->ctx = kmalloc(sizeof(SCACCtx), MPF_CLR, __oryx_unused_val__);
    if (mpm_ctx->ctx == NULL) {
        exit(EXIT_FAILURE);
    }

    mpm_ctx->memory_cnt++;
    mpm_ctx->memory_size += sizeof(SCACCtx);

    /* initialize the hash we use to speed up pattern insertions */
    mpm_ctx->init_hash = kmalloc(sizeof(MpmPattern *) * MPM_INIT_HASH_SIZE, MPF_CLR, __oryx_unused_val__);
    if (mpm_ctx->init_hash == NULL) {
        exit(EXIT_FAILURE);
    }

    /* get conf values for AC from our yaml file.  We have no conf values for
     * now.  We will certainly need this, as we develop the algo */
    ACGetConfig();

    SCReturn;
}

/**
 * \brief Destroy the mpm thread context.
 *
 * \param mpm_ctx        Pointer to the mpm context.
 * \param mpm_thread_ctx Pointer to the mpm thread context.
 */
void ACDestroyThreadCtx(MpmCtx __oryx_unused__ *mpm_ctx, MpmThreadCtx *mpm_thread_ctx)
{
    ACPrintSearchStats(mpm_thread_ctx);

    if (mpm_thread_ctx->ctx != NULL) {
        kfree(mpm_thread_ctx->ctx);
        mpm_thread_ctx->ctx = NULL;
        mpm_thread_ctx->memory_cnt--;
        mpm_thread_ctx->memory_size -= sizeof(SCACThreadCtx);
    }

    return;
}

/**
 * \brief Destroy the mpm context.
 *
 * \param mpm_ctx Pointer to the mpm context.
 */
void ACDestroyCtx(MpmCtx *mpm_ctx)
{
    SCACCtx *ctx = (SCACCtx *)mpm_ctx->ctx;
    if (ctx == NULL)
        return;
	
    if (mpm_ctx->init_hash != NULL) {
        kfree(mpm_ctx->init_hash);
        mpm_ctx->init_hash = NULL;
        mpm_ctx->memory_cnt--;
        mpm_ctx->memory_size -= (MPM_INIT_HASH_SIZE * sizeof(MpmPattern *));
    }

    if (ctx->parray != NULL) {
        uint32_t i;
        for (i = 0; i < mpm_ctx->pattern_cnt; i++) {
            if (ctx->parray[i] != NULL) {
                MpmFreePattern(mpm_ctx, ctx->parray[i]);
            }
        }

        kfree(ctx->parray);
        ctx->parray = NULL;
        mpm_ctx->memory_cnt--;
        mpm_ctx->memory_size -= (mpm_ctx->pattern_cnt * sizeof(MpmPattern *));
    }

    if (ctx->state_table_u16 != NULL) {
        kfree(ctx->state_table_u16);
        ctx->state_table_u16 = NULL;

        mpm_ctx->memory_cnt++;
        mpm_ctx->memory_size -= (ctx->state_count *
                                 sizeof(SC_AC_STATE_TYPE_U16) * 256);
    }
    if (ctx->state_table_u32 != NULL) {
        kfree(ctx->state_table_u32);
        ctx->state_table_u32 = NULL;

        mpm_ctx->memory_cnt++;
        mpm_ctx->memory_size -= (ctx->state_count *
                                 sizeof(SC_AC_STATE_TYPE_U32) * 256);
    }

    if (ctx->output_table != NULL) {
        uint32_t state_count;
        for (state_count = 0; state_count < ctx->state_count; state_count++) {
            if (ctx->output_table[state_count].pids != NULL) {
                kfree(ctx->output_table[state_count].pids);
            }
        }
        kfree(ctx->output_table);
    }

    if (ctx->pid_pat_list != NULL) {
        uint32_t i;
        for (i = 0; i < (mpm_ctx->max_pat_id + 1); i++) {
            if (ctx->pid_pat_list[i].cs != NULL)
                kfree(ctx->pid_pat_list[i].cs);
            if (ctx->pid_pat_list[i].sids != NULL)
                kfree(ctx->pid_pat_list[i].sids);
        }
        kfree(ctx->pid_pat_list);
    }

    kfree(mpm_ctx->ctx);
    mpm_ctx->memory_cnt--;
    mpm_ctx->memory_size -= sizeof(SCACCtx);

    return;
}

/**
 * \brief The aho corasick search function.
 *
 * \param mpm_ctx        Pointer to the mpm context.
 * \param mpm_thread_ctx Pointer to the mpm thread context.
 * \param pmq            Pointer to the Pattern Matcher Queue to hold
 *                       search matches.
 * \param buf            Buffer to be searched.
 * \param buflen         Buffer length.
 *
 * \retval matches Match count.
 */

//#define BIT_ARRAY_ALLOC_STACK
uint8_t *bitarray_ptr;
#define BIT_ARRAY_DEFAULT_SIZE		102400
uint32_t ACSearch(MpmCtx *mpm_ctx, MpmThreadCtx __oryx_unused__ *mpm_thread_ctx,
                    PrefilterRuleStore *pmq, const uint8_t *buf, uint16_t buflen)
{
    const SCACCtx *ctx = (SCACCtx *)mpm_ctx->ctx;
    int i = 0;
    int matches = 0;

    /* \todo tried loop unrolling with register var, with no perf increase.  Need
     * to dig deeper */
    /* \todo Change it for stateful MPM.  Supply the state using mpm_thread_ctx */
    SCACPatternList *pid_pat_list = ctx->pid_pat_list;


#ifdef BIT_ARRAY_ALLOC_STACK
    uint8_t bitarray[ctx->pattern_id_bitarray_size];
    memset (bitarray, 0, ctx->pattern_id_bitarray_size);
#else
	uint8_t *bitarray = NULL;
#endif

/** TSIHANG */
#ifndef BIT_ARRAY_ALLOC_STACK 
	u32 size = BIT_ARRAY_DEFAULT_SIZE;

	bitarray = bitarray_ptr;
	if (bitarray) {		
		if (size < ctx->pattern_id_bitarray_size) {
			size = ctx->pattern_id_bitarray_size;
			bitarray_ptr = (uint8_t *) krealloc (bitarray_ptr, size, MPF_CLR, __oryx_unused_val__);
			ASSERT(bitarray_ptr);
			bitarray = bitarray_ptr;
		}
	} else {
		if (size < ctx->pattern_id_bitarray_size)
			size = ctx->pattern_id_bitarray_size;
		bitarray_ptr = (uint8_t *) kmalloc (size, MPF_CLR, __oryx_unused_val__);
		ASSERT (bitarray_ptr);
		bitarray = bitarray_ptr;
	}
	ASSERT (bitarray);
#endif

    if (ctx->state_count < 32767) {
        register SC_AC_STATE_TYPE_U16 state = 0;
        SC_AC_STATE_TYPE_U16 (*state_table_u16)[256] = ctx->state_table_u16;
        for (i = 0; i < buflen; i++) {
            state = state_table_u16[state & 0x7FFF][u8_tolower(buf[i])];
            if (state & 0x8000) {
                uint32_t no_of_entries = ctx->output_table[state & 0x7FFF].no_of_entries;
                uint32_t *pids = ctx->output_table[state & 0x7FFF].pids;
                uint32_t k;
                for (k = 0; k < no_of_entries; k++) {
                    if (pids[k] & AC_CASE_MASK) {
                        uint32_t lower_pid = pids[k] & AC_PID_MASK;
                        if (memcmp(pid_pat_list[lower_pid].cs,
                                     buf + i - pid_pat_list[lower_pid].patlen + 1,
                                     pid_pat_list[lower_pid].patlen) != 0) {
                            /* inside loop */
                            continue;
                        }
                        if (bitarray[(lower_pid) / 8] & (1 << ((lower_pid) % 8))) {
                            ;
                        } else {
                            bitarray[(lower_pid) / 8] |= (1 << ((lower_pid) % 8));
                            MpmAddSids(pmq, pid_pat_list[lower_pid].sids, pid_pat_list[lower_pid].sids_size);
                        }
                        matches++;
                    } else {
                        if (bitarray[pids[k] / 8] & (1 << (pids[k] % 8))) {
                            ;
                        } else {
                            bitarray[pids[k] / 8] |= (1 << (pids[k] % 8));
                            MpmAddSids(pmq, pid_pat_list[pids[k]].sids, pid_pat_list[pids[k]].sids_size);
                        }
                        matches++;
                    }
                    //loop1:
                    //;
                }
            }
        } /* for (i = 0; i < buflen; i++) */

    } else {
        register SC_AC_STATE_TYPE_U32 state = 0;
        SC_AC_STATE_TYPE_U32 (*state_table_u32)[256] = ctx->state_table_u32;
        for (i = 0; i < buflen; i++) {
            state = state_table_u32[state & 0x00FFFFFF][u8_tolower(buf[i])];
            if (state & 0xFF000000) {
                uint32_t no_of_entries = ctx->output_table[state & 0x00FFFFFF].no_of_entries;
                uint32_t *pids = ctx->output_table[state & 0x00FFFFFF].pids;
                uint32_t k;
                for (k = 0; k < no_of_entries; k++) {
                    if (pids[k] & AC_CASE_MASK) {
                        uint32_t lower_pid = pids[k] & 0x0000FFFF;
                        if (memcmp(pid_pat_list[lower_pid].cs,
                                     buf + i - pid_pat_list[lower_pid].patlen + 1,
                                     pid_pat_list[lower_pid].patlen) != 0) {
                            /* inside loop */
                            continue;
                        }
                        if (bitarray[(lower_pid) / 8] & (1 << ((lower_pid) % 8))) {
                            ;
                        } else {
                            bitarray[(lower_pid) / 8] |= (1 << ((lower_pid) % 8));
                            MpmAddSids(pmq, pid_pat_list[lower_pid].sids, pid_pat_list[lower_pid].sids_size);
                        }
                        matches++;
                    } else {
                        if (bitarray[pids[k] / 8] & (1 << (pids[k] % 8))) {
                            ;
                        } else {
                            bitarray[pids[k] / 8] |= (1 << (pids[k] % 8));
                            MpmAddSids(pmq, pid_pat_list[pids[k]].sids, pid_pat_list[pids[k]].sids_size);
                        }
                        matches++;
                    }
                    //loop1:
                    //;
                }
            }
        } /* for (i = 0; i < buflen; i++) */
    }

    return matches;
}

/**
 * \brief Add a case insensitive pattern.  Although we have different calls for
 *        adding case sensitive and insensitive patterns, we make a single call
 *        for either case.  No special treatment for either case.
 *
 * \param mpm_ctx Pointer to the mpm context.
 * \param pat     The pattern to add.
 * \param patnen  The pattern length.
 * \param offset  Ignored.
 * \param depth   Ignored.
 * \param pid     The pattern id.
 * \param sid     Ignored.
 * \param flags   Flags associated with this pattern.
 *
 * \retval  0 On success.
 * \retval -1 On failure.
 */
int ACAddPatternCI(MpmCtx *mpm_ctx, uint8_t *pat, uint16_t patlen,
                     uint16_t offset, uint16_t depth, uint32_t pid,
                     sig_id sid, uint8_t flags)
{
    flags |= MPM_PATTERN_FLAG_NOCASE;
    return MpmAddPattern(mpm_ctx, pat, patlen, offset, depth, pid, sid, flags);
}

/**
 * \brief Add a case sensitive pattern.  Although we have different calls for
 *        adding case sensitive and insensitive patterns, we make a single call
 *        for either case.  No special treatment for either case.
 *
 * \param mpm_ctx Pointer to the mpm context.
 * \param pat     The pattern to add.
 * \param patnen  The pattern length.
 * \param offset  Ignored.
 * \param depth   Ignored.
 * \param pid     The pattern id.
 * \param sid     Ignored.
 * \param flags   Flags associated with this pattern.
 *
 * \retval  0 On success.
 * \retval -1 On failure.
 */
int ACAddPatternCS(MpmCtx *mpm_ctx, uint8_t *pat, uint16_t patlen,
                     uint16_t offset, uint16_t depth, uint32_t pid,
                     sig_id sid, uint8_t flags)
{
    return MpmAddPattern(mpm_ctx, pat, patlen, offset, depth, pid, sid, flags);
}

void ACPrintSearchStats(MpmThreadCtx __oryx_unused__ *mpm_thread_ctx)
{

#ifdef SC_AC_COUNTERS
    SCACThreadCtx *ctx = (SCACThreadCtx *)mpm_thread_ctx->ctx;
    printf("AC Thread Search stats (ctx %p)\n", ctx);
    printf("Total calls: %" PRIu32 "\n", ctx->total_calls);
    printf("Total matches: %" PRIu64 "\n", ctx->total_matches);
#endif /* SC_AC_COUNTERS */

    return;
}

void ACPrintInfo(MpmCtx __oryx_unused__ *mpm_ctx)
{
    SCACCtx __oryx_unused__ *ctx = (SCACCtx *)mpm_ctx->ctx;
#if 1
    printf("MPM AC Information:\n");
    printf("Memory allocs:   %" PRIu32 "\n", mpm_ctx->memory_cnt);
    printf("Memory alloced:  %" PRIu32 "\n", mpm_ctx->memory_size);
    printf(" Sizeof:\n");
    printf("  MpmCtx         %" PRIuMAX "\n", (uintmax_t)sizeof(MpmCtx));
    printf("  SCACCtx:         %" PRIuMAX "\n", (uintmax_t)sizeof(SCACCtx));
    printf("  MpmPattern      %" PRIuMAX "\n", (uintmax_t)sizeof(MpmPattern));
    printf("  MpmPattern     %" PRIuMAX "\n", (uintmax_t)sizeof(MpmPattern));
    printf("Unique Patterns: %" PRIu32 "\n", mpm_ctx->pattern_cnt);
    printf("Smallest:        %" PRIu32 "\n", mpm_ctx->minlen);
    printf("Largest:         %" PRIu32 "\n", mpm_ctx->maxlen);
    printf("Total states in the state table:    %" PRIu32 "\n", ctx->state_count);
    printf("\n");
#endif

    return;
}

/****************************Cuda side of things****************************/

#ifdef __SC_CUDA_SUPPORT__

/* \todo Technically it's generic to all mpms, but since we use ac only, the
 *       code internally directly references ac and hence it has found its
 *       home in this file, instead of util-mpm.c
 */
void DetermineCudaStateTableSize(DetectEngineCtx *de_ctx)
{
    MpmCtx *mpm_ctx = NULL;

    int ac_16_tables = 0;
    int ac_32_tables = 0;

    mpm_ctx = MpmFactoryGetMpmCtxForProfile(de_ctx, de_ctx->sgh_mpm_context_proto_tcp_packet, 0);
    if (mpm_ctx->mpm_type == MPM_AC_CUDA) {
        SCACCtx *ctx = (SCACCtx *)mpm_ctx->ctx;
        if (ctx->state_count < 32767)
            ac_16_tables++;
        else
            ac_32_tables++;
    }
    mpm_ctx = MpmFactoryGetMpmCtxForProfile(de_ctx, de_ctx->sgh_mpm_context_proto_tcp_packet, 1);
    if (mpm_ctx->mpm_type == MPM_AC_CUDA) {
        SCACCtx *ctx = (SCACCtx *)mpm_ctx->ctx;
        if (ctx->state_count < 32767)
            ac_16_tables++;
        else
            ac_32_tables++;
    }

    mpm_ctx = MpmFactoryGetMpmCtxForProfile(de_ctx, de_ctx->sgh_mpm_context_proto_udp_packet, 0);
    if (mpm_ctx->mpm_type == MPM_AC_CUDA) {
        SCACCtx *ctx = (SCACCtx *)mpm_ctx->ctx;
        if (ctx->state_count < 32767)
            ac_16_tables++;
        else
            ac_32_tables++;
    }
    mpm_ctx = MpmFactoryGetMpmCtxForProfile(de_ctx, de_ctx->sgh_mpm_context_proto_udp_packet, 1);
    if (mpm_ctx->mpm_type == MPM_AC_CUDA) {
        SCACCtx *ctx = (SCACCtx *)mpm_ctx->ctx;
        if (ctx->state_count < 32767)
            ac_16_tables++;
        else
            ac_32_tables++;
    }

    mpm_ctx = MpmFactoryGetMpmCtxForProfile(de_ctx, de_ctx->sgh_mpm_context_proto_other_packet, 0);
    if (mpm_ctx->mpm_type == MPM_AC_CUDA) {
        SCACCtx *ctx = (SCACCtx *)mpm_ctx->ctx;
        if (ctx->state_count < 32767)
            ac_16_tables++;
        else
            ac_32_tables++;
    }

    if (ac_16_tables > 0 && ac_32_tables > 0)
        SCACConstructBoth16and32StateTables();

    printf("Total mpm ac 16 bit state tables - %d\n", ac_16_tables);
    printf("Total mpm ac 32 bit state tables - %d\n", ac_32_tables);

}

void CudaReleasePacket(Packet *p)
{
    if (p->cuda_pkt_vars.cuda_mpm_enabled == 1) {
        p->cuda_pkt_vars.cuda_mpm_enabled = 0;
        do_lock(&p->cuda_pkt_vars.cuda_mutex);
        p->cuda_pkt_vars.cuda_done = 0;
        do_unlock(&p->cuda_pkt_vars.cuda_mutex);
    }

    return;
}

/* \todos
 * - Use texture memory - Can we fit all the arrays into a 3d texture.
 *   Texture memory definitely offers slightly better performance even
 *   on gpus that offer cache for global memory.
 * - Packetpool - modify to support > 65k max pending packets.  We are
 *   hitting packetpool limit currently even with 65k packets.
 * - Use streams.  We have tried overlapping parsing results from the
 *   previous call with invoking the next call.
 * - Offer higher priority to decode threads.
 * - Modify pcap file mode to support reading from multiple pcap files
 *   and hence we will have multiple receive threads.
 * - Split state table into many small pieces and have multiple threads
 *   run each small state table on the same payload.
 * - Used a config peference of l1 over shared memory with no noticeable
 *   perf increase.  Explore it in detail over cards/architectures.
 * - Constant memory performance sucked.  Explore it in detail.
 * - Currently all our state tables are small.  Implement 16 bit state
 *   tables on priority.
 * - Introduce profiling.
 * - Retrieve sgh before buffer packet.
 * - Buffer smsgs too.
 */

void SCACConstructBoth16and32StateTables(void)
{
    construct_both_16_and_32_state_tables = 1;

    return;
}

/* \todo Reduce offset buffer size.  Probably a 100,000 entry would be sufficient. */
static void *SCACCudaDispatcher(void *arg)
{
#define BLOCK_SIZE 32

    int r = 0;
    ThreadVars *tv = (ThreadVars *)arg;
    MpmCudaConf *conf = CudaHandlerGetCudaProfile("mpm");
    uint32_t sleep_interval_ms = conf->batching_timeout;

    printf("AC Cuda Mpm Dispatcher using a timeout of "
              "\"%"PRIu32"\" micro-seconds", sleep_interval_ms);

    CudaBufferData *cb_data =
        CudaHandlerModuleGetData(MPM_AC_CUDA_MODULE_NAME,
                                 MPM_AC_CUDA_MODULE_CUDA_BUFFER_NAME);

    CUcontext cuda_context =
        CudaHandlerModuleGetContext(MPM_AC_CUDA_MODULE_NAME, conf->device_id);
    if (cuda_context == 0) {
        printf( "context is NULL.");
        exit(EXIT_FAILURE);
    }
    r = SCCudaCtxPushCurrent(cuda_context);
    if (r < 0) {
        printf( "context push failed.");
        exit(EXIT_FAILURE);
    }
    CUmodule cuda_module = 0;
    if (CudaHandlerGetCudaModule(&cuda_module, "util-mpm-ac-cuda-kernel") < 0) {
        printf( "Error retrieving cuda module.");
        exit(EXIT_FAILURE);
    }
    CUfunction kernel = 0;
#if __WORDSIZE==64
    if (SCCudaModuleGetFunction(&kernel, cuda_module, "SCACCudaSearch64") == -1) {
        printf( "Error retrieving kernel");
        exit(EXIT_FAILURE);
    }
#else
    if (SCCudaModuleGetFunction(&kernel, cuda_module, "SCACCudaSearch32") == -1) {
        printf( "Error retrieving kernel");
        exit(EXIT_FAILURE);
    }
#endif

    uint8_t g_u8_lowercasetable[256];
    for (int c = 0; c < 256; c++)
        g_u8_lowercasetable[c] = tolower((uint8_t)c);
    CUdeviceptr cuda_g_u8_lowercasetable_d = 0;
    CUdeviceptr cuda_packets_buffer_d = 0;
    CUdeviceptr cuda_offset_buffer_d = 0;
    CUdeviceptr cuda_results_buffer_d = 0;
    uint32_t *cuda_results_buffer_h = NULL;
    r = SCCudaMemAlloc(&cuda_g_u8_lowercasetable_d, sizeof(g_u8_lowercasetable));
    if (r < 0) {
        printf( "SCCudaMemAlloc failure.");
        exit(EXIT_FAILURE);
    }
    r = SCCudaMemcpyHtoD(cuda_g_u8_lowercasetable_d, g_u8_lowercasetable, sizeof(g_u8_lowercasetable));
    if (r < 0) {
        printf( "SCCudaMemcpyHtoD failure.");
        exit(EXIT_FAILURE);
    }
    r = SCCudaMemAlloc(&cuda_packets_buffer_d, conf->gpu_transfer_size);
    if (r < 0) {
        printf( "SCCudaMemAlloc failure.");
        exit(EXIT_FAILURE);
    }
    r = SCCudaMemAlloc(&cuda_offset_buffer_d, conf->gpu_transfer_size * 4);
    if (r < 0) {
        printf( "SCCudaMemAlloc failure.");
        exit(EXIT_FAILURE);
    }
    r = SCCudaMemAlloc(&cuda_results_buffer_d, conf->gpu_transfer_size * 8);
    if (r < 0) {
        printf( "SCCudaMemAlloc failure.");
        exit(EXIT_FAILURE);
    }
    r = SCCudaMemAllocHost((void **)&cuda_results_buffer_h, conf->gpu_transfer_size * 8);
    if (r < 0) {
        printf( "SCCudaMemAlloc failure.");
        exit(EXIT_FAILURE);
    }

    CudaBufferCulledInfo cb_culled_info;
    memset(&cb_culled_info, 0, sizeof(cb_culled_info));

    TmThreadsSetFlag(tv, THV_INIT_DONE);
    while (1) {
        if (TmThreadsCheckFlag(tv, THV_KILL))
            break;

        usleep(sleep_interval_ms);

        /**************** 1 SEND ****************/
        CudaBufferCullCompletedSlices(cb_data, &cb_culled_info, conf->gpu_transfer_size);
        if (cb_culled_info.no_of_items == 0)
            continue;
#if 0
        printf("1 - cb_culled_info.no_of_items-%"PRIu32" "
                  "cb_culled_info.buffer_len - %"PRIu32" "
                  "cb_culled_info.average size - %f "
                  "cb_culled_info.d_buffer_start_offset - %"PRIu32" "
                  "cb_culled_info.op_buffer_start_offset - %"PRIu32" "
                  "cb_data.no_of_items - %"PRIu32"  "
                  "cb_data.d_buffer_read - %"PRIu32" "
                  "cb_data.d_buffer_write - %"PRIu32" "
                  "cb_data.op_buffer_read - %"PRIu32" "
                  "cb_data.op_buffer_write - %"PRIu32"\n",
                  cb_culled_info.no_of_items,
                  cb_culled_info.d_buffer_len,
                  cb_culled_info.d_buffer_len / (float)cb_culled_info.no_of_items,
                  cb_culled_info.d_buffer_start_offset,
                  cb_culled_info.op_buffer_start_offset,
                  cb_data->no_of_items,
                  cb_data->d_buffer_read,
                  cb_data->d_buffer_write,
                  cb_data->op_buffer_read,
                  cb_data->op_buffer_write);
#endif
        r = SCCudaMemcpyHtoDAsync(cuda_packets_buffer_d, (cb_data->d_buffer + cb_culled_info.d_buffer_start_offset), cb_culled_info.d_buffer_len, 0);
        if (r < 0) {
            printf( "SCCudaMemcpyHtoD failure.");
            exit(EXIT_FAILURE);
        }
        r = SCCudaMemcpyHtoDAsync(cuda_offset_buffer_d, (cb_data->o_buffer + cb_culled_info.op_buffer_start_offset), sizeof(uint32_t) * cb_culled_info.no_of_items, 0);
        if (r < 0) {
            printf( "SCCudaMemcpyHtoD failure.");
            exit(EXIT_FAILURE);
        }
        void *args[] = { &cuda_packets_buffer_d,
                         &cb_culled_info.d_buffer_start_offset,
                         &cuda_offset_buffer_d,
                         &cuda_results_buffer_d,
                         &cb_culled_info.no_of_items,
                         &cuda_g_u8_lowercasetable_d };
        r = SCCudaLaunchKernel(kernel,
                               (cb_culled_info.no_of_items / BLOCK_SIZE) + 1, 1, 1,
                               BLOCK_SIZE, 1, 1,
                               0, 0,
                               args, NULL);
        if (r < 0) {
            printf( "SCCudaLaunchKernel failure.");
            exit(EXIT_FAILURE);
        }
        r = SCCudaMemcpyDtoHAsync(cuda_results_buffer_h, cuda_results_buffer_d, sizeof(uint32_t) * (cb_culled_info.d_buffer_len * 2), 0);
        if (r < 0) {
            printf( "SCCudaMemcpyDtoH failure.");
            exit(EXIT_FAILURE);
        }



        /**************** 1 SYNCHRO ****************/
        r = SCCudaCtxSynchronize();
        if (r < 0) {
            printf( "SCCudaCtxSynchronize failure.");
            exit(EXIT_FAILURE);
        }

        /************* 1 Parse Results ************/
        uint32_t i_op_start_offset = cb_culled_info.op_buffer_start_offset;
        uint32_t no_of_items = cb_culled_info.no_of_items;
        uint32_t *o_buffer = cb_data->o_buffer;
        uint32_t d_buffer_start_offset = cb_culled_info.d_buffer_start_offset;
        for (uint32_t i = 0; i < no_of_items; i++, i_op_start_offset++) {
            Packet *p = (Packet *)cb_data->p_buffer[i_op_start_offset];

            do_lock(&p->cuda_pkt_vars.cuda_mutex);
            if (p->cuda_pkt_vars.cuda_mpm_enabled == 0) {
                p->cuda_pkt_vars.cuda_done = 0;
                do_unlock(&p->cuda_pkt_vars.cuda_mutex);
                continue;
            }

            p->cuda_pkt_vars.cuda_gpu_matches =
                cuda_results_buffer_h[((o_buffer[i_op_start_offset] - d_buffer_start_offset) * 2)];
            if (p->cuda_pkt_vars.cuda_gpu_matches != 0) {
                memcpy(p->cuda_pkt_vars.cuda_results,
                       cuda_results_buffer_h +
                       ((o_buffer[i_op_start_offset] - d_buffer_start_offset) * 2),
                       (cuda_results_buffer_h[((o_buffer[i_op_start_offset] -
                                                d_buffer_start_offset) * 2)] * sizeof(uint32_t)) + 4);
            }

            p->cuda_pkt_vars.cuda_done = 1;
            do_unlock(&p->cuda_pkt_vars.cuda_mutex);
            do_cond_signal(&p->cuda_pkt_vars.cuda_cond);
        }
        if (no_of_items != 0)
            CudaBufferReportCulledConsumption(cb_data, &cb_culled_info);
    } /* while (1) */

    r = SCCudaModuleUnload(cuda_module);
    if (r < 0) {
        printf( "Error unloading cuda module.");
        exit(EXIT_FAILURE);
    }
    r = SCCudaMemFree(cuda_packets_buffer_d);
    if (r < 0) {
        printf( "Error freeing cuda device memory.");
        exit(EXIT_FAILURE);
    }
    r = SCCudaMemFree(cuda_offset_buffer_d);
    if (r < 0) {
        printf( "Error freeing cuda device memory.");
        exit(EXIT_FAILURE);
    }
    r = SCCudaMemFree(cuda_results_buffer_d);
    if (r < 0) {
        printf( "Error freeing cuda device memory.");
        exit(EXIT_FAILURE);
    }
    r = SCCudaMemFreeHost(cuda_results_buffer_h);
    if (r < 0) {
        printf( "Error freeing cuda host memory.");
        exit(EXIT_FAILURE);
    }

    TmThreadsSetFlag(tv, THV_RUNNING_DONE);
    TmThreadWaitForFlag(tv, THV_DEINIT);
    TmThreadsSetFlag(tv, THV_CLOSED);

    return NULL;

#undef BLOCK_SIZE
}

uint32_t SCACCudaPacketResultsProcessing(Packet *p, const MpmCtx *mpm_ctx,
                                         PrefilterRuleStore *pmq)
{
    uint32_t u = 0;

    while (!p->cuda_pkt_vars.cuda_done) {
        do_lock(&p->cuda_pkt_vars.cuda_mutex);
        if (p->cuda_pkt_vars.cuda_done) {
            do_unlock(&p->cuda_pkt_vars.cuda_mutex);
            break;
        } else {
            do_cond_wait(&p->cuda_pkt_vars.cuda_cond, &p->cuda_pkt_vars.cuda_mutex);
            do_unlock(&p->cuda_pkt_vars.cuda_mutex);
        }
    } /* while */
    p->cuda_pkt_vars.cuda_done = 0;
    p->cuda_pkt_vars.cuda_mpm_enabled = 0;

    uint32_t cuda_matches = p->cuda_pkt_vars.cuda_gpu_matches;
    if (cuda_matches == 0)
        return 0;

    uint32_t matches = 0;
    uint32_t *results = p->cuda_pkt_vars.cuda_results + 1;
    uint8_t *buf = p->payload;
    SCACCtx *ctx = mpm_ctx->ctx;
    SCACOutputTable *output_table = ctx->output_table;
    SCACPatternList *pid_pat_list = ctx->pid_pat_list;

    uint8_t bitarray[ctx->pattern_id_bitarray_size];
    memset(bitarray, 0, ctx->pattern_id_bitarray_size);

    for (u = 0; u < cuda_matches; u += 2) {
        uint32_t offset = results[u];
        uint32_t state = results[u + 1];
        /* we should technically be doing state & 0x00FFFFFF, but we don't
         * since the cuda kernel does that for us */
        uint32_t no_of_entries = output_table[state].no_of_entries;
        /* we should technically be doing state & 0x00FFFFFF, but we don't
         * since the cuda kernel does that for us */
        uint32_t *pids = output_table[state].pids;
        uint32_t k;
        /* note that this is not a verbatim copy from ACSearch().  We
         * don't copy the pattern id into the pattern_id_array.  That's
         * the only change */
        for (k = 0; k < no_of_entries; k++) {
            if (pids[k] & AC_CASE_MASK) {
                uint32_t lower_pid = pids[k] & 0x0000FFFF;
                if (memcmp(pid_pat_list[lower_pid].cs,
                             buf + offset - pid_pat_list[lower_pid].patlen + 1,
                             pid_pat_list[lower_pid].patlen) != 0) {
                    /* inside loop */
                    continue;
                }
                if (bitarray[(lower_pid) / 8] & (1 << ((lower_pid) % 8))) {
                    ;
                } else {
                    bitarray[(lower_pid) / 8] |= (1 << ((lower_pid) % 8));
                    MpmAddSids(pmq, pid_pat_list[lower_pid].sids, pid_pat_list[lower_pid].sids_size);
                }
                matches++;
            } else {
                if (bitarray[pids[k] / 8] & (1 << (pids[k] % 8))) {
                    ;
                } else {
                    bitarray[pids[k] / 8] |= (1 << (pids[k] % 8));
                    MpmAddSids(pmq, pid_pat_list[pids[k]].sids, pid_pat_list[pids[k]].sids_size);
                }
                matches++;
            }
        }
    }

    return matches;
}

void SCACCudaStartDispatcher(void)
{
    /* create the threads */
    ThreadVars *tv = TmThreadCreate("Cuda_Mpm_AC_Dispatcher",
                                    NULL, NULL,
                                    NULL, NULL,
                                    "custom", SCACCudaDispatcher, 0);
    if (tv == NULL) {
        oryx_log_error(SC_ERR_THREAD_CREATE, "Error creating a thread for "
                   "ac cuda dispatcher.  Killing engine.");
        exit(EXIT_FAILURE);
    }
    if (TmThreadSpawn(tv) != 0) {
        oryx_log_error(SC_ERR_THREAD_SPAWN, "Failed to spawn thread for "
                   "ac cuda dispatcher.  Killing engine.");
        exit(EXIT_FAILURE);
    }

    return;
}

int MpmCudaBufferSetup(void)
{
    int r = 0;
    MpmCudaConf *conf = CudaHandlerGetCudaProfile("mpm");
    if (conf == NULL) {
        printf( "Error obtaining cuda mpm profile.");
        return -1;
    }

    CUcontext cuda_context = CudaHandlerModuleGetContext(MPM_AC_CUDA_MODULE_NAME, conf->device_id);
    if (cuda_context == 0) {
        printf( "Error retrieving cuda context.");
        return -1;
    }
    r = SCCudaCtxPushCurrent(cuda_context);
    if (r < 0) {
        printf( "Error pushing cuda context.");
        return -1;
    }

    uint8_t *d_buffer = NULL;
    uint32_t *o_buffer = NULL;
    void **p_buffer = NULL;

    r = SCCudaMemAllocHost((void *)&d_buffer, conf->cb_buffer_size);
    if (r < 0) {
        printf( "Cuda alloc host failure.");
        return -1;
    }
    printf("Allocated a cuda d_buffer - %"PRIu32" bytes", conf->cb_buffer_size);
    r = SCCudaMemAllocHost((void *)&o_buffer, sizeof(uint32_t) * UTIL_MPM_CUDA_CUDA_BUFFER_OPBUFFER_ITEMS_DEFAULT);
    if (r < 0) {
        printf( "Cuda alloc host failue.");
        return -1;
    }
    r = SCCudaMemAllocHost((void *)&p_buffer, sizeof(void *) * UTIL_MPM_CUDA_CUDA_BUFFER_OPBUFFER_ITEMS_DEFAULT);
    if (r < 0) {
        printf( "Cuda alloc host failure.");
        return -1;
    }

    r = SCCudaCtxPopCurrent(NULL);
    if (r < 0) {
        printf( "cuda context pop failure.");
        return -1;
    }

    CudaBufferData *cb = CudaBufferRegisterNew(d_buffer, conf->cb_buffer_size, o_buffer, p_buffer, UTIL_MPM_CUDA_CUDA_BUFFER_OPBUFFER_ITEMS_DEFAULT);
    if (cb == NULL) {
        printf( "Error registering new cb instance.");
        return -1;
    }
    CudaHandlerModuleStoreData(MPM_AC_CUDA_MODULE_NAME, MPM_AC_CUDA_MODULE_CUDA_BUFFER_NAME, cb);

    return 0;
}

int MpmCudaBufferDeSetup(void)
{
    int r = 0;
    MpmCudaConf *conf = CudaHandlerGetCudaProfile("mpm");
    if (conf == NULL) {
        printf( "Error obtaining cuda mpm profile.");
        return -1;
    }

    CudaBufferData *cb_data = CudaHandlerModuleGetData(MPM_AC_CUDA_MODULE_NAME, MPM_AC_CUDA_MODULE_CUDA_BUFFER_NAME);
    BUG_ON(cb_data == NULL);

    CUcontext cuda_context = CudaHandlerModuleGetContext(MPM_AC_CUDA_MODULE_NAME, conf->device_id);
    if (cuda_context == 0) {
        printf( "Error retrieving cuda context.");
        return -1;
    }
    r = SCCudaCtxPushCurrent(cuda_context);
    if (r < 0) {
        printf( "Error pushing cuda context.");
        return -1;
    }

    r = SCCudaMemFreeHost(cb_data->d_buffer);
    if (r < 0) {
        printf( "Error freeing cuda host memory.");
        return -1;
    }
    r = SCCudaMemFreeHost(cb_data->o_buffer);
    if (r < 0) {
        printf( "Error freeing cuda host memory.");
        return -1;
    }
    r = SCCudaMemFreeHost(cb_data->p_buffer);
    if (r < 0) {
        printf( "Error freeing cuda host memory.");
        return -1;
    }

    r = SCCudaCtxPopCurrent(NULL);
    if (r < 0) {
        printf( "cuda context pop failure.");
        return -1;
    }

    CudaBufferDeRegister(cb_data);

    return 0;
}

#endif /* __SC_CUDA_SUPPORT */

/************************** Mpm Registration ***************************/

/**
 * \brief Register the aho-corasick mpm.
 */
void MpmACRegister(void)
{
    mpm_table[MPM_AC].name = "ac";
    mpm_table[MPM_AC].InitCtx = ACinitCtx;
    mpm_table[MPM_AC].InitThreadCtx = ACInitThreadCtx;
    mpm_table[MPM_AC].DestroyCtx = ACDestroyCtx;
    mpm_table[MPM_AC].DestroyThreadCtx = ACDestroyThreadCtx;
    mpm_table[MPM_AC].AddPattern = ACAddPatternCS;
    mpm_table[MPM_AC].AddPatternNocase = ACAddPatternCI;
    mpm_table[MPM_AC].Prepare = ACPreparePatterns;
    mpm_table[MPM_AC].Search = ACSearch;
    mpm_table[MPM_AC].Cleanup = NULL;
    mpm_table[MPM_AC].PrintCtx = ACPrintInfo;
    mpm_table[MPM_AC].PrintThreadCtx = ACPrintSearchStats;
    mpm_table[MPM_AC].RegisterUnittests = NULL;

    return;
}


