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
time_t regx_date2stamp(char *date)
{
	char *p, i;
	struct tm	tm = {
			.tm_sec	=	0,
			.tm_min	=	0,
			.tm_hour=	0,
			.tm_mday=	0,
			.tm_mon	=	0,
			.tm_year=	0,
			.tm_wday=	0,
			.tm_yday=	0,
			.tm_isdst=	0,
	};
	char *chkptr;
	char d[8] = {0};
	
	for (p = (char *)&date[0], i = 0;
				*p != '\0' && *p != '\t'; ++ p) {
		/* year */
		if (tm.tm_year == 0) {
			d[i ++] = *p;
			if (i == 4) {
				tm.tm_year = strtol(d, &chkptr, 10);
				if ((chkptr != NULL && *chkptr == 0)) {
					tm.tm_year = 2018;
				}
				tm.tm_year -= 1900;
				i = 0;
				chkptr = NULL;
				memset (d, 0, 8);
			}
		} else if (tm.tm_mon == 0) {
			d[i ++] = *p;
			if (i == 2) {
				tm.tm_mon = strtol(d, &chkptr, 10);
				tm.tm_mon -= 1;
				i = 0;
				chkptr = NULL;
				memset (d, 0, 8);
			}	
		} else if (tm.tm_mday == 0) {
			d[i ++] = *p;
			if (i == 2) {		
				tm.tm_mday = strtol(d, &chkptr, 10);
				i = 0;
				chkptr = NULL;
				memset (d, 0, 8);
			}	
		} else if (tm.tm_hour == 0) {
			d[i ++] = *p;
			if (i == 2) {
				tm.tm_hour = strtol(d, &chkptr, 10);
				//tm.tm_hour -= 1;
				i = 0;
				chkptr = NULL;
				memset (d, 0, 8);
			}	
		} else if (tm.tm_min == 0) {
			d[i ++] = *p;
			if (i == 2) {
				tm.tm_min = strtol(d, &chkptr, 10);
				//tm.tm_min -= 1;
				i = 0;
				chkptr = NULL;
				memset (d, 0, 8);
			}
		} else if (tm.tm_sec == 0) {
			d[i ++] = *p;
			if (i == 2) {
				tm.tm_sec = strtol(d, &chkptr, 10);
				//tm.tm_sec -= 1;
				i = 0;
				chkptr = NULL;
				memset (d, 0, 8);
			}
		}			
	}
				
/** fprintf(stdout, "%d-%d-%d %d:%d:%d\n",
		tm.tm_year, tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec); */

	return mktime(&tm);
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

