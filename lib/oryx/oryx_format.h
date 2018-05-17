#ifndef ORYX_FORMAT_H
#define ORYX_FORMAT_H

#include "format.h"

void oryx_format(struct oryx_fmt_buff_t *fb, const char *fmt, ...);
void oryx_format_reset(struct oryx_fmt_buff_t *fb);
void oryx_format_free(struct oryx_fmt_buff_t *fb);


#endif

