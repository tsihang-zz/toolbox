#ifndef FILE_H
#define FILE_H

//#define HAVE_F_CACHE

#define VLIB_CONF_NONE	(1 << 0)
#define VLIB_CONF_MV	(1 << 1)
#define VLIB_CONF_RM	(1 << 2)
#define VLIB_CONF_LOG	(1 << 3)
typedef struct vlib_conf_t {
	char		name[128];
	uint32_t	ul_flags;
	uint64_t	nr_size;
	uint64_t	nr_entries;
	uint64_t	nr_err_skip_entries;
	uint64_t	tv_usec;
} vlib_conf_t;

struct fq_element_t {
#define fq_name_length	128
	struct lq_prefix_t lp;
	char name[fq_name_length];
};

#define VLIB_FILE_NEW		(1 << 0)
#define VLIB_FILE_OPENED	(1 << 1)
#define VLIB_FILE_FLUSH		(1 << 2)
typedef struct vlib_file_t {
#define	name_length	256
	FILE		*fp;
	char		filepath[name_length];
	uint64_t	entries;
	time_t		local_time;
	uint32_t	ul_flags;
	struct list_head fnode;

#if defined(HAVE_F_CACHE)
#define file_cache_size	(8192 * 256)
	char		*cache;
	size_t		offset;
#endif

}vlib_file_t;

#if defined(HAVE_STAT)
struct stat { 
     dev_t			st_dev; // 文件所在设备ID 
     ino_t			st_ino; // 结点(inode)编号  
     mode_t			st_mode; // 保护模式 
     nlink_t		st_nlink; // 硬链接个数  
     uid_t			st_uid; // 所有者用户ID  
     gid_t st_gid; // 所有者组ID  
     dev_t st_rdev; // 设备ID(如果是特殊文件) 
     off_t ** st_size;** // 总体尺寸，以字节为单位 
     blksize_t st_blksize; // 文件系统 I/O 块大小
     blkcnt_t st_blocks; // 已分配 512B 块个数
     time_t st_atime; // 上次访问时间 
     time_t st_mtime; // 上次更新时间 
     time_t st_ctime; // 上次状态更改时间 
};
#endif

#if defined(HAVE_INOTIFY)
/* the following are legal, implemented events that user-space can watch for */
#define IN_ACCESS		0x00000001	/* File was accessed */
#define IN_MODIFY		0x00000002	/* File was modified */
#define IN_ATTRIB		0x00000004	/* Metadata changed */
#define IN_CLOSE_WRITE		0x00000008	/* Writtable file was closed */
#define IN_CLOSE_NOWRITE	0x00000010	/* Unwrittable file closed */
#define IN_OPEN			0x00000020	/* File was opened */
#define IN_MOVED_FROM	0x00000040	/* File was moved from X */
#define IN_MOVED_TO		0x00000080	/* File was moved to Y */
#define IN_CREATE		0x00000100	/* Subfile was created */
#define IN_DELETE		0x00000200	/* Subfile was deleted */
#define IN_DELETE_SELF	0x00000400	/* Self was deleted */

/* the following are legal events.  they are sent as needed to any watch */
#define IN_UNMOUNT		0x00002000	/* Backing fs was unmounted */
#define IN_Q_OVERFLOW	0x00004000	/* Event queued overflowed */
#define IN_IGNORED		0x00008000	/* File was ignored */

/* helper events */
#define IN_CLOSE		(IN_CLOSE_WRITE | IN_CLOSE_NOWRITE) /* close */
#define IN_MOVE			(IN_MOVED_FROM | IN_MOVED_TO) /* moves */

/* special flags */
#define IN_ISDIR		0x40000000	/* event occurred against dir */
#define IN_ONESHOT		0x80000000	/* only send event once */

/*
 * struct inotify_event - structure read from the inotify device for each event
 *
 * When you are watching a directory, you will receive the filename for events
 * such as IN_CREATE, IN_DELETE, IN_OPEN, IN_CLOSE, ..., relative to the wd.
 */
struct inotify_event {
	__s32		wd;		/* watch descriptor */
	__u32		mask;		/* watch mask */
	__u32		cookie;		/* cookie to synchronize two events */
	__u32		len;		/* length (including nulls) of name */
	char		name[0];	/* stub for possible name */
};
#endif

static __oryx_always_inline__
void file_reset(vlib_file_t *f)
{
	f->ul_flags 	= ~0;
	f->local_time	= ~0;
#if defined(HAVE_F_CACHE)
	f->offset		= ~0;
#endif
	f->fp			= NULL;
	memset ((void *)&f->filepath[0], 0, name_length);
}

static __oryx_always_inline__
int file_empty0(vlib_file_t *f)
{
	char ch;
	size_t nr_rb = 0;

	if (f->fp != NULL)
		nr_rb = fread(&ch, sizeof(char), 1, f->fp);

	return nr_rb == 0 ? 1 : 0;
}


static __oryx_always_inline__
int file_empty(vlib_file_t *f)
{
	FILE *fp;
	char ch;
	size_t nr_rb = 0;
	
	fp = fopen(f->filepath, "r");
	if (fp != NULL) {
		nr_rb = fread(&ch, sizeof(char), 1, fp);
		fclose(fp);
	}
	
	return nr_rb == 0 ? 1 : 0;
}

static __oryx_always_inline__
size_t file_write(vlib_file_t *f, const char *val, size_t valen)
{
	size_t nr_wb;

	nr_wb = fwrite(val, sizeof(char), valen, f->fp);
	return nr_wb;
}

static __oryx_always_inline__
int file_overtime(vlib_file_t *f, time_t now, int threshold)
{
	return (now > (f->local_time +  threshold * 60));
}

static __oryx_always_inline__
int file_overdisk(vlib_file_t *f, int max_entries)
{
	return ((int64_t)f->entries > (int64_t)max_entries);
}

static __oryx_always_inline__
struct fq_element_t *fqe_alloc(void)
{
	const size_t fqe_size = (sizeof(struct fq_element_t));
	struct fq_element_t *fqe;
	
	fqe = malloc(fqe_size);
	if(fqe) {
		memset (fqe, 0, (sizeof(struct fq_element_t)));
		fqe->lp.lnext = fqe->lp.lprev = NULL;
	}
	return fqe;
}

extern char *inotify_home;
extern char *store_home;

extern struct oryx_lq_ctx_t *fmgr_q;
extern void fmgr_move(const char *oldpath, const vlib_conf_t *vf);
extern void vlib_fmgr_init(vlib_main_t *vm);

#endif

