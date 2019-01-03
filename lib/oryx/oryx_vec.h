/*!
 * @file oryx_vec.h
 * @date 2017/08/29
 *
 * TSIHANG (haechime@gmail.com)
 */

#ifndef __ORYX_VEC_H__
#define __ORYX_VEC_H__

#include "vec.h"

/* Prototypes. */
ORYX_DECLARE (
	oryx_vector vec_init (
		IN unsigned int size
	)
);

ORYX_DECLARE (
	oryx_vector vec_copy (
		IN oryx_vector v
	)
);

ORYX_DECLARE (
	int vec_empty_slot (
		IN oryx_vector v
	)
);

ORYX_DECLARE (
	int vec_set (
		IN oryx_vector v,
		IN void *val
	)
);

ORYX_DECLARE (
	int vec_set_index (
		IN oryx_vector v,
		IN unsigned int i,
		IN void *val
	)
);

ORYX_DECLARE (
	void vec_unset (
		IN oryx_vector v,
		IN unsigned int i
	)
);

ORYX_DECLARE (
	void vec_only_wrapper_free (
		IN oryx_vector v
	)
);

ORYX_DECLARE (
	void vec_only_index_free (
		IN void *index
	)
);

ORYX_DECLARE (
	void vec_free (
		IN oryx_vector v
	)
);

ORYX_DECLARE (
	void *vec_first (
		IN oryx_vector v
	)
);

ORYX_DECLARE (
	void *vec_last (
		IN oryx_vector v
	)
);

ORYX_DECLARE (
	void vec_ensure (
		IN oryx_vector v,
		IN unsigned int num
	)
);

ORYX_DECLARE (
	void *vec_lookup_ensure (
		IN oryx_vector v,
		IN unsigned int i
	)
);

#endif
