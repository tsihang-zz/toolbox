/*
 * Generic oryx_vector interface header.
 * Copyright (C) 1997, 98 Kunihiro Ishiguro
 *
 * This file is part of GNU Zebra.
 *
 * GNU Zebra is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * GNU Zebra is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Zebra; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.  
 */

#ifndef _ORYX_VECTOR_H
#define _ORYX_VECTOR_H

#include <stddef.h>

#if 1
/* struct for oryx_vector */
struct _oryx_vector 
{
  unsigned int active;		/* number of active slots */
  unsigned int alloced;		/* number of allocated slot */
  void **index;			/* index to data */
};
typedef struct _oryx_vector *oryx_vector;

#define VEC_MIN_SIZE 1

/* (Sometimes) usefull macros.  This macro convert index expression to
 array expression. */
/* Reference slot at given index, caller must ensure slot is active */
#define vec_slot(V,I)  ((V)->index[(I)])
/* Number of active slots. 
 * Note that this differs from vector_count() as it the count returned
 * will include any empty slots
 */
#define vec_active(V) ((V)->active)

/* Prototypes. */
extern oryx_vector vec_init (unsigned int size);
extern void vec_ensure (oryx_vector v, unsigned int num);
extern int vec_empty_slot (oryx_vector v);
extern int vec_set (oryx_vector v, void *val);
extern int vec_set_index (oryx_vector v, unsigned int i, void *val);
extern void vec_unset (oryx_vector v, unsigned int i);
extern unsigned int vec_count (oryx_vector v);
extern void vec_only_wrapper_free (oryx_vector v);
extern void vec_only_index_free (void *index);
extern void vec_free (oryx_vector v);
extern oryx_vector vec_copy (oryx_vector v);

extern void *vec_lookup (oryx_vector, unsigned int);
extern void *vec_lookup_ensure (oryx_vector, unsigned int);

void *vec_first (oryx_vector v);
void *vec_last (oryx_vector v);

#define vec_foreach_element(oryx_vector, foreach_element, element)\
	for (foreach_element = 0, element = NULL;\
		foreach_element <= (int)vec_active(oryx_vector); \
		element = (typeof(*element) *)vec_slot(oryx_vector, foreach_element), foreach_element++)
		
#endif
#endif /* _ZEBRA_VECTOR_H */
