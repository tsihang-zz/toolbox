#include "oryx.h"

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
static counter_id register_qualified_counter (const char *name,
                                              struct CounterCtx *ctx,
                                              int type_q, uint64_t (*hook)(void))
{
	BUG_ON ((name == NULL) || (ctx == NULL));
	
    struct counter_t **h = &ctx->hhead;
    struct counter_t *temp = NULL;
    struct counter_t *prev = NULL;
    struct counter_t *c = NULL;

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
    if ((c = malloc(sizeof(struct counter_t))) == NULL)
		oryx_panic(-1,
			"malloc: %s", oryx_safe_strerror(errno));
	
    memset(c, 0, sizeof(struct counter_t));

    /* assign a unique id to this struct counter_t.  The id is local to this
     * thread context.  Please note that the id start from 1, and not 0 */
	c->id		= atomic_inc(&ctx->curr_id);
    c->sc_name	= name;
    c->type		= type_q;
    c->hook		= hook;

    /* we now add the counter to the list */
    if (prev == NULL) *h = c;
	else prev->next = c;

	ctx->h_size ++;
	
    return c->id;
}

counter_id oryx_register_counter(const char *name,
							const char *comments,
							 struct CounterCtx *ctx)
{
	comments = comments;
	counter_id id = register_qualified_counter (name, ctx,
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
		c		= head;
		head	= head->next;
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
	BUG_ON (ctx == NULL);
	BUG_ON ((s_id < 1) || (e_id < 1) || (s_id > e_id));

    struct counter_t *c = NULL;
    uint32_t i = 0;
	
    if (e_id > (counter_id)atomic_read(&ctx->curr_id))
        oryx_panic(-1,
			"end id is greater than the max id.");

    if ((ctx->head = malloc(sizeof(struct counter_t) * (e_id - s_id  + 2))) == NULL)
        oryx_panic(-1,
			"malloc: %s", oryx_safe_strerror(errno));
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

void oryx_counter_initialize(void)
{
	/* DO NOTHING */
}

