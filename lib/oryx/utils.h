/*!
 * @file utils.h
 * @date 2017/08/29
 *
 * TSIHANG (haechime@gmail.com)
 */

#ifndef FILE_IO_H
#define FILE_IO_H

#define fmt_speed(__us, __refcnt, __f)\
	(uint64_t)((__us) ? (__refcnt) * 1000000 * (__f)/(__us) : 0)

#define fmt_bps(us, refcnt) fmt_speed(us, refcnt, 8)
#define fmt_pps(us, refcnt) fmt_speed(us, refcnt, 1)


ORYX_DECLARE (
	int oryx_path_remove (
		IN const char *path
	)
);

ORYX_DECLARE (
	int oryx_path_exsit (
		IN const char *path
	)
);

ORYX_DECLARE (
	int oryx_mkfile (
		IN const char *file,
		IN const char *mode,
		OUT FILE **fp
	)
);

ORYX_DECLARE (
	int oryx_mkdir(
		IN const char *dir,
		OUT DIR **d
	)
);

ORYX_DECLARE (
	char * oryx_fmt_program_counter (
		IN uint64_t , 
		IN char *,
		IN int ,
		IN int
	)
);

ORYX_DECLARE (
	void oryx_register_sighandler (
		IN int signal,
		IN void (*handler)(IN int)
	)
);

ORYX_DECLARE (
	int oryx_pattern_generate (
		IN char *pattern,
		IN size_t l
	)
);

ORYX_DECLARE (
	void oryx_l4_port_generate (
		IN char *sp,
		IN char *dp
	)
);

ORYX_DECLARE (
	void oryx_ipaddr_generate (
		IN char *ipv4
	)
);

ORYX_DECLARE (
	const char *draw_color (
		IN color_t color
	)
);

ORYX_DECLARE (
	int kvpair (
		IN char *str,
		OUT char **k,
		OUT char **v
	)
);

ORYX_DECLARE (
	bool is_numerical (
		IN const char* s,
		IN size_t l
	)
);

ORYX_DECLARE (
	bool is_number (
		IN const char* s,
		IN size_t l
	)
);

ORYX_DECLARE (
	int do_system (
		IN const char *cmd
	)
);

ORYX_DECLARE (
	uint64_t oryx_elapsed_us (
		IN struct  timeval *start,
		IN struct  timeval *end
	)
);

ORYX_DECLARE (
	void oryx_fmt_time (
		IN uint64_t ts, 
		IN const char *fmt,
		OUT char *date,
		IN size_t len
	)
);

ORYX_DECLARE (
	int oryx_get_prgname (
		IN pid_t pid,
		OUT char *prgname
	)
);

ORYX_DECLARE (
	void memcpy_tolower (
		OUT uint8_t *d,
		IN uint8_t *s,
		IN uint16_t len
	)
);

ORYX_DECLARE (
	int oryx_foreach_directory_file (
		IN char *dir_name,
		IN int (*f) (IN void *arg, IN char * path_name,
			IN char * file_name), 
		IN void *arg,
		IN int scan_dirs
	)
);

ORYX_DECLARE (
	void oryx_file_clear (
		const char *f
	)
);

ORYX_DECLARE (
	uint32_t oryx_next_rand (
		uint32_t *p
	)
);

ORYX_DECLARE (
	int oryx_formatted_range (
		IN const char *str,
		IN uint32_t lim,
		IN int base,
		IN char dlm,
		OUT uint32_t *ul_start,
		OUT uint32_t *ul_end
	)
);


#ifndef HAVE_STRLCPY
ORYX_DECLARE(
	size_t strlcpy(char *d, const char *s, size_t l));
#endif

#ifndef HAVE_STRLCAT
ORYX_DECLARE(
	size_t strlcat(char *d, const char *s, size_t l));
#endif

#endif

