#ifndef ORYX_FORMAT_H
#define ORYX_FORMAT_H

#include "format.h"

ORYX_DECLARE(void oryx_format(struct oryx_fmt_buff_t *fb, const char *fmt, ...));
ORYX_DECLARE(void oryx_format_reset(struct oryx_fmt_buff_t *fb));
ORYX_DECLARE(void oryx_format_free(struct oryx_fmt_buff_t *fb));


#endif

