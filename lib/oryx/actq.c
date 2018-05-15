
#include "oryx.h"

extern oryx_status_t actq_inner_read (struct oryx_actq_t *actq,
						struct oryx_actq_raw_t **inner_raw);
extern oryx_status_t actq_inner_write (struct oryx_actq_t *actq,
						struct oryx_actq_raw_t *inner_raw);

#define tracker(fmt,...)\
	printf (""fmt"", ##__VA_ARGS__)

typedef struct oryx_actq_mgr_t {
	/** Hash table. */
	struct oryx_htable_t *ht_table;
	/** Used to alloc queue id. */
	oryx_vector vec_table;
	oryx_status_t has_been_initialized;

	oryx_status_t (*fn_write) (struct oryx_actq_t *, struct oryx_actq_raw_t *raw);
	oryx_status_t (*fn_read) (struct oryx_actq_t *, struct oryx_actq_raw_t **raw);
	
#define INIT_HASH_TABLE_ERROR	(1 << 0)
#define INIT_VEC_TABLE_ERROR	(1 << 1)
	u32 ul_flags;

} oryx_actq_mgr_t;

static struct oryx_actq_mgr_t actq_mgr = {
	.ht_table = NULL,
	.vec_table = NULL,
	.has_been_initialized = 0,
	.ul_flags = (INIT_HASH_TABLE_ERROR | INIT_VEC_TABLE_ERROR),
	.fn_write = actq_inner_write,
	.fn_read = actq_inner_read,
};


static void _wakeup (struct oryx_actq_t *actq)
{	
	if (actq->ul_flags & ACTQ_ENABLE_TRIGGER) {
		do_lock (actq->ol_lock);
		oryx_thread_cond_signal(actq->ol_cond);
		do_unlock (actq->ol_lock);
	}
}

static void _stupor (struct oryx_actq_t *actq)
{
	if (atomic_read(&actq->ul_buffered_blocks) == 0) {
		if (actq->ul_flags & ACTQ_ENABLE_TRIGGER) {
			do_lock (actq->ol_lock);
			oryx_thread_cond_wait(actq->ol_cond, actq->ol_lock);		
			do_unlock (actq->ol_lock);
		}
	}
}

static __oryx_always_inline__
void actq_try_alloc (struct oryx_actq_prefix_t *ap, struct oryx_actq_t **actq)
{
	
	(*actq) = kmalloc(sizeof (struct oryx_actq_t), MPF_CLR, -1);

	ASSERT ((*actq));

	/** */
	memcpy ((*actq)->sc_alias, ap->sc_alias, ap->ul_alias_size);
	(*actq)->ul_alias_size = ap->ul_alias_size;
	(*actq)->ul_prio = ap->ul_prio;
	(*actq)->ul_flags = ap->ul_flags;
	(*actq)->ul_cache_size = (ap->ul_flags & ACTQ_ENABLE_FIXED_BUFFER) ? ap->ul_buffered_refcnt : INT_MAX;
	(*actq)->fn_wakeup = _wakeup;
	(*actq)->fn_stupor = _stupor;
	atomic_set(&(*actq)->ul_buffered_blocks, 0);

	oryx_thread_mutex_create(&(*actq)->ol_actq_lock);

	/** critical for RAW data arrival notification. */
	oryx_thread_mutex_create(&(*actq)->ol_lock);
	oryx_thread_cond_create (&(*actq)->ol_cond);

	INIT_LIST_HEAD (&((*actq)->head));

}

static void actq_free (void __oryx_unused__ *v)
{
	v = v; /** To avoid warnings. */
}

static ht_key_t actq_hval (struct oryx_htable_t *ht,
		ht_value_t v, u32 s) 
{
     uint8_t *d = (uint8_t *)v;
     uint32_t i;
     uint32_t hv = 0;

     for (i = 0; i < s; i++) {
         if (i == 0)      hv += (((uint32_t)*d++));
         else if (i == 1) hv += (((uint32_t)*d++) * s);
         else             hv *= (((uint32_t)*d++) * i) + s + i;
     }

     hv *= s;
     hv %= ht->array_size;
     
     return hv;
}

static int actq_cmp (ht_value_t v1, 
		u32 s1,
		ht_value_t v2,
		u32 s2)
{
	int xret = 0;

	if (!v1 || !v2 ||s1 != s2 ||
		memcmp(v1, v2, s2))
		xret = 1;
	
	return xret;
}

__oryx_always_extern__
void actq_lookup (struct oryx_actq_prefix_t *ap, struct oryx_actq_t **v)
{
	(*v) = NULL;
	ASSERT (ap);

	struct oryx_actq_mgr_t *mgr = &actq_mgr;
	
	void *s = oryx_htable_lookup (mgr->ht_table,
		ap->sc_alias, ap->ul_alias_size);

	if (s) {
		(*v) = (struct oryx_actq_t *) container_of (s, struct oryx_actq_t, sc_alias);
	}
}

static __oryx_always_inline__
void actq_inner_init (struct oryx_actq_mgr_t *mgr)
{	
	mgr->ht_table = oryx_htable_init(DEFAULT_HASH_CHAIN_SIZE, 
			actq_hval, actq_cmp, actq_free, HTABLE_SYNCHRONIZED);
	if (unlikely (!mgr->ht_table)) {
		mgr->ul_flags |= INIT_HASH_TABLE_ERROR;
		goto finish;
	}

	mgr->vec_table = vec_init (1);
	if (unlikely (!mgr->ht_table)) {
		oryx_htable_destroy(mgr->ht_table);
		mgr->ul_flags |= INIT_VEC_TABLE_ERROR;
		goto finish;
	}

	mgr->has_been_initialized = 1;

finish:
	return;
}

static __oryx_always_inline__
void actq_inner_raw_try_alloc (struct oryx_actq_raw_t *raw, 
						struct oryx_actq_raw_t **inner_raw)
{

	(*inner_raw) = NULL;
	
	struct oryx_actq_raw_t *ir;
	ir = (struct oryx_actq_raw_t *) kmalloc (sizeof (struct oryx_actq_raw_t), 
						MPF_CLR, __oryx_unused_val__);
	ASSERT (ir);
	RAW_DATA(ir) = RAW_DATA(raw);
	RAW_SIZE(ir) = RAW_SIZE(raw);
	INIT_LIST_HEAD (&ir->node);

	/** hold this raw. */
	RAW(*inner_raw) = RAW(ir);
}

static __oryx_always_inline__
void actq_inner_raw_try_free (struct oryx_actq_raw_t *inner_raw)
{
	ASSERT(inner_raw);
	ASSERT(RAW_DATA(inner_raw));
	
	kfree(RAW_DATA(inner_raw));
	kfree(inner_raw);
}

oryx_status_t actq_inner_write (struct oryx_actq_t *actq,
						struct oryx_actq_raw_t *inner_raw)
{
	const u32 ul_current_backlog = atomic_inc(&actq->ul_buffered_blocks);

	if (actq->ul_flags & ACTQ_ENABLE_FIXED_BUFFER) {
		int off = atomic_read(&actq->ul_buffered_blocks);
		if (off >= (int)actq->ul_cache_size) {
			actq->ul_write_error ++;
			return -2;
		}
	} 
	
	if (actq->ul_flags & ACTQ_ENABLE_WRITE) {

		ACTQ_STAT_INC_EQ (RAW_SIZE(inner_raw), actq);

		actq->ul_peek_backlog = MAX(ul_current_backlog, actq->ul_peek_backlog);

		/** write raw to Write Queue. */
		do_lock(actq->ol_actq_lock);	
		list_add_tail (&inner_raw->node, &actq->head);
		/**
		 * tracker ("%p, data-> %p\n", inner_raw, RAW_DATA(inner_raw));
		 */
		do_unlock(actq->ol_actq_lock);
		actq->fn_wakeup(actq);
		return 0;
	}

	tracker ("queue %s is not writeable.\n", actq->sc_alias);
	return 0;
}

oryx_status_t actq_inner_read (struct oryx_actq_t *actq,
						struct oryx_actq_raw_t **inner_raw)
{
	oryx_status_t s = -1;

	/** wait for raw arrival signal. */
	actq->fn_stupor(actq);
	
	if (actq->ul_flags & ACTQ_ENABLE_READ) {
		struct oryx_actq_raw_t *pos, *n;

		do_lock(actq->ol_actq_lock);
		list_for_each_entry_safe (pos, n, &actq->head, node) {
			/** Read raw from Read Queue. */
			list_del (&pos->node);
			atomic_dec(&actq->ul_buffered_blocks);
			s = 0;
			(*inner_raw) = pos;
			/**
			 *	tracker ("%s%p%s, data-> %p\n", 
			 *		draw_color(COLOR_RED), (*inner_raw), draw_color(COLOR_FIN),
			 *		RAW_DATA(*inner_raw));
			 */
			ACTQ_STAT_INC_DQ (RAW_SIZE(*inner_raw), actq);

			break;
		}
		do_unlock(actq->ol_actq_lock);
	}
	
	return s;
}

oryx_status_t oryx_actq_write (struct oryx_actq_t *actq, 
						struct oryx_actq_raw_t *raw)
{
	oryx_status_t s = -1;
	struct oryx_actq_mgr_t *mgr = &actq_mgr;
	ASSERT (actq);
	ASSERT (raw);

	struct oryx_actq_raw_t *inner_raw;
	actq_inner_raw_try_alloc (raw, &inner_raw);
	if (likely (inner_raw)) {
		s = mgr->fn_write (actq, inner_raw);
		if (likely (s)) {
			kfree (inner_raw);
		}
	}
	
	return s;
}

oryx_status_t oryx_actq_read (struct oryx_actq_t *actq, 
						struct oryx_actq_raw_t **raw)
{
	struct oryx_actq_mgr_t *mgr = &actq_mgr;
	ASSERT (actq);
	(*raw) = NULL;

	return mgr->fn_read (actq, raw);
}

oryx_status_t oryx_actq_new (struct oryx_actq_prefix_t *ap, 
						struct oryx_actq_t **actq)
{
	struct oryx_actq_mgr_t *mgr = &actq_mgr;

	(*actq) = NULL;
	ASSERT (ap);

	if (unlikely(!mgr->has_been_initialized)) {
		actq_inner_init (mgr);
		if (unlikely (!mgr->has_been_initialized)) {
			tracker ("Tables init error\n");
		}
	}

	struct oryx_actq_t *v;
	actq_lookup (ap, &v);
	if (v) {
		/** back to caller. */
		tracker ("existent actq %p\n", v);
		(*actq) = v;
		return 0;
	} else {
	
		actq_try_alloc (ap, &v);
		ASSERT (v);
		(*actq) = v;
		int r = oryx_htable_add (mgr->ht_table, (*actq)->sc_alias, (*actq)->ul_alias_size);
		if (r == 0 /** success*/) {
			(*actq)->ul_id = vec_set (mgr->vec_table, (*actq));
			tracker ("new actq %p\n", (*actq));
		}
	}
	
	printf ("alias			%s\n", (*actq)->sc_alias);
	printf ("threshold		%d\n", (*actq)->ul_cache_size);
	printf ("flags			%0x\n", (*actq)->ul_flags);
	
	return 0;
}



