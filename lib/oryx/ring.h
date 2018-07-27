#ifndef RING_H
#define RING_H

#define RLOCK_INIT(r)\
	if(!((r)->ul_flags & RING_SHARED))\
		do_mutex_init(&(r)->m)

#define RLOCK_DESTROY(r)\
	if(!((r)->ul_flags & RING_SHARED))\
		do_mutex_destroy(&(r)->m)

#define RLOCK_TRYLOCK(r)\
		do_mutex_trylock(&(r)->m)

#define RLOCK_LOCK(r)\
	if(!((r)->ul_flags & RING_SHARED))\
		do_mutex_lock(&(r)->m)

#define RLOCK_UNLOCK(r)\
	if(!((r)->ul_flags & RING_SHARED))\
		do_mutex_unlock(&(r)->m)

#define	ring_element_next(ring,rw)	(((rw) + 1) % (ring)->max_elements)

#define ring_key_data(key,factor)\
	((key) << 16 | factor)

struct oryx_ring_data_t {
	uint32_t	pad0: 16;
	uint32_t	s: 16;
	void	*v;
};

/* */
#define	RING_SHARED				(1 << 0)	/* A shared ring is a ring that
											 * used to communication among progresses.
											 */
struct oryx_ring_t {
	const char			*ring_name;
	int					max_elements;
	key_t					key;			/* for different progress. */

	volatile uint32_t			wp;
	volatile uint32_t			rp;
	struct oryx_ring_data_t		*data;

	uint32_t			ul_flags;
	uint32_t			ul_rp_times;
	uint32_t			ul_wp_times;
	os_mutex_t			m;
};

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
	ring->ul_rp_times ++;

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
		return -1;
	}

	ring->data[wp].v	= data;
	ring->data[wp].s	= data_size;
	ring->wp			= ring_element_next(ring, ring->wp);
	ring->ul_wp_times ++;

	RLOCK_UNLOCK(ring);
	
	return 0;
}


extern int oryx_ring_create(const char *ring_name, int max_elements, uint32_t flags, struct oryx_ring_t **ring);
extern void oryx_ring_dump(struct oryx_ring_t *ring);

#endif

