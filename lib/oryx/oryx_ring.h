/*!
 * @file oryx_ring.h
 * @date 2017/08/29
 *
 * TSIHANG (haechime@gmail.com)
 */

#ifndef __ORYX_RING_H__
#define __ORYX_RING_H__

#include "ring.h"
#include "shm_ring.h"

ORYX_DECLARE (
	int oryx_ring_create (
		IN const char *name,
		IN int nr_elements,
		IN size_t __oryx_unused__ nr_bytes_per_element,
		IN uint32_t flags,
		OUT struct oryx_ring_t **ring
	)
);

ORYX_DECLARE (
	void oryx_ring_dump (
		IN struct oryx_ring_t *ring
	)
);

ORYX_DECLARE (
	int oryx_shmring_create (
		IN const char *shmring_name,
		IN int __oryx_unused__ nr_elements,
		IN int __oryx_unused__ nr_element_size,
		IN uint32_t flags,
		OUT struct oryx_shmring_t **shmring
	)
);

ORYX_DECLARE (
	void oryx_shmring_dump (
		IN struct oryx_shmring_t *shmring
	)
);

ORYX_DECLARE (
	int oryx_shmring_destroy (
		IN struct oryx_shmring_t *shmring
	)
);

#endif
