/*!
 * @file vec.h
 * @date 2017/08/29
 *
 * TSIHANG (haechime@gmail.com)
 */

#ifndef VEC_H
#define VEC_H

#include <stddef.h>

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
 * Note that this differs from vec_count() as it the count returned
 * will include any empty slots
 */
#define vec_active(V) ((V)->active)

#if defined(BUILD_DEBUG)
/* Look up oryx_vector.  */
static __oryx_always_inline__
void *vec_lookup
(
	IN oryx_vector v,
	IN unsigned int i
)
{
  if (i >= v->active)
    return NULL;
  return v->index[i];
}
#else
#define vec_lookup(v,i)\
	((v)->index[(i)])

#endif

/* Count the number of not emplty slot. */
static __oryx_always_inline__
unsigned int vec_count 
(
	IN oryx_vector v
)
{
  unsigned int i;
  unsigned count = 0;

  for (i = 0; i < v->active; i++) 
    if (v->index[i] != NULL)
      count++;

  return count;
}


#define vec_foreach_element(oryx_vector, foreach_element, element)\
	for (foreach_element = 0, element = NULL;\
		foreach_element <= (int)vec_active(oryx_vector); \
		element = (typeof(*element) *)vec_slot(oryx_vector, foreach_element), foreach_element++)

#endif /* _ZEBRA_VECTOR_H */
