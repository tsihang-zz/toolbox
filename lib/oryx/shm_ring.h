#ifndef SHMRING_H
#define SHMRING_H

#define NR_SHMRING_ELEMENTS		1024
#define NR_SHMRING_ELEMENT_SIZE	256

typedef struct vlib_shmring_data_t {
	uint32_t	pad0	: 16;
	uint32_t	valen	: 16;
	char		value[NR_SHMRING_ELEMENT_SIZE];
}vlib_shmring_data_t;

struct oryx_shmring_t {
	char			name[32];
	uint32_t			max_elements,
						ul_flags;
	key_t				key;			/* for different progress. */
	key_t				key0;

	volatile uint64_t	wp,
						rp;
	
	uint64_t			nr_times_r,
						nr_times_w,
						nr_times_f;	/* full times */

	vlib_shmring_data_t buffer[NR_SHMRING_ELEMENTS];

};
#define shmring_key_data(key,factor)\
	((key) << 16 | factor)
#define	shmring_element_next(r,rw)	(((rw) + 1) % (r)->max_elements)

static __oryx_always_inline__
int oryx_shmring_get(struct oryx_shmring_t *shmring, void **value, size_t *valen)
{
	uint32_t	rp,
				wp;
	vlib_shmring_data_t *b;
	
#if defined(BUILD_DEBUG)
	BUG_ON(shmring == NULL);
#endif

	(*value)	= NULL;
	(*valen)	= 0;

	//RLOCK_LOCK(shmring);

	rp = shmring->rp;
	wp = shmring->wp;
	
	if(rp == wp) {
		//RLOCK_UNLOCK(shmring);
		return -1;	/* empty ring */
	}

	b = (vlib_shmring_data_t *)&shmring->buffer[rp];
	(*value) = &b->value[0];
	(*valen) = b->valen;
	
	shmring->rp	= shmring_element_next(shmring, shmring->rp);
	shmring->nr_times_r ++;

	//RLOCK_UNLOCK(shmring);

	return 0;
}

static __oryx_always_inline__
int oryx_shmring_put(struct oryx_shmring_t *shmring, void *value, size_t valen)
{
	uint32_t	rp,
				wp;
	vlib_shmring_data_t *b;

#if defined(BUILD_DEBUG)
	BUG_ON(shmring == NULL);
#endif

	//RLOCK_LOCK(shmring);

	rp = shmring->rp;
	wp = shmring->wp;
	
	if(shmring_element_next(shmring, wp) == rp) {
		//RLOCK_UNLOCK(shmring); 
		shmring->nr_times_f ++;	/* no space to put. */
		return -1;
	}

	b = (vlib_shmring_data_t *)&shmring->buffer[wp];

	b->valen = valen;
	if(b->valen > NR_SHMRING_ELEMENT_SIZE)
		b->valen = NR_SHMRING_ELEMENT_SIZE;
	
	memcpy(b->value, value, b->valen);
	
	shmring->wp	= shmring_element_next(shmring, wp);
	shmring->nr_times_w ++;

	//RLOCK_UNLOCK(shmring);
	
	return 0;
}

#endif

