#ifndef __HASH_TABLE_H__
#define __HASH_TABLE_H__

#define DEFAULT_HASH_CHAIN_SIZE	(1 << 10)

struct oryx_hbucket_t {
	ht_value_t		data;
	ht_key_t		key;			/** key = hash(value) */	
	uint32_t		ul_d_size;		/** sizeof (data) */
	struct oryx_hbucket_t *next;	/** conflict chain. */
};

#define HTABLE_SYNCHRONIZED	(1 << 0)	/* Synchronized hash table. 
	 									 * Caller can use the hash table safely
	 									 * without maintaining a thread-safe-lock. */
#define HTABLE_PRINT_INFO	(1 << 1)

struct oryx_htable_t {
	struct oryx_hbucket_t	**array;
	int						array_size;		/* sizeof hash bucket */
	int						active_count;	/* total of instance stored in bucket,
										 	 * maybe great than array_size in future.
								 		 	 */
	uint32_t			ul_flags;

	os_mutex_t			*os_lock;
	oryx_status_t		(*ht_lock_fn)(os_mutex_t *lock);
	oryx_status_t		(*ht_unlock_fn)(os_mutex_t *lock);

	ht_key_t (*hash_fn)(struct oryx_htable_t *,
						const ht_value_t,
						uint32_t);		/* function for create a hash value
										 * with the given parameter *v
										 */
	int (*cmp_fn)(const ht_value_t,
				  uint32_t,
				  const ht_value_t,
				  uint32_t);			/* 0: equal,
										 * otherwise a value less than zero returned.
										 */
										 
	void (*free_fn)(const ht_value_t);	/* function for vlaue of
										 * oryx_hbucket_t releasing.
										 */
};

#define HTABLE_LOCK(ht)\
	if((ht)->ul_flags & HTABLE_SYNCHRONIZED)\
		do_mutex_lock((ht)->os_lock);

#define HTABLE_UNLOCK(ht)\
	if((ht)->ul_flags & HTABLE_SYNCHRONIZED)\
		do_mutex_unlock((ht)->os_lock);

#define htable_active_slots(ht)\
	((ht)->array_size)
	
#define htable_active_elements(ht)\
	((ht)->active_count)

static __oryx_always_inline__
uint32_t oryx_js_hash(const char* str, unsigned int len)  
{  
   unsigned int hash = 1315423911;  
   unsigned int i    = 0;  
   for(i = 0; i < len; str++, i++)  
   {  
      hash ^= ((hash << 5) + (*str) + (hash >> 2));  
   }  
   return hash;  
}  



#endif
