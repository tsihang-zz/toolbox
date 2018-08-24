#include "oryx.h"
#include "mpm_ac.h"

//#define BIT_ARRAY_ALLOC_STACK
uint8_t *bitarray_ptr;
#define BIT_ARRAY_DEFAULT_SIZE		102400

/* a placeholder to denote a failure transition in the goto table */
#define AC_FAIL (-1)

#define STATE_QUEUE_CONTAINER_SIZE 65536

#define AC_CASE_MASK    0x80000000
#define AC_PID_MASK     0x7FFFFFFF
#define AC_CASE_BIT     31

static int construct_both_16_and_32_state_tables = 0;

/**
 * \brief Helper structure used by AC during state table creation
 */
typedef struct ac_state_queue_t_ {
    int32_t store[STATE_QUEUE_CONTAINER_SIZE];
    int top;
    int bot;
} ac_state_queue_t;

/**
 * \internal
 * \brief Initialize the AC context with user specified conf parameters.  We
 *        aren't retrieving anything for AC conf now, but we will certainly
 *        need it, when we customize AC.
 */
static void ac_get_config (void)
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
static __oryx_always_inline__
int ac_state_realloc(ac_ctx_t *ctx, uint32_t cnt)
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
        fprintf (stdout, "Error allocating memory (%u, %u, %lu)\n", cnt, ctx->single_state_size, size);
	
        exit(EXIT_FAILURE);
    }
    ctx->goto_table = ptmp;

    /* reallocate space in the output table for the new state */
    int oldsize = ctx->state_count * sizeof(ac_output_table_t);
    size = cnt * sizeof(ac_output_table_t);
#ifdef MPM_DEBUG
    fprintf (stdout, "oldsize %d size %d cnt %u ctx->state_count %u\n",
            oldsize, size, cnt, ctx->state_count);
#endif
    ptmp = krealloc(ctx->output_table, size, MPF_NOFLGS, __oryx_unused_val__);
ASSERT (ptmp);
    if (ptmp == NULL) {
        kfree(ctx->output_table);
        ctx->output_table = NULL;
	fprintf (stdout, "Error allocating memory (%lu)\n", size);
        exit(EXIT_FAILURE);
    }
    ctx->output_table = ptmp;

    memset(((uint8_t *)ctx->output_table + oldsize), 0, (size - oldsize));

    /* \todo using it temporarily now during dev, since I have restricted
     *       state var in ac_ctx_t->state_table to uint16_t. */
    //if (ctx->state_count > 65536) {
    //    fprintf (stdout, "state count exceeded\n");
    //    exit(EXIT_FAILURE);
    //}

    return 0;//ctx->state_count++;
}

/** \internal
 *  \brief Shrink state after setup is done
 *
 *  Shrinks only the output table, goto table is freed after calling this
 */
static void ac_state_shrink(ac_ctx_t *ctx)
{
    /* reallocate space in the output table for the new state */
#ifdef MPM_DEBUG
    int oldsize = ctx->allocated_state_count * sizeof(ac_output_table_t);
#endif
    int newsize = ctx->state_count * sizeof(ac_output_table_t);

#ifdef MPM_DEBUG
    fprintf (stdout, "oldsize %d newsize %d ctx->allocated_state_count %u "
               "ctx->state_count %u: shrink by %d bytes\n", oldsize,
               newsize, ctx->allocated_state_count, ctx->state_count,
               oldsize - newsize);
#else
    fprintf (stdout, "newsize %d ctx->allocated_state_count %u "
               "ctx->state_count %u\n",
               newsize, ctx->allocated_state_count, ctx->state_count);
#endif

    void *ptmp = krealloc(ctx->output_table, newsize, MPF_NOFLGS, __oryx_unused_val__);
ASSERT (ptmp);
    if (ptmp == NULL) {
        kfree(ctx->output_table);
        ctx->output_table = NULL;
	fprintf (stdout, "Error allocating memory (%d)\n", newsize);
        exit(EXIT_FAILURE);
    }
    ctx->output_table = ptmp;
}

static __oryx_always_inline__ int ac_state_init(mpm_ctx_t *mpm_ctx)
{
    ac_ctx_t *ctx = (ac_ctx_t *)mpm_ctx->ctx;;

    /* Exponentially increase the allocated space when needed. */
    if (ctx->allocated_state_count < ctx->state_count + 1) {
        if (ctx->allocated_state_count == 0)
            ctx->allocated_state_count = 256;
        else
            ctx->allocated_state_count *= 2;

        ac_state_realloc(ctx, ctx->allocated_state_count);

    }
#if 0
    if (ctx->allocated_state_count > 260) {
        ac_output_table_t *output_state = &ctx->output_table[260];
        fprintf (stdout, "output_state %p %p %u", output_state, output_state->pids, output_state->no_of_entries);
    }
#endif
    int ascii_code = 0;
    /* set all transitions for the newly assigned state as FAIL transitions */
    for (ascii_code = 0; ascii_code < 256; ascii_code++) {
        ctx->goto_table[ctx->state_count][ascii_code] = AC_FAIL;
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
static void SCACSetOutputState(int32_t state, uint32_t pid, mpm_ctx_t *mpm_ctx)
{
    void *ptmp;
    ac_ctx_t *ctx = (ac_ctx_t *)mpm_ctx->ctx;
    ac_output_table_t *output_state = &ctx->output_table[state];
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
	fprintf (stdout, "Error allocating memory (%lu)\n", output_state->no_of_entries * sizeof(uint32_t));
        exit(EXIT_FAILURE);
    }
    output_state->pids = ptmp;

    output_state->pids[output_state->no_of_entries - 1] = pid;

    return;
}

/**
 * \brief Helper function used by ac_create_goto_table.  Adds a pattern to the
 *        goto table.
 *
 * \param pattern     Pointer to the pattern.
 * \param pattern_len Pattern length.
 * \param pid         The pattern id, that corresponds to this pattern.  We
 *                    need it to updated the output table for this pattern.
 * \param mpm_ctx     Pointer to the mpm context.
 */
static __oryx_always_inline__
void SCACEnter(uint8_t *pattern, uint16_t pattern_len, uint32_t pid,
                             mpm_ctx_t *mpm_ctx)
{
    ac_ctx_t *ctx = (ac_ctx_t *)mpm_ctx->ctx;
    int32_t state = 0;
    int32_t newstate = 0;
    int i = 0;
    int p = 0;

    /* walk down the trie till we have a match for the pattern prefix */
    state = 0;
    for (i = 0; i < pattern_len; i++) {
        if (ctx->goto_table[state][pattern[i]] != AC_FAIL) {
            state = ctx->goto_table[state][pattern[i]];
        } else {
            break;
        }
    }

    /* add the non-matching pattern suffix to the trie, from the last state
     * we left off */
    for (p = i; p < pattern_len; p++) {
        newstate = ac_state_init(mpm_ctx);
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
static __oryx_always_inline__
void ac_create_goto_table(mpm_ctx_t *mpm_ctx)
{
    ac_ctx_t *ctx = (ac_ctx_t *)mpm_ctx->ctx;
    uint32_t i = 0;

    /* add each pattern to create the goto table */
    for (i = 0; i < mpm_ctx->pattern_cnt; i++) {
        SCACEnter(ctx->parray[i]->ci, ctx->parray[i]->len,
                  ctx->parray[i]->id, mpm_ctx);
    }

    int ascii_code = 0;
    for (ascii_code = 0; ascii_code < 256; ascii_code++) {
        if (ctx->goto_table[0][ascii_code] == AC_FAIL) {
            ctx->goto_table[0][ascii_code] = 0;
        }
    }

    return;
}

static __oryx_always_inline__
void SCACDetermineLevel1Gap(mpm_ctx_t *mpm_ctx)
{
    ac_ctx_t *ctx = (ac_ctx_t *)mpm_ctx->ctx;
    uint32_t u = 0;

    int map[256];
    memset(map, 0, sizeof(map));

    for (u = 0; u < mpm_ctx->pattern_cnt; u++)
        map[ctx->parray[u]->ci[0]] = 1;

    for (u = 0; u < 256; u++) {
        if (map[u] == 0)
            continue;
        int32_t newstate = ac_state_init(mpm_ctx);
        ctx->goto_table[0][u] = newstate;
    }

    return;
}

static __oryx_always_inline__
int ac_state_queue_is_empty(ac_state_queue_t *q)
{
    if (q->top == q->bot)
        return 1;
    else
        return 0;
}

static __oryx_always_inline__
void ac_state_enqueue(ac_state_queue_t *q, int32_t state)
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
        fprintf (stdout, "Just ran out of space in the queue.  "
                      "Fatal Error.  Exiting.  Please file a bug report on this");
        exit(EXIT_FAILURE);
    }

    return;
}

static __oryx_always_inline__
int32_t ac_state_dequeue(ac_state_queue_t *q)
{
    if (q->bot == STATE_QUEUE_CONTAINER_SIZE)
        q->bot = 0;

    if (q->bot == q->top) {
        fprintf (stdout, "ac_state_queue_t behaving weirdly.  "
                      "Fatal Error.  Exiting.  Please file a bug report on this");
        exit(EXIT_FAILURE);
    }

    return q->store[q->bot++];
}

/*
#define ac_state_queue_is_empty(q) (((q)->top == (q)->bot) ? 1 : 0)

#define ac_state_enqueue(q, state) do { \
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

#define ac_state_dequeue(q) ( (((q)->bot == STATE_QUEUE_CONTAINER_SIZE)? ((q)->bot = 0): 0), \
                         (((q)->bot == (q)->top) ?                      \
                          (fprintf (stdout, "ac_state_queue_t behaving "                \
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
static __oryx_always_inline__
void SCACClubOutputStates(int32_t dst_state, int32_t src_state,
                                        mpm_ctx_t *mpm_ctx)
{
    void *ptmp;
    ac_ctx_t *ctx = (ac_ctx_t *)mpm_ctx->ctx;
    uint32_t i = 0;
    uint32_t j = 0;

    ac_output_table_t *output_dst_state = &ctx->output_table[dst_state];
    ac_output_table_t *output_src_state = &ctx->output_table[src_state];

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
                fprintf (stdout, "Error allocating memory");
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
static __oryx_always_inline__ void ac_create_failure_table(mpm_ctx_t *mpm_ctx)
{
    ac_ctx_t *ctx = (ac_ctx_t *)mpm_ctx->ctx;
    int ascii_code = 0;
    int32_t state = 0;
    int32_t r_state = 0;

    ac_state_queue_t q;
    memset(&q, 0, sizeof(ac_state_queue_t));

    /* allot space for the failure table.  A failure entry in the table for
     * every state(ac_ctx_t->state_count) */
    ctx->failure_table = kmalloc(ctx->state_count * sizeof(int32_t), MPF_CLR, __oryx_unused_val__);
ASSERT (ctx->failure_table);
    if (ctx->failure_table == NULL) {
        fprintf (stdout, "Error allocating memory");
        exit(EXIT_FAILURE);
    }
	
    /* add the failure transitions for the 0th state, and add every non-fail
     * transition from the 0th state to the queue for further processing
     * of failure states */
    for (ascii_code = 0; ascii_code < 256; ascii_code++) {
        int32_t temp_state = ctx->goto_table[0][ascii_code];
        if (temp_state != 0) {
            ac_state_enqueue(&q, temp_state);
            ctx->failure_table[temp_state] = 0;
        }
    }

    while (!ac_state_queue_is_empty(&q)) {
        /* pick up every state from the queue and add failure transitions */
        r_state = ac_state_dequeue(&q);
        for (ascii_code = 0; ascii_code < 256; ascii_code++) {
            int32_t temp_state = ctx->goto_table[r_state][ascii_code];
            if (temp_state == AC_FAIL)
                continue;
            ac_state_enqueue(&q, temp_state);
            state = ctx->failure_table[r_state];

            while(ctx->goto_table[state][ascii_code] == AC_FAIL)
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
static __oryx_always_inline__
void ac_create_delta_table(mpm_ctx_t *mpm_ctx)
{
    ac_ctx_t *ctx = (ac_ctx_t *)mpm_ctx->ctx;
    int ascii_code = 0;
    int32_t r_state = 0;

    if ((ctx->state_count < 32767) || construct_both_16_and_32_state_tables) {
        ctx->state_table_u16 = kmalloc(ctx->state_count *
                                        sizeof(AC_STATE_TYPE_U16) * 256, MPF_CLR, __oryx_unused_val__);
		ASSERT (ctx->state_table_u16);
        if (ctx->state_table_u16 == NULL) {
            fprintf (stdout, "Error allocating memory");
            exit(EXIT_FAILURE);
        }

        mpm_ctx->memory_cnt++;
        mpm_ctx->memory_size += (ctx->state_count *
                                 sizeof(AC_STATE_TYPE_U16) * 256);

        ac_state_queue_t q;
        memset(&q, 0, sizeof(ac_state_queue_t));

        for (ascii_code = 0; ascii_code < 256; ascii_code++) {
            AC_STATE_TYPE_U16 temp_state = ctx->goto_table[0][ascii_code];
            ctx->state_table_u16[0][ascii_code] = temp_state;
            if (temp_state != 0)
                ac_state_enqueue(&q, temp_state);
        }

        while (!ac_state_queue_is_empty(&q)) {
            r_state = ac_state_dequeue(&q);

            for (ascii_code = 0; ascii_code < 256; ascii_code++) {
                int32_t temp_state = ctx->goto_table[r_state][ascii_code];
                if (temp_state != AC_FAIL) {
                    ac_state_enqueue(&q, temp_state);
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
                                        sizeof(AC_STATE_TYPE_U32) * 256, MPF_CLR, __oryx_unused_val__);
	ASSERT (ctx->state_table_u32);
        if (ctx->state_table_u32 == NULL) {
            fprintf (stdout, "Error allocating memory");
            exit(EXIT_FAILURE);
        }

        mpm_ctx->memory_cnt++;
        mpm_ctx->memory_size += (ctx->state_count *
                                 sizeof(AC_STATE_TYPE_U32) * 256);

        ac_state_queue_t q;
        memset(&q, 0, sizeof(ac_state_queue_t));

        for (ascii_code = 0; ascii_code < 256; ascii_code++) {
            AC_STATE_TYPE_U32 temp_state = ctx->goto_table[0][ascii_code];
            ctx->state_table_u32[0][ascii_code] = temp_state;
            if (temp_state != 0)
                ac_state_enqueue(&q, temp_state);
        }

        while (!ac_state_queue_is_empty(&q)) {
            r_state = ac_state_dequeue(&q);

            for (ascii_code = 0; ascii_code < 256; ascii_code++) {
                int32_t temp_state = ctx->goto_table[r_state][ascii_code];
                if (temp_state != AC_FAIL) {
                    ac_state_enqueue(&q, temp_state);
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

static __oryx_always_inline__
void SCACClubOutputStatePresenceWithDeltaTable(mpm_ctx_t *mpm_ctx)
{
    ac_ctx_t *ctx = (ac_ctx_t *)mpm_ctx->ctx;
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

static __oryx_always_inline__
void SCACInsertCaseSensitiveEntriesForPatterns(mpm_ctx_t *mpm_ctx)
{
    ac_ctx_t *ctx = (ac_ctx_t *)mpm_ctx->ctx;
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
static void SCACPrintDeltaTable(mpm_ctx_t *mpm_ctx)
{
    ac_ctx_t *ctx = (ac_ctx_t *)mpm_ctx->ctx;
    int i = 0, j = 0;

    fprintf (stdout, "##############Delta Table##############\n");
    for (i = 0; i < ctx->state_count; i++) {
        fprintf (stdout, "%d: \n", i);
        for (j = 0; j < 256; j++) {
            if (SCACGetDelta(i, j, mpm_ctx) != 0) {
                fprintf (stdout, "  %c -> %d\n", j, SCACGetDelta(i, j, mpm_ctx));
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
static void ac_state_table_prepare(mpm_ctx_t *mpm_ctx)
{
    ac_ctx_t *ctx = (ac_ctx_t *)mpm_ctx->ctx;

    /* create the 0th state in the goto table and output_table */
    ac_state_init(mpm_ctx);

    SCACDetermineLevel1Gap(mpm_ctx);

    /* create the goto table */
    ac_create_goto_table(mpm_ctx);
    /* create the failure table */
    ac_create_failure_table(mpm_ctx);
    /* create the final state(delta) table */
    ac_create_delta_table(mpm_ctx);
    /* club the output state presence with delta transition entries */
    SCACClubOutputStatePresenceWithDeltaTable(mpm_ctx);

    /* club nocase entries */
    SCACInsertCaseSensitiveEntriesForPatterns(mpm_ctx);

    /* shrink the memory */
    ac_state_shrink(ctx);

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

static void ac_threadctx_print(mpm_threadctx_t __oryx_unused_param__ *mpm_thread_ctx)
{

#ifdef SC_AC_COUNTERS
    ac_threadctx_t *ctx = (ac_threadctx_t *)mpm_thread_ctx->ctx;
    fprintf (stdout, "AC Thread pat_search stats (ctx %p)\n", ctx);
    fprintf (stdout, "Total calls: %" PRIu32 "\n", ctx->total_calls);
    fprintf (stdout, "Total matches: %" PRIu64 "\n", ctx->total_matches);
#endif /* SC_AC_COUNTERS */

    return;
}

/**
 * \brief Init the mpm thread context.
 *
 * \param mpm_ctx        Pointer to the mpm context.
 * \param mpm_thread_ctx Pointer to the mpm thread context.
 * \param matchsize      We don't need this.
 */
static void ac_threadctx_init(mpm_ctx_t __oryx_unused_param__ *mpm_ctx, mpm_threadctx_t *mpm_thread_ctx)
{
    memset(mpm_thread_ctx, 0, sizeof(mpm_threadctx_t));

    mpm_thread_ctx->ctx = kmalloc(sizeof(ac_threadctx_t), MPF_CLR, __oryx_unused_val__);
    if (mpm_thread_ctx->ctx == NULL) {
        exit(EXIT_FAILURE);
    }

    mpm_thread_ctx->memory_cnt++;
    mpm_thread_ctx->memory_size += sizeof(ac_threadctx_t);

    return;
}

/**
 * \brief Destroy the mpm thread context.
 *
 * \param mpm_ctx        Pointer to the mpm context.
 * \param mpm_thread_ctx Pointer to the mpm thread context.
 */
static void ac_threadctx_destroy(mpm_ctx_t __oryx_unused_param__ *mpm_ctx, mpm_threadctx_t *mpm_thread_ctx)
{
    ac_threadctx_print(mpm_thread_ctx);

    if (mpm_thread_ctx->ctx != NULL) {
        kfree(mpm_thread_ctx->ctx);
        mpm_thread_ctx->ctx = NULL;
        mpm_thread_ctx->memory_cnt--;
        mpm_thread_ctx->memory_size -= sizeof(ac_threadctx_t);
    }

    return;
}

static void ac_ctx_print(mpm_ctx_t __oryx_unused_param__ *mpm_ctx)
{
    ac_ctx_t __oryx_unused_param__ *ctx = (ac_ctx_t *)mpm_ctx->ctx;
#if 1
    fprintf (stdout, "MPM AC Information:\n");
    fprintf (stdout, "Memory allocs:   %" PRIu32 "\n", mpm_ctx->memory_cnt);
    fprintf (stdout, "Memory alloced:  %" PRIu32 "\n", mpm_ctx->memory_size);
    fprintf (stdout, " Sizeof:\n");
    fprintf (stdout, "  mpm_ctx_t         %" PRIuMAX "\n", (uintmax_t)sizeof(mpm_ctx_t));
    fprintf (stdout, "  ac_ctx_t:         %" PRIuMAX "\n", (uintmax_t)sizeof(ac_ctx_t));
    fprintf (stdout, "  mpm_pattern_t      %" PRIuMAX "\n", (uintmax_t)sizeof(mpm_pattern_t));
    fprintf (stdout, "  mpm_pattern_t     %" PRIuMAX "\n", (uintmax_t)sizeof(mpm_pattern_t));
    fprintf (stdout, "Unique Patterns: %" PRIu32 "\n", mpm_ctx->pattern_cnt);
    fprintf (stdout, "Smallest:        %" PRIu32 "\n", mpm_ctx->minlen);
    fprintf (stdout, "Largest:         %" PRIu32 "\n", mpm_ctx->maxlen);
    fprintf (stdout, "Total states in the state table:    %" PRIu32 "\n", ctx->state_count);
    fprintf (stdout, "\n");
#endif

    return;
}

/**
 * \brief Initialize the AC context.
 *
 * \param mpm_ctx       Mpm context.
 */
static void ac_ctx_init(mpm_ctx_t *mpm_ctx)
{
    if (mpm_ctx->ctx != NULL)
        return;

    mpm_ctx->ctx = kmalloc(sizeof(ac_ctx_t), MPF_CLR, __oryx_unused_val__);
    if (mpm_ctx->ctx == NULL) {
        exit(EXIT_FAILURE);
    }

    mpm_ctx->memory_cnt++;
    mpm_ctx->memory_size += sizeof(ac_ctx_t);

    /* initialize the hash we use to speed up pattern insertions */
    mpm_ctx->init_hash = kmalloc(sizeof(mpm_pattern_t *) * MPM_INIT_HASH_SIZE, MPF_CLR, __oryx_unused_val__);
    if (mpm_ctx->init_hash == NULL) {
        exit(EXIT_FAILURE);
    }

    /* get conf values for AC from our yaml file.  We have no conf values for
     * now.  We will certainly need this, as we develop the algo */
    ac_get_config();

}

/**
 * \brief Destroy the mpm context.
 *
 * \param mpm_ctx Pointer to the mpm context.
 */
static void ac_ctx_destroy(mpm_ctx_t *mpm_ctx)
{
    ac_ctx_t *ctx = (ac_ctx_t *)mpm_ctx->ctx;
    if (ctx == NULL)
        return;
	
    if (mpm_ctx->init_hash != NULL) {
        kfree(mpm_ctx->init_hash);
        mpm_ctx->init_hash = NULL;
        mpm_ctx->memory_cnt--;
        mpm_ctx->memory_size -= (MPM_INIT_HASH_SIZE * sizeof(mpm_pattern_t *));
    }

    if (ctx->parray != NULL) {
        uint32_t i;
        for (i = 0; i < mpm_ctx->pattern_cnt; i++) {
            if (ctx->parray[i] != NULL) {
                mpm_pattern_free(mpm_ctx, ctx->parray[i]);
            }
        }

        kfree(ctx->parray);
        ctx->parray = NULL;
        mpm_ctx->memory_cnt--;
        mpm_ctx->memory_size -= (mpm_ctx->pattern_cnt * sizeof(mpm_pattern_t *));
    }

    if (ctx->state_table_u16 != NULL) {
        kfree(ctx->state_table_u16);
        ctx->state_table_u16 = NULL;

        mpm_ctx->memory_cnt++;
        mpm_ctx->memory_size -= (ctx->state_count *
                                 sizeof(AC_STATE_TYPE_U16) * 256);
    }
    if (ctx->state_table_u32 != NULL) {
        kfree(ctx->state_table_u32);
        ctx->state_table_u32 = NULL;

        mpm_ctx->memory_cnt++;
        mpm_ctx->memory_size -= (ctx->state_count *
                                 sizeof(AC_STATE_TYPE_U32) * 256);
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
    mpm_ctx->memory_size -= sizeof(ac_ctx_t);

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
static uint32_t ac_pattern_search(mpm_ctx_t *mpm_ctx, mpm_threadctx_t __oryx_unused_param__ *mpm_thread_ctx,
                    PrefilterRuleStore *pmq, const uint8_t *buf, uint16_t buflen)
{
    const ac_ctx_t *ctx = (ac_ctx_t *)mpm_ctx->ctx;
    int i = 0;
    int matches = 0;

    /* \todo tried loop unrolling with register var, with no perf increase.  Need
     * to dig deeper */
    /* \todo Change it for stateful MPM.  Supply the state using mpm_thread_ctx */
    ac_pattern_list_t *pid_pat_list = ctx->pid_pat_list;


#ifdef BIT_ARRAY_ALLOC_STACK
    uint8_t bitarray[ctx->pattern_id_bitarray_size];
    memset (bitarray, 0, ctx->pattern_id_bitarray_size);
#else
	uint8_t *bitarray = NULL;
#endif

/** TSIHANG */
#ifndef BIT_ARRAY_ALLOC_STACK 
	uint32_t size = BIT_ARRAY_DEFAULT_SIZE;

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
        register AC_STATE_TYPE_U16 state = 0;
        AC_STATE_TYPE_U16 (*state_table_u16)[256] = ctx->state_table_u16;
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
                            mpm_hold_matched_sids(pmq, pid_pat_list[lower_pid].sids, pid_pat_list[lower_pid].sids_size);
                        }
                        matches++;
                    } else {
                        if (bitarray[pids[k] / 8] & (1 << (pids[k] % 8))) {
                            ;
                        } else {
                            bitarray[pids[k] / 8] |= (1 << (pids[k] % 8));
                            mpm_hold_matched_sids(pmq, pid_pat_list[pids[k]].sids, pid_pat_list[pids[k]].sids_size);
                        }
                        matches++;
                    }
                    //loop1:
                    //;
                }
            }
        } /* for (i = 0; i < buflen; i++) */

    } else {
        register AC_STATE_TYPE_U32 state = 0;
        AC_STATE_TYPE_U32 (*state_table_u32)[256] = ctx->state_table_u32;
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
                            mpm_hold_matched_sids(pmq, pid_pat_list[lower_pid].sids, pid_pat_list[lower_pid].sids_size);
                        }
                        matches++;
                    } else {
                        if (bitarray[pids[k] / 8] & (1 << (pids[k] % 8))) {
                            ;
                        } else {
                            bitarray[pids[k] / 8] |= (1 << (pids[k] % 8));
                            mpm_hold_matched_sids(pmq, pid_pat_list[pids[k]].sids, pid_pat_list[pids[k]].sids_size);
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
static int ac_pattern_add_ci(mpm_ctx_t *mpm_ctx, uint8_t *pat, uint16_t patlen,
                     uint16_t offset, uint16_t depth, uint32_t pid,
                     sig_id sid, uint8_t flags)
{
    flags |= MPM_PATTERN_FLAG_NOCASE;
    return mpm_pattern_add(mpm_ctx, pat, patlen, offset, depth, pid, sid, flags);
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
static int ac_pattern_add_cs(mpm_ctx_t *mpm_ctx, uint8_t *pat, uint16_t patlen,
                     uint16_t offset, uint16_t depth, uint32_t pid,
                     sig_id sid, uint8_t flags)
{
    return mpm_pattern_add(mpm_ctx, pat, patlen, offset, depth, pid, sid, flags);
}

/**
* \brief Process the patterns added to the mpm, and create the internal tables.
*
* \param mpm_ctx Pointer to the mpm context.
*/
static int ac_pattern_prepare(mpm_ctx_t *mpm_ctx)
{
	ac_ctx_t *ctx = (ac_ctx_t *)mpm_ctx->ctx;

	if (mpm_ctx->pattern_cnt == 0 || mpm_ctx->init_hash == NULL) {
	 fprintf (stdout, "no patterns supplied to this mpm_ctx\n");
	 return 0;
	}

	/* alloc the pattern array */
	ctx->parray = (mpm_pattern_t **)kmalloc(mpm_ctx->pattern_cnt *
										sizeof(mpm_pattern_t *), MPF_CLR, __oryx_unused_val__);
	if (ctx->parray == NULL)
	 goto error;

	mpm_ctx->memory_cnt++;
	mpm_ctx->memory_size += (mpm_ctx->pattern_cnt * sizeof(mpm_pattern_t *));

	/* populate it with the patterns in the hash */
	uint32_t i = 0, p = 0;
	for (i = 0; i < MPM_INIT_HASH_SIZE; i++) {
	 mpm_pattern_t *node = mpm_ctx->init_hash[i], *nnode = NULL;
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
	ctx->pid_pat_list = kmalloc((mpm_ctx->max_pat_id + 1)* sizeof(ac_pattern_list_t), MPF_CLR, __oryx_unused_val__);
	ASSERT (ctx->pid_pat_list);
	if (ctx->pid_pat_list == NULL) {
	 fprintf (stdout,  "Error allocating memory\n");
	 exit(EXIT_FAILURE);
	}

	for (i = 0; i < mpm_ctx->pattern_cnt; i++) {
	 if (!(ctx->parray[i]->flags & MPM_PATTERN_FLAG_NOCASE)) {
		 ctx->pid_pat_list[ctx->parray[i]->id].cs = kmalloc(ctx->parray[i]->len, MPF_CLR, __oryx_unused_val__);
	 ASSERT (ctx->pid_pat_list[ctx->parray[i]->id].cs);
		 if (ctx->pid_pat_list[ctx->parray[i]->id].cs == NULL) {
			 fprintf (stdout,  "Error allocating memory\n");
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
	ac_state_table_prepare(mpm_ctx);

	fprintf (stdout, "Built %" PRIu32 " patterns into a database of size %" PRIu32
			" bytes\n", mpm_ctx->pattern_cnt, mpm_ctx->memory_size);

	/* free all the stored patterns.  Should save us a good 100-200 mbs */
	for (i = 0; i < mpm_ctx->pattern_cnt; i++) {
	 if (ctx->parray[i] != NULL) {
		 mpm_pattern_free(mpm_ctx, ctx->parray[i]);
	 }
	}
	kfree(ctx->parray);
	ctx->parray = NULL;
	mpm_ctx->memory_cnt--;
	mpm_ctx->memory_size -= (mpm_ctx->pattern_cnt * sizeof(mpm_pattern_t *));

	ctx->pattern_id_bitarray_size = (mpm_ctx->max_pat_id / 8) + 1;


#if 0
	fprintf (stdout, "ctx->pattern_id_bitarray_size =%u, mpm_ctx->max_pat_id =%u\n", 
	 ctx->pattern_id_bitarray_size,
	 mpm_ctx->max_pat_id);
#endif

	return 0;

	error:
	return -1;
}

/************************** Mpm Registration ***************************/

/**
 * \brief Register the aho-corasick mpm.
 */
void mpm_register_ac(void)
{
    mpm_table[MPM_AC].name = "ac";
    mpm_table[MPM_AC].ctx_init = ac_ctx_init;
    mpm_table[MPM_AC].threadctx_init = ac_threadctx_init;
    mpm_table[MPM_AC].ctx_destroy = ac_ctx_destroy;
    mpm_table[MPM_AC].threadctx_destroy = ac_threadctx_destroy;
    mpm_table[MPM_AC].pat_add = ac_pattern_add_cs;
    mpm_table[MPM_AC].pat_add_nocase = ac_pattern_add_ci;
    mpm_table[MPM_AC].pat_prepare = ac_pattern_prepare;
    mpm_table[MPM_AC].pat_search = ac_pattern_search;
    mpm_table[MPM_AC].Cleanup = NULL;
    mpm_table[MPM_AC].ctx_print = ac_ctx_print;
    mpm_table[MPM_AC].threadctx_print = ac_threadctx_print;

    return;
}


