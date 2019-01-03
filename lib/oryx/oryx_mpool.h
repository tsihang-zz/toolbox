/*!
 * @file oryx_mpool.h
 * @date 2017/08/29
 *
 * TSIHANG (haechime@gmail.com)
 */

#ifndef __ORYX_MPOOL_H__
#define __ORYX_MPOOL_H__

ORYX_DECLARE (
	void oryx_mpool_new (
		OUT void ** p,
		IN const char *mp_name,
		IN int nb_steps,
		IN int nb_bytes_per_element,
		IN int cache_line_size
	)
);

ORYX_DECLARE (
	void oryx_mpool_destroy (
		IN void * p
	)
);

ORYX_DECLARE (
	void oryx_mpool_free (
		IN void * p ,
		IN void *elem
	)
);

ORYX_DECLARE (
	void *oryx_mpool_alloc (
		IN void * p
	)
);


#endif

