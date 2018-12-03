#ifndef FIO_H
#define FIO_H

#define ORYX_FILE_NEW		(1 << 0)
#define ORYX_FILE_OPENED	(1 << 1)
#define ORYX_FILE_FLUSH		(1 << 2)
struct oryx_file_t {
#define	name_length	256
	FILE		*fp;
	char		filepath[name_length];
	time_t		local_time;
	uint32_t	ul_flags;
	struct list_head fnode;

#define oryx_cache_size	(8192 * 256)
	char		*cache;	/* storage */
	size_t		offset; /* data length in this cache */
	size_t		cachel;	/* cache size */
};

#define ORYX_FILE_INIT(f)\
	(f)->cache = NULL;


ORYX_DECLARE (
	int oryx_fopen (
		IN const char *fpath,
		IN const char *fmode,
		IN const size_t cachesiz,
		OUT struct oryx_file_t *f
	)
);

ORYX_DECLARE (
	size_t oryx_fflush (
		IN struct oryx_file_t *f
	)
);


ORYX_DECLARE (
	size_t oryx_fwrite (
		IN struct oryx_file_t *f,
		IN char *value,
		IN size_t valen
	)
);

ORYX_DECLARE (
	void oryx_fclose (
		IN struct oryx_file_t *f
	)

);

ORYX_DECLARE (
	void oryx_fdestroy (
		IN struct oryx_file_t *f
	)

);

#endif

