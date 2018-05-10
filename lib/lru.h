#ifndef __LRU_H__
#define __LRU_H__


struct lc_element {
	struct hlist_node colision;
	struct list_head list;		 /* LRU list or free list */

	/* refcnt to times this lc_element used. */
	unsigned refcnt;
	/* back "pointer" into lc_cache->element[index],
	 * for paranoia, and for "lc_element_to_index" */
	unsigned lc_index;
	/* if we want to track a larger set of objects,
	 * it needs to become arch independend u64 */
	unsigned lc_number;
	/* special label when on free list */
#define LC_FREE (~0U)

	/* for pending changes */
	unsigned lc_new_number;
};

struct lru_cache {
	/* the least recently used item is kept at lru->prev */
	struct list_head lru;
	struct list_head free;
	struct list_head in_use;
	struct list_head to_be_changed;

	/* the pre-created kmem cache to allocate the objects from */
	struct oryx_kmcache_t *lc_cache;

	/* size of tracked objects, used to memset(,0,) them in lc_reset */
	size_t element_size;
	/* offset of struct lc_element member in the tracked object */
	size_t element_off;

	/* number of elements (indices) */
	u32 nr_elements;
	/* Arbitrary limit on maximum tracked objects. Practical limit is much
	 * lower due to allocation failures, probably. For typical use cases,
	 * nr_elements should be a few thousand at most.
	 * This also limits the maximum value of lc_element.lc_index, allowing the
	 * 8 high bits of .lc_index to be overloaded with flags in the future. */
#define LC_MAX_ACTIVE	(1<<24)

	/* allow to accumulate a few (index:label) changes,
	 * but no more than max_pending_changes */
	u32 max_pending_changes;
	/* number of elements currently on to_be_changed list */
	u32 pending_changes;

	/* statistics */
	u32 used; /* number of elements currently on in_use list */
	unsigned long hits, misses, starving, locked, changed;

	/* see below: flag-bits for lru_cache */
	unsigned long flags;


	void  *lc_private;
	const char *name;

	/* nr_elements there */
	struct hlist_head *lc_slot;
	struct lc_element **lc_element;
};

/* flag-bits for lru_cache */
enum lc_flag {
	/* debugging aid, to catch concurrent access early.
	 * user needs to guarantee exclusive access by proper locking! */
	__LC_PARANOIA,

	/* annotate that the set is "dirty", possibly accumulating further
	 * changes, until a transaction is finally triggered */
	__LC_DIRTY,

	/* Locked, no further changes allowed.
	 * Also used to serialize changing transactions. */
	__LC_LOCKED,

	/* if we need to change the set, but currently there is no free nor
	 * unused element available, we are "starving", and must not give out
	 * further references, to guarantee that eventually some refcnt will
	 * drop to zero and we will be able to make progress again, changing
	 * the set, writing the transaction.
	 * if the statistics say we are frequently starving,
	 * nr_elements is too small. */
	__LC_STARVING,
};

#define LC_PARANOIA (1<<__LC_PARANOIA)
#define LC_DIRTY    (1<<__LC_DIRTY)
#define LC_LOCKED   (1<<__LC_LOCKED)
#define LC_STARVING (1<<__LC_STARVING)

extern struct lru_cache *lc_create(const char *name, struct oryx_kmcache_t *cache,
			unsigned max_pending_changes,
			unsigned e_count, size_t e_size, size_t e_off);
extern void lc_reset(struct lru_cache *lc);
extern void lc_destroy(struct lru_cache *lc);
extern void lc_set(struct lru_cache *lc, unsigned int enr, u32 index);
extern void lc_del(struct lru_cache *lc, struct lc_element *element);

extern struct lc_element *lc_get_cumulative(struct lru_cache *lc, unsigned int enr);
extern struct lc_element *lc_try_get(struct lru_cache *lc, unsigned int enr);
extern struct lc_element *lc_find(struct lru_cache *lc, unsigned int enr);
extern struct lc_element *lc_get(struct lru_cache *lc, unsigned int enr);
extern unsigned int lc_put(struct lru_cache *lc, struct lc_element *e);
extern void lc_committed(struct lru_cache *lc);
extern void lc_seq_dump_details(FILE *seq, struct lru_cache *lc, char *utext,
	     void (*detail) (FILE *, struct lc_element *));
extern void lc_seq_printf_stats(FILE *seq, struct lru_cache *lc);
extern unsigned int lc_index_of(struct lru_cache *lc, struct lc_element *e);
extern struct lc_element *lc_element_by_index(struct lru_cache *lc, unsigned i);

#define lc_entry(ptr, type, member) \
	container_of(ptr, type, member)


#endif	/** END of __LRU_H__ */
