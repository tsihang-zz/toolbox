/*!
 * @file mpool.h
 * @date 2017/08/29
 *
 * TSIHANG (haechime@gmail.com)
 */

#ifndef __MPOOL_H__
#define __MPOOL_H__

/* Allow the use in C++ code.  */
#ifdef __cplusplus
extern "C" {
#endif

/* Nothing to do. */

ORYX_DECLARE (
	void oryx_mpool_new	(
		OUT void ** p,
		IN const char *name,
		IN int nr_steps,
		IN int nr_bytes_per_element,
		IN int cache_line_size
	)
);

ORYX_DECLARE (
	void oryx_mpool_destroy (
		IN void * p
	)
);

ORYX_DECLARE (
	void *oryx_mpool_alloc (
		IN void * p
	)
);

ORYX_DECLARE (
	void oryx_mpool_dump (
		IN void * p
	)
);

ORYX_DECLARE (
	void oryx_mpool_free (
		IN void * p ,
		IN void *elem
	)
);

#ifdef __cplusplus
}
#endif /* C++ */

#endif

