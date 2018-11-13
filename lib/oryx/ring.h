#ifndef RING_H
#define RING_H

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


#define DEFAULT_RING_ELEMENTS	(1 << 1024)

struct oryx_ring_data_t {
	uint32_t	pad0: 16;
	uint32_t	s: 16;
	void	*v;
};

struct oryx_ring_t {
	const char			*name;
	int					max_elements;
	key_t					key;			/* for different progress. */

	volatile uint64_t			wp,
								rp;
	struct oryx_ring_data_t		*data;

	uint32_t			ul_flags;
	uint64_t			nr_times_r,
						nr_times_w,
						nr_times_f;	/* full times */

	os_mutex_t			m;
};
#define	ring_element_next(r,rw)	(((rw) + 1) % (r)->max_elements)

static __oryx_always_inline__
int oryx_ring_get(struct oryx_ring_t *ring, void **data, uint16_t *data_size)
{
	uint32_t	rp = 0;
	
#if defined(BUILD_DEBUG)
	BUG_ON(ring == NULL);
#endif

	(*data)			= NULL;
	(*data_size)	= 0;

	RLOCK_LOCK(ring);

	rp = ring->rp;
	if(rp == ring->wp) {
		RLOCK_UNLOCK(ring);
		return -1;
	}

	(*data)			= ring->data[rp].v;
	(*data_size)	= ring->data[rp].s;
	ring->rp		= ring_element_next(ring, ring->rp);
	ring->nr_times_r ++;

	RLOCK_UNLOCK(ring);

	return 0;
}

static __oryx_always_inline__
int oryx_ring_put(struct oryx_ring_t *ring, void *data, uint16_t data_size)
{
	uint32_t	wp = 0;
	
#if defined(BUILD_DEBUG)
	BUG_ON(ring == NULL);
#endif

	RLOCK_LOCK(ring);

	wp = ring->wp;
	if(ring_element_next(ring, wp) == ring->rp) {
		RLOCK_UNLOCK(ring); 
		ring->nr_times_f ++;
		return -1;
	}

	ring->data[wp].v	= data;
	ring->data[wp].s	= data_size;
	ring->wp			= ring_element_next(ring, ring->wp);
	ring->nr_times_w ++;

	RLOCK_UNLOCK(ring);
	
	return 0;
}


#endif

