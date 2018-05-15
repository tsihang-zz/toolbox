
#include "oryx.h"

/* Initialize oryx_vector : allocate memory and return oryx_vector. */
oryx_vector
vec_init (unsigned int size)
{
  oryx_vector v = kmalloc (sizeof (struct _oryx_vector), MPF_CLR, __oryx_unused_val__);

  /* allocate at least one slot */
  if (size == 0)
    size = 1;

  v->alloced = size;
  v->active = 0;
  v->index = kmalloc (sizeof (void *) * size, MPF_CLR, __oryx_unused_val__);
  return v;
}

void
vec_only_wrapper_free (oryx_vector v)
{
  kfree (v);
}

void
vec_only_index_free (void *index)
{
  kfree (index);
}

void
vec_free (oryx_vector v)
{
  kfree (v->index);
  kfree (v);
}

oryx_vector
vec_copy (oryx_vector v)
{
  unsigned int size;
  oryx_vector new = kmalloc (sizeof (struct _oryx_vector), MPF_CLR, __oryx_unused_val__);

  new->active = v->active;
  new->alloced = v->alloced;

  size = sizeof (void *) * (v->alloced);
  new->index = kmalloc (size, MPF_CLR, __oryx_unused_val__);
  memcpy (new->index, v->index, size);

  return new;
}

/* Check assigned index, and if it runs short double index pointer */
void
vec_ensure (oryx_vector v, unsigned int num)
{
  if (v->alloced > num)
    return;

  v->index = krealloc (v->index, sizeof (void *) * (v->alloced * 2),
  					MPF_NOFLGS, __oryx_unused_val__);
  memset (&v->index[v->alloced], 0, sizeof (void *) * v->alloced);
  v->alloced *= 2;
  
  if (v->alloced <= num)
    vec_ensure (v, num);
}

/* This function only returns next empty slot index.  It dose not mean
   the slot's index memory is assigned, please call vec_ensure()
   after calling this function. */
int
vec_empty_slot (oryx_vector v)
{
  unsigned int i;

  if (v->active == 0)
    return 0;

  for (i = 0; i < v->active; i++)
    if (v->index[i] == 0)
      return i;

  return i;
}

/* Set value to the smallest empty slot. */
int
vec_set (oryx_vector v, void *val)
{
  unsigned int i;

  i = vec_empty_slot (v);
  vec_ensure (v, i);

  v->index[i] = val;

  if (v->active <= i)
    v->active = i + 1;

  return i;
}

/* Set value to specified index slot. */
int
vec_set_index (oryx_vector v, unsigned int i, void *val)
{
  vec_ensure (v, i);

  v->index[i] = val;

  if (v->active <= i)
    v->active = i + 1;

  return i;
}

/* Look up oryx_vector.  */
void *
vec_lookup (oryx_vector v, unsigned int i)
{
  if (i >= v->active)
    return NULL;
  return v->index[i];
}

/* Lookup oryx_vector, ensure it. */
void *
vec_lookup_ensure (oryx_vector v, unsigned int i)
{
  vec_ensure (v, i);
  return v->index[i];
}

/* Unset value at specified index slot. */
void
vec_unset (oryx_vector v, unsigned int i)
{
  if (i >= v->alloced)
    return;

  v->index[i] = NULL;

  if (i + 1 == v->active) 
    {
      v->active--;
      while (i && v->index[--i] == NULL && v->active--) 
	;				/* Is this ugly ? */
    }
}

/* Count the number of not emplty slot. */
unsigned int
vec_count (oryx_vector v)
{
  unsigned int i;
  unsigned count = 0;

  for (i = 0; i < v->active; i++) 
    if (v->index[i] != NULL)
      count++;

  return count;
}

void *vec_last (oryx_vector v)
{
    unsigned int i;
    void * last = NULL;
    
    for (i = 0; i < v->active; i++)
        if (v->index[i] != NULL)
            last = v->index[i];
            
    return last;
}

void *vec_first (oryx_vector v)
{
    unsigned int i;
    void * first = NULL;
    
    for (i = 0; i < v->active; i++)
        if (v->index[i] != NULL) {
	     first = v->index[i];
            break;
        }
            
    return first;
}

