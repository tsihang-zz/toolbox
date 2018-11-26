#ifndef ORYX_VEC_H
#define ORYX_VEC_H

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
