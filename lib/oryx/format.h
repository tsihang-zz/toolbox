#ifndef FORMAT_H
#define FORMAT_H

#define FMT_START				0
#define DEFAULT_FMT_MSG_SIZE	1024
struct oryx_fmt_buff_t {

	/** data hold by this variable. */
	char *fmt_data;

	/** data offset from start of fmt_data. */
	size_t fmt_doffs;

	/** buffer length of whole fmt_data. */
	size_t fmt_buff_size;
};

#define FMT_INITIALIZATION_VALUE	{NULL, 0, 0}
#define FMT_DATA(fmt)	((struct oryx_fmt_buff_t *)(&fmt))->fmt_data

#endif

