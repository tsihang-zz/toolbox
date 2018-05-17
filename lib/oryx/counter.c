#include "oryx.h"

static struct StatsGlobalCtx *sg_ctx = NULL;


/**
 * \brief Registers a counter.
 *
 * \param name    Name of the counter, to be registered
 * \param ctx     CounterCtx
 * \param type_q   Qualifier describing the type of counter to be registered
 *
 * \retval the counter id for the newly registered counter, or the already
 *         present counter on success
 * \retval 0 on failure
 */
static counter_id RegisterQualifiedCounter(const char *name,
                                              struct CounterCtx *ctx,
                                              int type_q, u64 (*hook)(void))
{
    struct counter_t **h = &ctx->hhead;
    struct counter_t *temp = NULL;
    struct counter_t *prev = NULL;
    struct counter_t *c = NULL;

    if (name == NULL || ctx == NULL) {
        oryx_loge(-1,
			"CounterName/CounterCtx NULL");
        return 0;
    }

    temp = prev = *h;
    while (temp != NULL) {
        prev = temp;
        if (strcmp(name, temp->sc_name) == 0) {
			oryx_logw(0,
				"Counter %s registered more than one times.", name);
			break;
        }
        temp = temp->next;
    }

    /* We already have a counter registered by this name */
    if (temp != NULL)
        return(temp->id);

    /* if we reach this point we don't have a counter registered by this name */
    if ( (c = malloc(sizeof(struct counter_t))) == NULL) {
		oryx_logw(0,
				"Can not alloc memory for counter %s ... exit", name);
        exit(EXIT_FAILURE);
    }
	
    memset(c, 0, sizeof(struct counter_t));

    /* assign a unique id to this struct counter_t.  The id is local to this
     * thread context.  Please note that the id start from 1, and not 0 */
	c->id = atomic_inc(&ctx->curr_id);
    c->sc_name = name;
    c->type = type_q;
    c->hook = hook;

    /* we now add the counter to the list */
    if (prev == NULL)
        *h = c;
    else
        prev->next = c;

	ctx->h_size ++;
	
    return c->id;
}

counter_id oryx_register_counter(const char *name,
							const char *comments,
							 struct CounterCtx *ctx)
{
	comments = comments;
	counter_id id = RegisterQualifiedCounter(name, ctx,
											  STATS_TYPE_Q_NORMAL, NULL);
	return id;
}
							 
/**
* \brief Releases counters
*
* \param head Pointer to the head of the list of perf counters that have to
* 			be freed
*/
void oryx_release_counter(struct CounterCtx *ctx)
{
	struct counter_t *head = ctx->hhead;
	struct counter_t *c = NULL;

	while (head != NULL) {
		c = head;
		head = head->next;
		free(c);
		ctx->h_size --;
	}

	if (ctx->head) {
		free (ctx->head);
	}
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
int oryx_counter_get_array_range(counter_id s_id, counter_id e_id,
                                      struct CounterCtx *ctx)
{
    struct counter_t *c = NULL;
    uint32_t i = 0;

    if (ctx == NULL) {
        oryx_loge(-1,
			"CounterCtx is NULL");
        return -1;
    }

    if (s_id < 1 || e_id < 1 || s_id > e_id) {
        oryx_loge(-1,
			"error with the counter ids");
        return -1;
    }

    if (e_id > (counter_id)atomic_read(&ctx->curr_id)) {
        oryx_loge(-1,
			"end id is greater than the max id.");
        return -1;
    }

    if ( (ctx->head = malloc(sizeof(struct counter_t) * (e_id - s_id  + 2))) == NULL) {
        return -1;
    }
    memset(ctx->head, 0, sizeof(struct counter_t) * (e_id - s_id  + 2));

    c = ctx->hhead;
    while (c->id != s_id)
        c = c->next;

    i = 1;
    while ((c != NULL) && (c->id <= e_id)) {
		memcpy(&ctx->head[i], c, sizeof(*c));
        c = c->next;
        i++;
    }
    ctx->size = i - 1;
	
    return 0;
}

/**
* \brief Increments the counter
*/
void oryx_counter_inc(struct CounterCtx *ctx, counter_id id)
{
#ifdef DEBUG
	BUG_ON ((id < 1) || (id > ctx->size));
#endif
	atomic64_inc(&ctx->head[id].value);
	atomic64_inc(&ctx->head[id].updates);
}
/**
 * \brief Adds a value of type uint64_t to the local counter.
 */
void oryx_counter_add(struct CounterCtx *ctx, counter_id id, u64 x)
{
#ifdef DEBUG
    BUG_ON ((id < 1) || (id > ctx->size));
#endif
	atomic64_add(&ctx->head[id].value, x);
	atomic64_inc(&ctx->head[id].updates);
    return;
}

/**
 * \brief Sets a value of type double to the local counter
 */
void oryx_counter_set(struct CounterCtx *ctx, counter_id id, u64 x)
{
#ifdef DEBUG
		BUG_ON ((id < 1) || (id > ctx->size));
#endif

    if ((ctx->head[id].type == STATS_TYPE_Q_MAXIMUM) &&
            (x > (u64)atomic64_read(&ctx->head[id].value))) {
		atomic64_set(&ctx->head[id].value, x);
    } else if (ctx->head[id].type == STATS_TYPE_Q_NORMAL) {
		atomic64_set(&ctx->head[id].value, x);
    }

	atomic64_inc(&ctx->head[id].updates);

    return;
}

/**
 * \brief Get the value of the local copy of the counter that hold this id.
 */
u64 oryx_counter_get(struct CounterCtx *ctx, counter_id id)
{
#ifdef DEBUG
    BUG_ON ((id < 1) || (id > ctx->size));
#endif
    return (u64)atomic64_read(&ctx->head[id].value);
}


void oryx_counter_init(void)
{
	BUG_ON(sg_ctx != NULL);

	if((sg_ctx = malloc (sizeof (*sg_ctx))) == NULL) {
		 oryx_loge(-1, 
			 "Fatal error encountered in Stats Context. Exiting...");
		 exit(EXIT_FAILURE);
	}
	memset (sg_ctx, 0, sizeof(*sg_ctx));
	pthread_mutex_init(&sg_ctx->lock, NULL);

	/** do a simple test */
	struct CounterCtx ctx;

	memset(&ctx, 0, sizeof(struct CounterCtx));

	counter_id id1 = oryx_register_counter("t1", "c1", &ctx);
	BUG_ON(id1 != 1);
	counter_id id2 = oryx_register_counter("t2", "c2", &ctx);
	BUG_ON(id2 != 2);
	
	oryx_counter_get_array_range(1, 
		atomic_read(&ctx.curr_id), &ctx);

	oryx_counter_inc(&ctx, id1);
	oryx_counter_inc(&ctx, id2);

	u64 id1_val = 0;
	u64 id2_val = 0;
	
	id1_val = oryx_counter_get(&ctx, id1);
	id2_val = oryx_counter_get(&ctx, id2);

	BUG_ON(id1_val != 1);
	BUG_ON(id2_val != 1);

	oryx_counter_inc(&ctx, id1);
	oryx_counter_inc(&ctx, id2);

	id1_val = oryx_counter_get(&ctx, id1);
	id2_val = oryx_counter_get(&ctx, id2);

	BUG_ON(id1_val != 2);
	BUG_ON(id2_val != 2);

	oryx_counter_add(&ctx, id1, 100);
	oryx_counter_add(&ctx, id2, 100);

	id1_val = oryx_counter_get(&ctx, id1);
	id2_val = oryx_counter_get(&ctx, id2);

	BUG_ON(id1_val != 102);
	BUG_ON(id2_val != 102);

	oryx_counter_set(&ctx, id1, 1000);
	oryx_counter_set(&ctx, id2, 1000);

	id1_val = oryx_counter_get(&ctx, id1);
	id2_val = oryx_counter_get(&ctx, id2);

	BUG_ON(id1_val != 1000);
	BUG_ON(id2_val != 1000);
	
	oryx_release_counter(&ctx);

	BUG_ON(ctx.h_size != 0);
	
}

