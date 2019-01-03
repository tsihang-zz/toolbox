/*!
 * @file format.c
 * @date 2017/08/29
 *
 * TSIHANG (haechime@gmail.com)
 */

#include "oryx.h"

/*
 * Generates a log message The message will be sent in the stream
 * defined by the previous call to rte_openlog_stream().
 */
static int ___format
(
	IN struct oryx_fmt_buff_t *fb,
	IN const char *format,
	IN va_list ap
)
{
	int ret;
	ret = vsnprintf(fb->fmt_data + fb->fmt_doffs, 
			fb->fmt_buff_size - fb->fmt_doffs, 
			format, ap);
	return ret;
}

__oryx_always_extern__
void oryx_format
(
	IN struct oryx_fmt_buff_t *fb,
	IN const char *fmt,
	...
)
{
	va_list ap;

	if (!fb->fmt_data) {
		fb->fmt_data = malloc (DEFAULT_FMT_MSG_SIZE);
		if(unlikely (!fb->fmt_data))
			oryx_panic(-1,
				"malloc: %s", oryx_safe_strerror(errno));
		fb->fmt_doffs = 0;
		fb->fmt_buff_size = DEFAULT_FMT_MSG_SIZE;
		memset (fb->fmt_data, 0, fb->fmt_buff_size);
	}

	/** fmt buffer expand when needed. */
	if ((float)fb->fmt_doffs > (fb->fmt_buff_size * 0.8)) {
		fb->fmt_data = realloc(fb->fmt_data, fb->fmt_doffs + DEFAULT_FMT_MSG_SIZE);
		if(unlikely (!fb->fmt_data))
			oryx_panic(-1, 
				"realloc: %s", oryx_safe_strerror(errno));
			
		/* new size. */
		fb->fmt_buff_size = fb->fmt_doffs + DEFAULT_FMT_MSG_SIZE;
	}
	
	va_start(ap, fmt);
	fb->fmt_doffs += ___format(fb, fmt, ap);
	va_end(ap);

}

__oryx_always_extern__
void oryx_format_free
(
	IN struct oryx_fmt_buff_t *fb
)
{
	if(fb->fmt_data)
		free(fb->fmt_data);

	fb->fmt_buff_size = fb->fmt_doffs = 0;
}

__oryx_always_extern__
void oryx_format_reset
(
	IN struct oryx_fmt_buff_t *fb
)
{
	if(fb->fmt_data) {
		memset (fb->fmt_data, 0, fb->fmt_doffs);
		fb->fmt_doffs = FMT_START;
	}
}

