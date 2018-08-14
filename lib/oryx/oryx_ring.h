#ifndef ORYX_RING_H
#define ORYX_RING_H

#include "ring.h"

ORYX_DECLARE(int oryx_ring_create(const char *ring_name,
				int max_elements, uint32_t flags, struct oryx_ring_t **ring));
ORYX_DECLARE(void oryx_ring_dump(struct oryx_ring_t *ring));

#endif
