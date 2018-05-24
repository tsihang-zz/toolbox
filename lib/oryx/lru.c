#include "oryx.h"

/* this is developers aid only.
 * it catches concurrent access (lack of locking on the users part) */
#define PARANOIA_ENTRY() do {		\
	BUG_ON(!lc);			\
	BUG_ON(!lc->nr_elements);	\
	__test_and_set_bit (__LC_PARANOIA, &lc->flags);\
} while (0)

#define RETURN(x...)     do { \
	clear_bit_unlock(__LC_PARANOIA, &lc->flags); \
	return x ; } while (0)

/* BUG() if e is not one of the elements tracked by lc */
#define PARANOIA_LC_ELEMENT(lc, e) do {	\
	struct lru_cache *lc_ = (lc);	\
	struct lc_element *e_ = (e);	\
	unsigned i = e_->lc_index;	\
	BUG_ON(i >= lc_->nr_elements);	\
	BUG_ON(lc_->lc_element[i] != e_); } while (0)

/* used as internal flags to __lc_get */
enum {
	LC_GET_MAY_CHANGE = 1,
	LC_GET_MAY_USE_UNCOMMITTED = 2,
};

static void lc_free_by_index(struct lru_cache *lc, unsigned i)
{
	void *p = lc->lc_element[i];
	WARN_ON(!p);
	if (p) {
		/** p -= lc->element_off; */
		void *obj = (void *)((size_t)p - lc->element_off);
		oryx_kc_free(lc->lc_cache, obj);
		/** printf ("(size_t)p %lu, p-off %lu\n", (size_t)obj, ((size_t)p - lc->element_off)); */
	}
}

/**
 * lc_destroy - frees memory allocated by lc_create()
 * @lc: the lru cache to destroy
 */
void lc_destroy(struct lru_cache *lc)
{
	unsigned i;
	if (!lc)
		return;
	for (i = 0; i < lc->nr_elements; i++)
		lc_free_by_index(lc, i);
	kfree(lc->lc_element);
	kfree(lc->lc_slot);
	kfree(lc);
}
	
/**
 * lc_reset - does a full reset for @lc and the hash table slots.
 * @lc: the lru cache to operate on
 *
 * It is roughly the equivalent of re-allocating a fresh lru_cache object,
 * basically a short cut to lc_destroy(lc); lc = lc_create(...);
 */
void lc_reset(struct lru_cache *lc)
{
	unsigned i;

	INIT_LIST_HEAD(&lc->in_use);
	INIT_LIST_HEAD(&lc->lru);
	INIT_LIST_HEAD(&lc->free);
	INIT_LIST_HEAD(&lc->to_be_changed);
	lc->used = 0;
	lc->hits = 0;
	lc->misses = 0;
	lc->starving = 0;
	lc->locked = 0;
	lc->changed = 0;
	lc->pending_changes = 0;
	lc->flags = 0;
	memset(lc->lc_slot, 0, sizeof(struct hlist_head) * lc->nr_elements);

	for (i = 0; i < lc->nr_elements; i++) {
		struct lc_element *e = lc->lc_element[i];
		/**
		void *p = e;
		p -= lc->element_off;
		*/
		void *p = (void *)((size_t)e - lc->element_off);
		memset(p, 0, lc->element_size);
		/* re-init it */
		e->lc_index = i;
		e->lc_number = LC_FREE;
		e->lc_new_number = LC_FREE;
		list_add(&e->list, &lc->free);
	}
}

/**
 * lc_seq_printf_stats - print stats about @lc into @seq
 * @seq: the seq_file to print into
 * @lc: the lru cache to print statistics of
 */
void lc_seq_printf_stats(FILE *seq, struct lru_cache *lc)
{
	/* NOTE:
	 * total calls to lc_get are
	 * (starving + hits + misses)
	 * misses include "locked" count (update from an other thread in
	 * progress) and "changed", when this in fact lead to an successful
	 * update of the cache.
	 */
	fprintf(seq, "\t%s: used:%u/%u hits:%lu misses:%lu starving:%lu locked:%lu changed:%lu\n",
		   lc->name, lc->used, lc->nr_elements,
		   lc->hits, lc->misses, lc->starving, lc->locked, lc->changed);
}

static struct hlist_head *lc_hash_slot(struct lru_cache *lc, unsigned int enr)
{
	return	lc->lc_slot + (enr % lc->nr_elements);
}

static int lc_unused_element_available(struct lru_cache *lc)
{
	if (!list_empty(&lc->free))
		return 1; /* something on the free list */
	if (!list_empty(&lc->lru))
		return 1;  /* something to evict */

	return 0;
}

static struct lc_element *lc_prepare_for_change(struct lru_cache *lc, unsigned new_number)
{
	struct list_head *n;
	struct lc_element *e;

	if (!list_empty(&lc->free))
		n = lc->free.next;
	else if (!list_empty(&lc->lru))
		n = lc->lru.prev;
	else
		return NULL;

	e = list_entry(n, struct lc_element, list);
	PARANOIA_LC_ELEMENT(lc, e);

	e->lc_new_number = new_number;
	if (!hlist_unhashed(&e->colision))
		__hlist_del(&e->colision);
	hlist_add_head(&e->colision, lc_hash_slot(lc, new_number));
	list_move(&e->list, &lc->to_be_changed);

	return e;
}


/**
 * lc_create - prepares to track objects in an active set
 * @name: descriptive name only used in lc_seq_printf_stats and lc_seq_dump_details
 * @max_pending_changes: maximum changes to accumulate until a transaction is required
 * @e_count: number of elements allowed to be active simultaneously
 * @e_size: size of the tracked objects
 * @e_off: offset to the &struct lc_element member in a tracked object
 *
 * Returns a pointer to a newly initialized struct lru_cache on success,
 * or NULL on (allocation) failure.
 */
struct lru_cache *lc_create(const char *name, struct oryx_kmcache_t *cache,
			unsigned max_pending_changes,
			unsigned e_count, size_t e_size, size_t e_off)
{
	struct hlist_head *slot = NULL;
	struct lc_element **element = NULL;
	struct lru_cache *lc;
	struct lc_element *e;
	unsigned cache_obj_size = oryx_kc_size(cache);
	unsigned i;

	WARN_ON(cache_obj_size < e_size);
	if (cache_obj_size < e_size) {
		return NULL;
	}

	/* e_count too big; would probably fail the allocation below anyways.
	 * for typical use cases, e_count should be few thousand at most. */
	if (e_count > LC_MAX_ACTIVE)
		return NULL;

	slot = kmalloc (e_count * sizeof(struct hlist_head), MPF_CLR, __oryx_unused_val__);
	if (!slot)
		goto out_fail;
	element = kmalloc (e_count * sizeof(struct lc_element *), MPF_CLR, __oryx_unused_val__);
	if (!element)
		goto out_fail;

	lc = kmalloc (sizeof(*lc), MPF_CLR, __oryx_unused_val__);
	if (!lc)
		goto out_fail;

	INIT_LIST_HEAD(&lc->in_use);
	INIT_LIST_HEAD(&lc->lru);
	INIT_LIST_HEAD(&lc->free);
	INIT_LIST_HEAD(&lc->to_be_changed);

	lc->name = name;
	lc->element_size = e_size;
	lc->element_off = e_off;
	lc->nr_elements = e_count;
	lc->max_pending_changes = max_pending_changes;
	lc->lc_cache = cache;
	lc->lc_element = element;
	lc->lc_slot = slot;

	/* preallocate all objects */
	for (i = 0; i < e_count; i++) {
		void *p = oryx_kc_alloc(cache);
		if (!p)
			break;
		/** lc->element_size is equal with cache->obj_size.
		 *  kmalloc do real memset internal, so no need memset here. */
		memset(p, 0, lc->element_size);
		
		/** a point to object's lc_element structure. */
		/** e = p + e_off; */
		e = (void *)((size_t)p + e_off);
		printf ("tracked_object=%p, lc_element=%p\n", p, e);
		e->lc_index = i;
		e->lc_number = LC_FREE;
		e->lc_new_number = LC_FREE;
		/** add lc_element to lru_cache's free list. */
		list_add(&e->list, &lc->free);
		/** save lc_element in tracked object to current lru_cache->lc_element. */
		element[i] = e;
	}

	ASSERT (i == e_count);
	
	if (i == e_count) {
		return lc;
	}
	
	/** error if approached here. */
	printf ("%s, error, i=%d, e_count=%d\n", __func__, i, e_count);

	/* else: could not allocate all elements, give up */
	for (i--; i; i--) {
		void *p = element[i];
		/** oryx_kc_free(cache, p - e_off); */
		void *obj = (void *)((size_t)p - e_off);
		oryx_kc_free(cache, obj);
	}
	kfree(lc);
out_fail:
	kfree(element);
	kfree(slot);
	return NULL;
}

static struct lc_element *__lc_find(struct lru_cache *lc, unsigned int enr,
		bool include_changing)
{
	struct lc_element *e;

	BUG_ON(!lc);
	BUG_ON(!lc->nr_elements);
	hlist_for_each_entry(e, lc_hash_slot(lc, enr), colision) {
		/* "about to be changed" elements, pending transaction commit,
		 * are hashed by their "new number". "Normal" elements have
		 * lc_number == lc_new_number. */
		if (e->lc_new_number != enr)
			continue;
		if (e->lc_new_number == e->lc_number || include_changing)
			return e;
		break;
	}
	return NULL;
}

/**
 * lc_find - find element by label, if present in the hash table
 * @lc: The lru_cache object
 * @enr: element number
 *
 * Returns the pointer to an element, if the element with the requested
 * "label" or element number is present in the hash table,
 * or NULL if not found. Does not change the refcnt.
 * Ignores elements that are "about to be used", i.e. not yet in the active
 * set, but still pending transaction commit.
 */
struct lc_element *lc_find(struct lru_cache *lc, unsigned int enr)
{
	return __lc_find(lc, enr, 0);
}

static struct lc_element *__lc_get(struct lru_cache *lc, unsigned int enr, unsigned int flags)
{
	struct lc_element *e;

	PARANOIA_ENTRY();
	if (lc->flags & LC_STARVING) {
		++lc->starving;
		RETURN(NULL);
	}

	e = __lc_find(lc, enr, 1);
	/* if lc_new_number != lc_number,
	 * this enr is currently being pulled in already,
	 * and will be available once the pending transaction
	 * has been committed. */
	if (e) {
		if (e->lc_new_number != e->lc_number) {
			/* It has been found above, but on the "to_be_changed"
			 * list, not yet committed.  Don't pull it in twice,
			 * wait for the transaction, then try again...
			 */
			if (!(flags & LC_GET_MAY_USE_UNCOMMITTED))
				RETURN(NULL);
			/* ... unless the caller is aware of the implications,
			 * probably preparing a cumulative transaction. */
			++e->refcnt;
			++lc->hits;
			RETURN(e);
		}
		/* else: lc_new_number == lc_number; a real hit. */
		++lc->hits;
		if (e->refcnt++ == 0)
			lc->used++;
		list_move(&e->list, &lc->in_use); /* Not evictable... */
		RETURN(e);
	}
	/* e == NULL */

	++lc->misses;
	if (!(flags & LC_GET_MAY_CHANGE))
		RETURN(NULL);

	/* To avoid races with lc_try_lock(), first, mark us dirty
	 * (using test_and_set_bit, as it implies memory barriers), ... */
	__test_and_set_bit(__LC_DIRTY, &lc->flags);

	/* ... only then check if it is locked anyways. If lc_unlock clears
	 * the dirty bit again, that's not a problem, we will come here again.
	 */
	if (test_bit(__LC_LOCKED, &lc->flags)) {
		++lc->locked;
		RETURN(NULL);
	}

	/* In case there is nothing available and we can not kick out
	 * the LRU element, we have to wait ...
	 */
	if (!lc_unused_element_available(lc)) {
		__set_bit(__LC_STARVING, &lc->flags);
		RETURN(NULL);
	}

	/* It was not present in the active set.  We are going to recycle an
	 * unused (or even "free") element, but we won't accumulate more than
	 * max_pending_changes changes.  */
	if (lc->pending_changes >= lc->max_pending_changes)
		RETURN(NULL);

	e = lc_prepare_for_change(lc, enr);
	BUG_ON(!e);

	clear_bit(__LC_STARVING, &lc->flags);
	BUG_ON(++e->refcnt != 1);
	lc->used++;
	lc->pending_changes++;

	RETURN(e);
}

/**
 * lc_get - get element by label, maybe change the active set
 * @lc: the lru cache to operate on
 * @enr: the label to look up
 *
 * Finds an element in the cache, increases its usage count,
 * "touches" and returns it.
 *
 * In case the requested number is not present, it needs to be added to the
 * cache. Therefore it is possible that an other element becomes evicted from
 * the cache. In either case, the user is notified so he is able to e.g. keep
 * a persistent log of the cache changes, and therefore the objects in use.
 *
 * Return values:
 *  NULL
 *     The cache was marked %LC_STARVING,
 *     or the requested label was not in the active set
 *     and a changing transaction is still pending (@lc was marked %LC_DIRTY).
 *     Or no unused or free element could be recycled (@lc will be marked as
 *     %LC_STARVING, blocking further lc_get() operations).
 *
 *  pointer to the element with the REQUESTED element number.
 *     In this case, it can be used right away
 *
 *  pointer to an UNUSED element with some different element number,
 *          where that different number may also be %LC_FREE.
 *
 *          In this case, the cache is marked %LC_DIRTY,
 *          so lc_try_lock() will no longer succeed.
 *          The returned element pointer is moved to the "to_be_changed" list,
 *          and registered with the new element number on the hash collision chains,
 *          so it is possible to pick it up from lc_is_used().
 *          Up to "max_pending_changes" (see lc_create()) can be accumulated.
 *          The user now should do whatever housekeeping is necessary,
 *          typically serialize on lc_try_lock_for_transaction(), then call
 *          lc_committed(lc) and lc_unlock(), to finish the change.
 *
 * NOTE: The user needs to check the lc_number on EACH use, so he recognizes
 *       any cache set change.
 */
struct lc_element *lc_get(struct lru_cache *lc, unsigned int enr)
{
	return __lc_get(lc, enr, LC_GET_MAY_CHANGE);
}

/**
 * lc_get_cumulative - like lc_get; also finds to-be-changed elements
 * @lc: the lru cache to operate on
 * @enr: the label to look up
 *
 * Unlike lc_get this also returns the element for @enr, if it is belonging to
 * a pending transaction, so the return values are like for lc_get(),
 * plus:
 *
 * pointer to an element already on the "to_be_changed" list.
 * 	In this case, the cache was already marked %LC_DIRTY.
 *
 * Caller needs to make sure that the pending transaction is completed,
 * before proceeding to actually use this element.
 */
struct lc_element *lc_get_cumulative(struct lru_cache *lc, unsigned int enr)
{
	return __lc_get(lc, enr, LC_GET_MAY_CHANGE|LC_GET_MAY_USE_UNCOMMITTED);
}

/**
 * lc_try_get - get element by label, if present; do not change the active set
 * @lc: the lru cache to operate on
 * @enr: the label to look up
 *
 * Finds an element in the cache, increases its usage count,
 * "touches" and returns it.
 *
 * Return values:
 *  NULL
 *     The cache was marked %LC_STARVING,
 *     or the requested label was not in the active set
 *
 *  pointer to the element with the REQUESTED element number.
 *     In this case, it can be used right away
 */
struct lc_element *lc_try_get(struct lru_cache *lc, unsigned int enr)
{
	return __lc_get(lc, enr, 0);
}

/**
 * lc_committed - tell @lc that pending changes have been recorded
 * @lc: the lru cache to operate on
 *
 * User is expected to serialize on explicit lc_try_lock_for_transaction()
 * before the transaction is started, and later needs to lc_unlock() explicitly
 * as well.
 */
void lc_committed(struct lru_cache *lc)
{
	struct lc_element *e, *tmp;

	PARANOIA_ENTRY();
	list_for_each_entry_safe(e, tmp, &lc->to_be_changed, list) {
		/* count number of changes, not number of transactions */
		++lc->changed;
		e->lc_number = e->lc_new_number;
		list_move(&e->list, &lc->in_use);
	}
	lc->pending_changes = 0;
	RETURN();
}

/**
 * lc_put - give up refcnt of @e
 * @lc: the lru cache to operate on
 * @e: the element to put
 *
 * If refcnt reaches zero, the element is moved to the lru list,
 * and a %LC_STARVING (if set) is cleared.
 * Returns the new (post-decrement) refcnt.
 */
unsigned int lc_put(struct lru_cache *lc, struct lc_element *e)
{
	PARANOIA_ENTRY();
	PARANOIA_LC_ELEMENT(lc, e);
	BUG_ON(e->refcnt == 0);
	BUG_ON(e->lc_number != e->lc_new_number);
	if (--e->refcnt == 0) {
		/* move it to the front of LRU. */
		list_move(&e->list, &lc->lru);
		lc->used--;
		clear_bit_unlock(__LC_STARVING, &lc->flags);
	}
	RETURN(e->refcnt);
}

/**
 * lc_element_by_index
 * @lc: the lru cache to operate on
 * @i: the index of the element to return
 */
struct lc_element *lc_element_by_index(struct lru_cache *lc, unsigned i)
{
	BUG_ON(i >= lc->nr_elements);
	BUG_ON(lc->lc_element[i] == NULL);
	BUG_ON(lc->lc_element[i]->lc_index != i);
	return lc->lc_element[i];
}

/**
 * lc_index_of
 * @lc: the lru cache to operate on
 * @e: the element to query for its index position in lc->element
 */
unsigned int lc_index_of(struct lru_cache *lc, struct lc_element *e)
{
	PARANOIA_LC_ELEMENT(lc, e);
	return e->lc_index;
}

/**
 * lc_set - associate index with label
 * @lc: the lru cache to operate on
 * @enr: the label to set
 * @index: the element index to associate label with.
 *
 * Used to initialize the active set to some previously recorded state.
 */
void lc_set(struct lru_cache *lc, unsigned int enr, u32 index)
{
	struct lc_element *e;
	struct list_head *lh;

	if ((int)index < 0 || index >= lc->nr_elements)
		return;

	e = lc_element_by_index(lc, index);
	BUG_ON(e->lc_number != e->lc_new_number);
	BUG_ON(e->refcnt != 0);

	e->lc_number = e->lc_new_number = enr;
	hlist_del_init(&e->colision);
	if (enr == LC_FREE)
		lh = &lc->free;
	else {
		hlist_add_head(&e->colision, lc_hash_slot(lc, enr));
		lh = &lc->lru;
	}
	list_move(&e->list, lh);
}

/**
 * lc_dump - Dump a complete LRU cache to seq in textual form.
 * @lc: the lru cache to operate on
 * @seq: the &struct seq_file pointer to seq_printf into
 * @utext: user supplied additional "heading" or other info
 * @detail: function pointer the user may provide to dump further details
 * of the object the lc_element is embedded in. May be NULL.
 * Note: a leading space ' ' and trailing newline '\n' is implied.
 */
void lc_seq_dump_details(FILE *seq, struct lru_cache *lc, char *utext,
	     void (*detail) (FILE *, struct lc_element *))
{
	unsigned int nr_elements = lc->nr_elements;
	struct lc_element *e;
	u32 i;

	fprintf(seq, "\tnn: lc_number (new nr) refcnt %s\n ", utext);
	for (i = 0; i < nr_elements; i++) {
		e = lc_element_by_index(lc, i);
		if (e->lc_number != e->lc_new_number)
			fprintf(seq, "\t%5d: %6d %8d %6d ",
				i, e->lc_number, e->lc_new_number, e->refcnt);
		else
			fprintf(seq, "\t%5d: %6d %-8s %6d ",
				i, e->lc_number, "-\"-", e->refcnt);
		if (detail)
			detail(seq, e);
		fprintf(seq, "\n");
	}
}


