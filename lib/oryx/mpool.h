#ifndef MPOOL_H
#define MPOOL_H

ORYX_DECLARE (
	void mpool_init (
		OUT void ** p,
		IN const char *mp_name,
		IN int nb_steps,
		IN int nb_bytes_per_element,
		IN int cache_line_size
	)
);

ORYX_DECLARE (
	void mpool_uninit (
		IN void * p
	)
);

ORYX_DECLARE (
	void mpool_free (
		IN void * p ,
		IN void *elem
	)
);

ORYX_DECLARE (
	void *mpool_alloc (
		IN void * p
	)
);

#endif

