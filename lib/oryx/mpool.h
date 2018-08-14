#ifndef MPOOL_H
#define MPOOL_H


ORYX_DECLARE(void mpool_init(void ** p, const char *mp_name,
					int nb_steps, int nb_bytes_per_element, int cache_line_size));
ORYX_DECLARE(void mpool_uninit(void * p));
ORYX_DECLARE(void mpool_free (void * p , void *elem));
ORYX_DECLARE(void * mpool_alloc(void * p));

#endif

