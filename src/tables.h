#ifndef __TABLE_H__
#define __TABLE_H__

struct table_t {
	/** vector used to make ID for element. */
	vector vec;
	/** hash table used to lookup alias for element within this table. */
	struct oryx_htable_t *ht;
	os_lock_t ol_vec_lock;
	os_lock_t ol_ht_lock;
	int (*lock_fn)(os_lock_t *lock);
	int (*unlock_fn)(os_lock_t *lock);
};

#define TABLE_NAME(name) (name##_table)
#define TABLE(table) ((struct table_t *) (&(TABLE_NAME(table))))
#define TABLE_(table) ((struct table_t *) (&(TABLE_NAME(table))))
#define EXTERN_TABLE(table) extern struct table_t TABLE_NAME(table);

#define INIT_TABLE(table, vector_table, hash_table)\
	TABLE(table)->vec = vector_table;\
	TABLE(table)->ht = hash_table;

#define DECLARE_TABLE(table)\
	struct table_t TABLE_NAME(table) = {\
	.vec = NULL,\
	.ht = NULL,\
	.ol_vec_lock = INIT_MUTEX_VAL,\
	.ol_ht_lock = INIT_MUTEX_VAL,\
	.lock_fn = &oryx_thread_mutex_lock,\
	.unlock_fn = &oryx_thread_mutex_unlock\
	}

#define DECLARE_FUNC(func, type)\
	void __##func##_##type (void)
	
#define DECLARE_INIT_FUNC(func)\
	DECLARE_FUNC(func, init)

#define CALL_FUNC(func, type)\
	 __##func##_##type ()
	
#endif

