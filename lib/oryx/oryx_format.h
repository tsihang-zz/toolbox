/*!
 * @file oryx_format.h
 * @date 2017/08/29
 *
 * TSIHANG (haechime@gmail.com)
 */

#ifndef __ORYX_FORMAT_H__
#define __ORYX_FORMAT_H__

#include "format.h"

ORYX_DECLARE (
	void oryx_format (
		IN struct oryx_fmt_buff_t *fb,
		IN const char *fmt,
		...
	)
);

ORYX_DECLARE (
	void oryx_format_reset (
		IN struct oryx_fmt_buff_t *fb
	)
);

ORYX_DECLARE (
	void oryx_format_free (
		IN struct oryx_fmt_buff_t *fb
	)
);


#endif

