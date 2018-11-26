#ifndef ORYX_FORMAT_H
#define ORYX_FORMAT_H

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

