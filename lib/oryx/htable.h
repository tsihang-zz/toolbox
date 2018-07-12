#ifndef __HASH_TABLE_H__
#define __HASH_TABLE_H__

#include <stdint.h>

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

#define htable_active_slots(ht)\
	((ht)->array_size)
	
#define htable_active_elements(ht)\
	((ht)->active_count)

#define HASH_JEN_MIX(a,b,c)                                             \
    do {                                                                            \
        a -= b; a -= c; a ^= ( c >> 13 );                               \
        b -= c; b -= a; b ^= ( a << 8 );                                \
        c -= a; c -= b; c ^= ( b >> 13 );                               \
        a -= b; a -= c; a ^= ( c >> 12 );                               \
        b -= c; b -= a; b ^= ( a << 16 );                               \
        c -= a; c -= b; c ^= ( b >> 5 );                                \
        a -= b; a -= c; a ^= ( c >> 3 );                                \
        b -= c; b -= a; b ^= ( a << 10 );                               \
        c -= a; c -= b; c ^= ( b >> 15 );                               \
    } while (0)
    

static __oryx_always_inline__ uint32_t 
hash_data(void *key, int keylen)
{
    unsigned _hj_i, _hj_j, _hj_k;
    char *_hj_key = (char *) key;
    uint32_t hashv = 0xfeedbeef;
    _hj_i = _hj_j = 0x9e3779b9;
    _hj_k = keylen;

    while (_hj_k >= 12)
    {
        _hj_i += (_hj_key[0] + ((unsigned) _hj_key[1] << 8)
                  + ((unsigned) _hj_key[2] << 16)
                  + ((unsigned) _hj_key[3] << 24));
        _hj_j += (_hj_key[4] + ((unsigned) _hj_key[5] << 8)
                  + ((unsigned) _hj_key[6] << 16)
                  + ((unsigned) _hj_key[7] << 24));
        hashv += (_hj_key[8] + ((unsigned) _hj_key[9] << 8)
                  + ((unsigned) _hj_key[10] << 16)
                  + ((unsigned) _hj_key[11] << 24));
        HASH_JEN_MIX(_hj_i, _hj_j, hashv);
        _hj_key += 12;
        _hj_k -= 12;
    }

    hashv += keylen;

    switch (_hj_k)
    {
        case 11:
            hashv += ((unsigned) _hj_key[10] << 24);

        case 10:
            hashv += ((unsigned) _hj_key[9] << 16);

        case 9:
            hashv += ((unsigned) _hj_key[8] << 8);

        case 8:
            _hj_j += ((unsigned) _hj_key[7] << 24);

        case 7:
            _hj_j += ((unsigned) _hj_key[6] << 16);

        case 6:
            _hj_j += ((unsigned) _hj_key[5] << 8);

        case 5:
            _hj_j += _hj_key[4];

        case 4:
            _hj_i += ((unsigned) _hj_key[3] << 24);

        case 3:
            _hj_i += ((unsigned) _hj_key[2] << 16);

        case 2:
            _hj_i += ((unsigned) _hj_key[1] << 8);

        case 1:
            _hj_i += _hj_key[0];
    }

    HASH_JEN_MIX(_hj_i, _hj_j, hashv);
    return hashv;
}

struct oryx_htable_t* oryx_htable_init (uint32_t max_buckets, 
	ht_key_t (*hash_fn)(struct oryx_htable_t *, ht_value_t, uint32_t), 
	int (*cmp_fn)(ht_value_t, uint32_t, ht_value_t, uint32_t), 
	void (*free_fn)(ht_value_t),
	uint32_t flags);

extern void oryx_htable_destroy(struct oryx_htable_t *ht);
extern void oryx_htable_print(struct oryx_htable_t *ht);
extern int oryx_htable_add(struct oryx_htable_t *ht, ht_value_t data, uint32_t datalen);
extern int oryx_htable_del(struct oryx_htable_t *ht, ht_value_t data, uint32_t datalen);
extern void *oryx_htable_lookup(struct oryx_htable_t *ht, ht_value_t data, uint32_t datalen);

extern int oryx_htable_foreach_elem(
									struct oryx_htable_t *ht,
									void (*handler)(ht_value_t, uint32_t, void *, int),
									void *opaque,
									int opaque_size
);

#endif
