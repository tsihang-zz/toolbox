#ifndef __ATOMIC_H__
#define __ATOMIC_H__


typedef struct{
#define ATOMIC_SIZE sizeof(atomic_t)
	oryx_int32_t counter;
}atomic_t;

typedef struct{
#define ATOMIC64_SIZE sizeof(atomic64_t)
	int64_t counter;
}atomic64_t;

#define ATOMIC_INIT(i)	{ (i) }


/**
 * atomic_set - set atomic variable
 * @v: pointer of type atomic_t
 * @i: required value
 *
 * Atomically sets the value of @v to @i.
 */
#define atomic_set(v, i) (((v)->counter) = (i))


/**
 * atomic_read - read atomic variable
 * @v: pointer of type atomic_t
 *
 * Atomically reads the value of @v.
 */
#define atomic_read(v)	(*(volatile oryx_int32_t *)&(v)->counter)

static __oryx_always_inline__
oryx_int32_t atomic_add (volatile atomic_t *v, oryx_int32_t val)
{
	return (oryx_int32_t)__sync_add_and_fetch (&v->counter, val);
}

static __oryx_always_inline__
oryx_int32_t atomic_sub (volatile atomic_t *v, oryx_int32_t val)
{
	return (oryx_int32_t)__sync_sub_and_fetch (&v->counter, val);
}

static __oryx_always_inline__
oryx_int32_t atomic_inc (volatile atomic_t *v)
{
	return atomic_add (v, (oryx_int32_t)1);
}

static __oryx_always_inline__
oryx_int32_t atomic_dec (volatile atomic_t *v)
{
	return atomic_sub (v, (oryx_int32_t)1);
}


/**
 * atomic_set - set atomic variable
 * @v: pointer of type atomic_t
 * @i: required value
 *
 * Atomically sets the value of @v to @i.
 */
#define atomic64_set(v, i) (((v)->counter) = (i))

/**
 * atomic_read - read atomic variable
 * @v: pointer of type atomic_t
 *
 * Atomically reads the value of @v.
 */
#define atomic64_read(v)	(*(volatile oryx_int64_t *)&(v)->counter)

static __oryx_always_inline__
oryx_int64_t atomic64_add (volatile atomic64_t *v, oryx_int64_t val)
{
	return (oryx_int64_t)__sync_add_and_fetch (&v->counter, val);
}

static __oryx_always_inline__
oryx_int64_t atomic64_sub (volatile atomic64_t *v, oryx_int64_t val)
{
	return (oryx_int64_t)__sync_sub_and_fetch (&v->counter, val);
}

static __oryx_always_inline__
oryx_int64_t atomic64_inc (volatile atomic64_t *v)
{
	return atomic64_add (v, (oryx_int64_t)1);
}

static __oryx_always_inline__
oryx_int64_t atomic64_dec (volatile atomic64_t *v)
{
	return atomic64_sub (v, (oryx_int64_t)1);
}

#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))
#define BIT_MASK(nr)		(1UL << ((nr) % __BITS_PER_LONG))
#define BIT_WORD(nr)		((nr) / __BITS_PER_LONG)
#define BITS_PER_BYTE		8
#define BITS_TO_LONGS(nr)	DIV_ROUND_UP(nr, BITS_PER_BYTE * sizeof(long))


static __oryx_always_inline__
void set_bit(int nr, unsigned long *addr)
{
	addr[BIT_WORD(nr)] |= 1UL << (nr % __BITS_PER_LONG);
}

static __oryx_always_inline__
void clear_bit(int nr, unsigned long *addr)
{
	addr[BIT_WORD(nr)] &= ~(1UL << (nr % __BITS_PER_LONG));
}

/**
 * test_bit - Determine whether a bit is set
 * @nr: bit number to test
 * @addr: Address to start counting from
 */
static __oryx_always_inline__
int test_bit(int nr, const unsigned long *addr)
{
	return 1UL & (addr[BIT_WORD(nr)] >> (nr & (__BITS_PER_LONG-1)));
}


static __oryx_always_inline__
void clear_bit_unlock(long nr, unsigned long *addr)
{
	//barrier();
	clear_bit(nr, addr);
}

/**
 * __set_bit - Set a bit in memory
 * @nr: the bit to set
 * @addr: the address to start counting from
 *
 * Unlike set_bit(), this function is non-atomic and may be reordered.
 * If it's called on the same region of memory simultaneously, the effect
 * may be that only one operation succeeds.
 */
static __oryx_always_inline__
void __set_bit(int nr, unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);

	*p  |= mask;
}

static __oryx_always_inline__
void __clear_bit(int nr, unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);

	*p &= ~mask;
}

/**
 * __change_bit - Toggle a bit in memory
 * @nr: the bit to change
 * @addr: the address to start counting from
 *
 * Unlike change_bit(), this function is non-atomic and may be reordered.
 * If it's called on the same region of memory simultaneously, the effect
 * may be that only one operation succeeds.
 */
static __oryx_always_inline__
void __change_bit(int nr, unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);

	*p ^= mask;
}

/**
 * __test_and_set_bit - Set a bit and return its old value
 * @nr: Bit to set
 * @addr: Address to count from
 *
 * This operation is non-atomic and can be reordered.
 * If two examples of this operation race, one can appear to succeed
 * but actually fail.  You must protect multiple accesses with a lock.
 */
static __oryx_always_inline__
int __test_and_set_bit(int nr, unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);
	unsigned long old = *p;

	*p = old | mask;
	return (old & mask) != 0;
}

/**
 * __test_and_clear_bit - Clear a bit and return its old value
 * @nr: Bit to clear
 * @addr: Address to count from
 *
 * This operation is non-atomic and can be reordered.
 * If two examples of this operation race, one can appear to succeed
 * but actually fail.  You must protect multiple accesses with a lock.
 */
static __oryx_always_inline__
int __test_and_clear_bit(int nr, unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);
	unsigned long old = *p;

	*p = old & ~mask;
	return (old & mask) != 0;
}

/* WARNING: non atomic and it can be reordered! */
static __oryx_always_inline__
int __test_and_change_bit(int nr, unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);
	unsigned long old = *p;

	*p = old ^ mask;
	return (old & mask) != 0;
}

/**
 * __ffs - find first bit in word.
 * @word: The word to search
 *
 * Undefined if no bit exists, so code should check against 0 first.
 */
static __oryx_always_inline__
unsigned long __ffs(unsigned long word)
{
	int num = 0;

	if ((word & 0xffffffff) == 0) {
		num += 32;
		word >>= 32;
	}
	if ((word & 0xffff) == 0) {
		num += 16;
		word >>= 16;
	}
	if ((word & 0xff) == 0) {
		num += 8;
		word >>= 8;
	}
	if ((word & 0xf) == 0) {
		num += 4;
		word >>= 4;
	}
	if ((word & 0x3) == 0) {
		num += 2;
		word >>= 2;
	}
	if ((word & 0x1) == 0)
		num += 1;
	return num;
}

#if defined(HAVE_APR_H)
oryx_status_t oryx_atomic_initialize(oryx_pool_t __oryx_unused__ *p);
#else
oryx_status_t oryx_atomic_initialize(void __oryx_unused__ *p);
#endif



#endif

