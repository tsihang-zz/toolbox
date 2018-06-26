#ifndef RING_H
#define RING_H

struct oryx_ring_data_t {
	uint32_t	pad0: 16;
	uint32_t	s: 16;
	void	*v;
};

#define RING_CRITICAL			(1 << 0)
struct oryx_ring_t {
	const char			*ring_name;
	int					nb_data;
	volatile uint32_t			wp;
	volatile uint32_t			rp;
	struct oryx_ring_data_t		*data;

	uint32_t			ul_flags;
	uint32_t			ul_rp_times;
	uint32_t			ul_wp_times;
};

extern int oryx_ring_create(const char *ring_name, int nb_data, uint32_t flags, struct oryx_ring_t **ring);
extern int oryx_ring_get(struct oryx_ring_t *ring, void **data, uint16_t *data_size);
extern int oryx_ring_put(struct oryx_ring_t *ring, void *data, uint16_t data_size);
extern void oryx_ring_dump(struct oryx_ring_t *ring);

extern void oryx_ring_test(void);

#endif


