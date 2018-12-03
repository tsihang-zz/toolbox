#ifndef __HASH_TABLE_H__
#define __HASH_TABLE_H__

#define DEFAULT_HASH_CHAIN_SIZE	(1 << 10)

struct oryx_hbucket_t {
	ht_value_t		value;
	uint32_t		valen;	/** sizeof (data) */
	ht_key_t		key;	/** key = hash(value) */		
	struct oryx_hbucket_t *next;	/** conflict chain. */
};

#define HTABLE_SYNCHRONIZED	(1 << 0)	/* Synchronized hash table. 
	 					 * Caller can use the hash table safely
	 					 * without maintaining a thread-safe-lock. */
#define HTABLE_PRINT_INFO	(1 << 1)

struct oryx_htable_t {
	struct oryx_hbucket_t	**array;
	int			array_size;	/* sizeof hash bucket */
	int			active_count;	/* total of instance stored in bucket,
						 * maybe great than array_size in future. */
	uint32_t		ul_flags;

	os_mutex_t		*os_lock;
	int		(*ht_lock_fn)(os_mutex_t *lock);
	int		(*ht_unlock_fn)(os_mutex_t *lock);

	ht_key_t (*hash_fn)(struct oryx_htable_t *,
				const ht_value_t, uint32_t);	/* function for create a hash value
								 * with the given parameter *v */
	int (*cmp_fn)(const ht_value_t,
				  uint32_t,
				  const ht_value_t,
				  uint32_t);			/* 0: equal,
								 * otherwise a value less than zero returned. */
										 
	void (*free_fn)(const ht_value_t);	/* function for vlaue of
						 * oryx_hbucket_t releasing. */
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
uint32_t oryx_js_hash
(
	IN const char* str,
	OUT unsigned int len
)  
{
	unsigned int hash = 1315423911;
	unsigned int i    = 0;

	for(i = 0; i < len; str++, i++)  
      		hash ^= ((hash << 5) + (*str) + (hash >> 2));

	return hash;  
}  

/*
 * My best guess at if you are big-endian or little-endian.  This may
 * need adjustment.
 */
#if (defined(__BYTE_ORDER) && defined(__LITTLE_ENDIAN) && \
     __BYTE_ORDER == __LITTLE_ENDIAN) || \
    (defined(i386) || defined(__i386__) || defined(__i486__) || \
     defined(__i586__) || defined(__i686__) || defined(vax) || defined(MIPSEL))
# define HASH_LITTLE_ENDIAN 1
# define HASH_BIG_ENDIAN 0
#elif (defined(__BYTE_ORDER) && defined(__BIG_ENDIAN) && \
       __BYTE_ORDER == __BIG_ENDIAN) || \
      (defined(sparc) || defined(POWERPC) || defined(mc68000) || defined(sel))
# define HASH_LITTLE_ENDIAN 0
# define HASH_BIG_ENDIAN 1
#else
# define HASH_LITTLE_ENDIAN 0
# define HASH_BIG_ENDIAN 0
#endif

#define rot(x,k) (((x)<<(k)) | ((x)>>(32-(k))))
/*
-------------------------------------------------------------------------------
mix -- mix 3 32-bit values reversibly.

This is reversible, so any information in (a,b,c) before mix() is
still in (a,b,c) after mix().

If four pairs of (a,b,c) inputs are run through mix(), or through
mix() in reverse, there are at least 32 bits of the output that
are sometimes the same for one pair and different for another pair.
This was tested for:
* pairs that differed by one bit, by two bits, in any combination
  of top bits of (a,b,c), or in any combination of bottom bits of
  (a,b,c).
* "differ" is defined as +, -, ^, or ~^.  For + and -, I transformed
  the output delta to a Gray code (a^(a>>1)) so a string of 1's (as
  is commonly produced by subtraction) look like a single 1-bit
  difference.
* the base values were pseudorandom, all zero but one bit set, or 
  all zero plus a counter that starts at zero.

Some k values for my "a-=c; a^=rot(c,k); c+=b;" arrangement that
satisfy this are
    4  6  8 16 19  4
    9 15  3 18 27 15
   14  9  3  7 17  3
Well, "9 15 3 18 27 15" didn't quite get 32 bits diffing
for "differ" defined as + with a one-bit base and a two-bit delta.  I
used http://burtleburtle.net/bob/hash/avalanche.html to choose 
the operations, constants, and arrangements of the variables.

This does not achieve avalanche.  There are input bits of (a,b,c)
that fail to affect some output bits of (a,b,c), especially of a.  The
most thoroughly mixed value is c, but it doesn't really even achieve
avalanche in c.

This allows some parallelism.  Read-after-writes are good at doubling
the number of bits affected, so the goal of mixing pulls in the opposite
direction as the goal of parallelism.  I did what I could.  Rotates
seem to cost as much as shifts on every machine I could lay my hands
on, and rotates are much kinder to the top and bottom bits, so I used
rotates.
-------------------------------------------------------------------------------
*/
#define mix(a,b,c) \
{ \
  a -= c;  a ^= rot(c, 4);  c += b; \
  b -= a;  b ^= rot(a, 6);  a += c; \
  c -= b;  c ^= rot(b, 8);  b += a; \
  a -= c;  a ^= rot(c,16);  c += b; \
  b -= a;  b ^= rot(a,19);  a += c; \
  c -= b;  c ^= rot(b, 4);  b += a; \
}

/*
-------------------------------------------------------------------------------
final -- final mixing of 3 32-bit values (a,b,c) into c

Pairs of (a,b,c) values differing in only a few bits will usually
produce values of c that look totally different.  This was tested for
* pairs that differed by one bit, by two bits, in any combination
  of top bits of (a,b,c), or in any combination of bottom bits of
  (a,b,c).
* "differ" is defined as +, -, ^, or ~^.  For + and -, I transformed
  the output delta to a Gray code (a^(a>>1)) so a string of 1's (as
  is commonly produced by subtraction) look like a single 1-bit
  difference.
* the base values were pseudorandom, all zero but one bit set, or 
  all zero plus a counter that starts at zero.

These constants passed:
 14 11 25 16 4 14 24
 12 14 25 16 4 14 24
and these came close:
  4  8 15 26 3 22 24
 10  8 15 26 3 22 24
 11  8 15 26 3 22 24
-------------------------------------------------------------------------------
*/
#define final(a,b,c) \
{ \
  c ^= b; c -= rot(b,14); \
  a ^= c; a -= rot(c,11); \
  b ^= a; b -= rot(a,25); \
  c ^= b; c -= rot(b,16); \
  a ^= c; a -= rot(c,4);  \
  b ^= a; b -= rot(a,14); \
  c ^= b; c -= rot(b,24); \
}


#endif
