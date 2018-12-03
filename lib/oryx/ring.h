#ifndef LOCKLESS_RING_H
#define LOCKLESS_RING_H

#define RLOCK_INIT(r)\
		do_mutex_init(&(r)->m)

#define RLOCK_DESTROY(r)\
		do_mutex_destroy(&(r)->m)

#define RLOCK_TRYLOCK(r)\
		do_mutex_trylock(&(r)->m)

#define RLOCK_LOCK(r)\
		do_mutex_lock(&(r)->m)

#define RLOCK_UNLOCK(r)\
		do_mutex_unlock(&(r)->m)

struct oryx_ring_data_t {
	uint32_t	s0: 16;
	uint32_t	s: 16;
	void	*v;
};

struct oryx_ring_t {
	const char			*name;
	int					max_elements;
	key_t					key;			/* for different progress. */

	ATOMIC_DECLARE(uint64_t, wp);
	ATOMIC_DECLARE(uint64_t, rp);
	
	struct oryx_ring_data_t		*data;

	uint32_t			ul_flags;
	uint64_t			nr_times_rd,
						nr_times_wr,
						nr_times_f;	/* full times */

	os_mutex_t			m;			/* lockless .*/
};
#define	ring_element_next(r,rw)	(((rw) + 1) % (r)->max_elements)

static __oryx_always_inline__
int oryx_ring_get
(
	IN struct oryx_ring_t *ring,
	OUT void **value,
	OUT uint16_t *valen
)
{
	uint64_t	rp = 0,
				wp = 0;
	
#if defined(BUILD_DEBUG)
	BUG_ON(ring == NULL);
#endif

	(*value)	= NULL;
	(*valen)	= 0;

	//RLOCK_LOCK(ring);

	rp = atomic_add(ring->rp, 0);
	wp = atomic_add(ring->wp, 0);
	if(rp == wp) {
		//RLOCK_UNLOCK(ring);
		return -1;
	}

	/* load and hold the data pointer from ring. 
	 * zero-copy. */
	(*value)	= ring->data[rp].v;
	(*valen)	= ring->data[rp].s;
	//ring->rp	= ring_element_next(ring, ring->rp);
	atomic_set(ring->rp, ring_element_next(ring, rp));
	ring->nr_times_rd ++;

	//RLOCK_UNLOCK(ring);

	return 0;
}

static __oryx_always_inline__
int oryx_ring_put
(
	IN struct oryx_ring_t *ring,
	IN void *value,
	IN uint16_t valen
)
{
	uint64_t	rp = 0,
				wp = 0;

	
#if defined(BUILD_DEBUG)
	BUG_ON(ring == NULL);
#endif

	//RLOCK_LOCK(ring);

	rp = atomic_add(ring->rp, 0);
	wp = atomic_add(ring->wp, 0);

	if(ring_element_next(ring, wp) == rp) {
		//RLOCK_UNLOCK(ring); 
		ring->nr_times_f ++;
		return -1;
	}

	/* zero-copy.
	 * store external data point to ring element. */
	ring->data[wp].v	= value;
	ring->data[wp].s	= valen;
	//ring->wp			= ring_element_next(ring, ring->wp);
	atomic_set(ring->wp, ring_element_next(ring, wp));
	ring->nr_times_wr ++;

	//RLOCK_UNLOCK(ring);
	
	return 0;
}


#endif

