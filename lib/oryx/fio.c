#include "oryx.h"

static __oryx_always_inline__
int file_empty0(struct oryx_file_t *f)
{
	char ch;
	size_t nr_rb = 0;

	if (f->fp != NULL)
		nr_rb = fread(&ch, sizeof(char), 1, f->fp);

	return nr_rb == 0 ? 1 : 0;
}

__oryx_always_extern__
int oryx_fopen
(
	IN const char *fpath,
	IN const char *fmode,
	IN const size_t cachesiz,
	OUT struct oryx_file_t *f
)
{
	size_t size = 0;
	const char *mode = "a+";

	BUG_ON (fpath == NULL);
	BUG_ON (f == NULL);

	if (fmode)
		mode = fmode;

	if (!cachesiz)
		size = oryx_cache_size;

	if (!oryx_path_exsit(fpath))
		f->ul_flags |= ORYX_FILE_NEW;

	f->fp = fopen(fpath, mode);
	if (!f->fp) {
		fprintf (stdout, "fopen: %s\n", oryx_safe_strerror(errno));
		return -1;
	}

	if (!f->cache) {
		f->cache = malloc(size);
		if (!f->cache) {
			fprintf (stdout, "malloc: %s\n", oryx_safe_strerror(errno));
			return -1;
		}
		memset (f->cache, 0, size);
		f->offset = 0;
		f->cachel = size;
	}

	sprintf(f->filepath, "%s", fpath);
	f->ul_flags |= ORYX_FILE_OPENED;
	
	return 0;
}

__oryx_always_extern__
size_t oryx_fflush
(
	IN struct oryx_file_t *f
)
{
	size_t nr_wb = 0;

	if(f->offset) {
		nr_wb = fwrite(f->cache, sizeof(char), f->offset, f->fp);
		fflush(f->fp);
		f->offset = 0;
	}
	
	return nr_wb;	
}

__oryx_always_extern__
size_t oryx_fwrite
(
	IN struct oryx_file_t *f,
	IN char *value,
	IN size_t valen
)
{
	size_t nr_wb;

	if ((f->offset + valen) > f->cachel)
		nr_wb = oryx_fflush(f);
	else {
		memcpy(f->cache + f->offset, value, valen);
		f->offset += valen;
		nr_wb = valen;
	}
	
	return nr_wb;	
}

__oryx_always_extern__
void oryx_fclose
(
	IN struct oryx_file_t *f
)
{
	BUG_ON (f == NULL);

	if (!(f->ul_flags & ORYX_FILE_OPENED))
		return;

	fprintf(stdout, "f->offset %lu\n", f->offset);
	oryx_fflush(f);
	fclose(f->fp);
	f->ul_flags &= ~ORYX_FILE_OPENED;
}

__oryx_always_extern__
void oryx_fdestroy
(
	IN struct oryx_file_t *f
)
{
	oryx_fclose(f);
	free(f->cache);
}


