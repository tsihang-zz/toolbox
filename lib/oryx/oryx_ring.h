#ifndef ORYX_RING_H
#define ORYX_RING_H

#include "ring.h"
#include "shm_ring.h"

ORYX_DECLARE(int oryx_ring_create(const char *name,
				int max_elements, uint32_t flags, struct oryx_ring_t **ring));
ORYX_DECLARE(void oryx_ring_dump(struct oryx_ring_t *ring));

ORYX_DECLARE(int oryx_shmring_create(const char *shmring_name,
		int __oryx_unused_param__ nr_elements,
		int __oryx_unused_param__ nr_element_size,
		uint32_t flags, 
		struct oryx_shmring_t **shmring));
ORYX_DECLARE(void oryx_shmring_dump(struct oryx_shmring_t *shmring));
ORYX_DECLARE(int oryx_shmring_destroy(struct oryx_shmring_t *shmring));




#endif
