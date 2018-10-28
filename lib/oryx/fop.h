#ifndef FILE_IO_H
#define FILE_IO_H

extern uint32_t __os_rand;

#define fmt_speed(__us, __refcnt, __f)\
	(uint64_t)((__us) ? (__refcnt) * 1000000 * (__f)/(__us) : 0)

#define fmt_bps(us, refcnt) fmt_speed(us, refcnt, 8)
#define fmt_pps(us, refcnt) fmt_speed(us, refcnt, 1)

enum {
	RW_MODE_WRITE,
	RW_MODE_READ_LINE = 1 << 0,	/** Read one line. */
	RW_MODE_READ	= 1 << 1,
};

struct oryx_file_rw_context_t {
	char *sc_buffer;
	oryx_size_t ul_size;	/** buffer size */
	u8 cmd;
	oryx_size_t ul_rw_size;	/** Real read or write data size. */
};

ORYX_DECLARE(
	int oryx_path_remove (const char *path));
ORYX_DECLARE(
	int oryx_path_exsit (const char *path));
ORYX_DECLARE(
	int oryx_mkfile (const char *file, oryx_file_t **fp, const char *mode));
ORYX_DECLARE(
	int oryx_mkdir(const char *path, oryx_dir_t **dir));
ORYX_DECLARE(
	int oryx_file_close (oryx_file_t *fp));
ORYX_DECLARE(
	int oryx_file_read_write (oryx_file_t *fp, 
			struct oryx_file_rw_context_t *frw_ctx));
ORYX_DECLARE(
	char * oryx_fmt_program_counter (uint64_t , char *, int , int));
ORYX_DECLARE(
	void oryx_register_sighandler(int signal, void (*handler)(int)));
ORYX_DECLARE(
	int oryx_pattern_generate (char *pattern, size_t l));
ORYX_DECLARE(
	void oryx_l4_port_generate (char *port_src, char *port_dst));
ORYX_DECLARE(
	void oryx_ipaddr_generate (char *ipv4));
ORYX_DECLARE(
	int isalldigit(const char *str));
ORYX_DECLARE(
	const char *draw_color(color_t color));
ORYX_DECLARE(
	int kvpair(char *str, char **k, char **v));
ORYX_DECLARE(
	int is_numerical (char* s));
ORYX_DECLARE(
	int do_system(const char *cmd));
ORYX_DECLARE(
	uint64_t tm_elapsed_us (struct  timeval *start, struct  timeval *end));
ORYX_DECLARE(
	void fmt_time(uint64_t ts, const char *fmt, char *date, size_t len));
ORYX_DECLARE(
	void memcpy_tolower (u8 *d, u8 *s, u16 len));
ORYX_DECLARE(
	int path_is_absolute(const char *path));
ORYX_DECLARE(
	int path_is_relative(const char *path));
ORYX_DECLARE(
	int foreach_directory_file (char *dir_name,
			int (*f) (void *arg, char * path_name,
			char * file_name), void *arg,
			int scan_dirs));
ORYX_DECLARE(
	void oryx_file_clear(const char *f));

#ifndef HAVE_STRLCPY
ORYX_DECLARE(
	size_t strlcpy(char *d, const char *s, size_t VLIB_BUFSIZE));
#endif

#ifndef HAVE_STRLCAT
ORYX_DECLARE(
	size_t strlcat(char *d, const char *s, size_t VLIB_BUFSIZE));
#endif

#endif

