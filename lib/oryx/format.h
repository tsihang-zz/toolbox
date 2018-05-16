#ifndef FORMAT_H
#define FORMAT_H

#define DEFAULT_FMT_MSG_SIZE	1024
struct oryx_fmt_buff_t {
	char *fmt_data;
	size_t fmt_data_size;
	size_t fmt_buff_size;
};


#endif

