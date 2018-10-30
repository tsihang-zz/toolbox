#ifndef ORYX_VEC_H
#define ORYX_VEC_H

#include "vec.h"

/* Prototypes. */
ORYX_DECLARE(oryx_vector vec_init (unsigned int size));
ORYX_DECLARE(oryx_vector vec_copy (oryx_vector v));
ORYX_DECLARE(int vec_empty_slot (oryx_vector v));
ORYX_DECLARE(int vec_set (oryx_vector v, void *val));
ORYX_DECLARE(int vec_set_index (oryx_vector v, unsigned int i, void *val));
ORYX_DECLARE(void vec_unset (oryx_vector v, unsigned int i));
ORYX_DECLARE(void vec_only_wrapper_free (oryx_vector v));
ORYX_DECLARE(void vec_only_index_free (void *index));
ORYX_DECLARE(void vec_free (oryx_vector v));
ORYX_DECLARE(void *vec_first (oryx_vector v));
ORYX_DECLARE(void *vec_last (oryx_vector v));
ORYX_DECLARE(void vec_ensure (oryx_vector v, unsigned int num));
ORYX_DECLARE(void *vec_lookup_ensure (oryx_vector v, unsigned int i));


#endif
