#include "oryx.h"
#include "main.h"
#include <sys/inotify.h>

struct oryx_lq_ctx_t *fmgr_q = NULL;
static struct oryx_htable_t *file_hash_tab = NULL; 

char *inotify_dir = "/vsu/db/cdr_csv/event_GEO_LTE";
char inotify_file[BUFSIZ];

uint64_t nr_classified_files = 0;
uint64_t nr_handlers = 0;
uint64_t nr_fopen_times = 0;
uint64_t nr_fopen_times_r = 0;
uint64_t nr_fopen_times_error = 0;

#define FKEY_ADDED	(1 << 0)
typedef struct vlib_fkey_t {
	char name[128];
	uint32_t ul_flags;
}vlib_fkey_t;


struct event_mask {  
    int        flag;  
    const char *name;  
  
}; 
struct event_mask event_masks[] = {  
	   {IN_ACCESS		 , "IN_ACCESS"} 	   ,	
	   {IN_ATTRIB		 , "IN_ATTRIB"} 	   ,	
	   {IN_CLOSE_WRITE	 , "IN_CLOSE_WRITE"}   ,	
	   {IN_CLOSE_NOWRITE , "IN_CLOSE_NOWRITE"} ,	
	   {IN_CREATE		 , "IN_CREATE"} 	   ,	
	   {IN_DELETE		 , "IN_DELETE"} 	   ,	
	   {IN_DELETE_SELF	 , "IN_DELETE_SELF"}   ,	
	   {IN_MODIFY		 , "IN_MODIFY"} 	   ,	
	   {IN_MOVE_SELF	 , "IN_MOVE_SELF"}	   ,	
	   {IN_MOVED_FROM	 , "IN_MOVED_FROM"}    ,	
	   {IN_MOVED_TO 	 , "IN_MOVED_TO"}	   ,	
	   {IN_OPEN 		 , "IN_OPEN"}		   ,	

	   {IN_DONT_FOLLOW	 , "IN_DONT_FOLLOW"}   ,	
	   {IN_EXCL_UNLINK	 , "IN_EXCL_UNLINK"}   ,	
	   {IN_MASK_ADD 	 , "IN_MASK_ADD"}	   ,	
	   {IN_ONESHOT		 , "IN_ONESHOT"}	   ,	
	   {IN_ONLYDIR		 , "IN_ONLYDIR"}	   ,	

	   {IN_IGNORED		 , "IN_IGNORED"}	   ,	
	   {IN_ISDIR		 , "IN_ISDIR"}		   ,	
	   {IN_Q_OVERFLOW	 , "IN_Q_OVERFLOW"}    ,	
	   {IN_UNMOUNT		 , "IN_UNMOUNT"}	   ,	
}; 

static void fkey_free (const ht_value_t __oryx_unused_param__ v)
{
	/** Never free here! */
}

static ht_key_t fkey_hval (struct oryx_htable_t *ht,
		const ht_value_t v, uint32_t __oryx_unused_param__ s)
{
	uint8_t *d = (uint8_t *)v;
	uint32_t i;
	uint32_t hv = 0;

	 for (i = 0; i < s; i++) {
		 if (i == 0)	  hv += (((uint32_t)*d++));
		 else if (i == 1) hv += (((uint32_t)*d++) * s);
		 else			  hv *= (((uint32_t)*d++) * i) + s + i;
	 }

	 hv *= s;
	 hv %= ht->array_size;
	 
	 return hv;
}

static int fkey_cmp (const ht_value_t v1, uint32_t s1,
		const ht_value_t v2, uint32_t s2)
{
	int xret = 0;
	
	if (!v1 || !v2 || s1 != s2 ||
		memcmp(v1, v2, s2))
		xret = 1;	/** Compare failed. */

	return xret;
}

static vlib_fkey_t *fkey_alloc(void)
{
	vlib_fkey_t *v = malloc(sizeof (vlib_fkey_t));
	BUG_ON(v == NULL);
	memset (v, 0, sizeof (vlib_fkey_t));
	return v;
}

static __oryx_always_inline__
int skip_event(struct inotify_event *event)
{
	return (!event->len || !strncmp(event->name, ".", 1) || !strncmp(event->name, "..", 2));
}

static void * inotify_handler (void __oryx_unused_param__ *r)
{
	const char *path = inotify_dir;
	int fd;
	int wd;
	int len;
	int nread;
	char buf[BUFSIZ];
	struct inotify_event *event;
	char *name;
	char abs_file[256] = {0};
	
	fd = inotify_init();
	if(fd < 0) {
		fprintf(stderr, "inotify_init failed\n");
		goto finish;
	}

	wd = inotify_add_watch(fd, path, IN_ALL_EVENTS);
	if(wd < 0) {
		fprintf(stderr, "inotify_add_watch %s failed\n", path);
		goto finish;
	}
	
	buf[BUFSIZ - 1] = 0;

	file_hash_tab = oryx_htable_init(DEFAULT_HASH_CHAIN_SIZE, 
								fkey_hval, fkey_cmp, fkey_free, 0);

	FOREVER {		
		nread = 0;
		len = read(fd, buf, sizeof(buf) - 1);
		if (len < 0)
			continue;
		while (len > 0) {
			event = (struct inotify_event *)&buf[nread];
			name = event->name;
			if (skip_event(event))
				goto next;
	
			if (event->mask & IN_CREATE) {
				if(event->mask & IN_ISDIR)
					fprintf (stdout, "Create DIR %s\n", name);
				else
					fprintf (stdout, "[CRT] %s\n", name);
			} else if (event->mask & IN_OPEN) {
				fprintf (stdout, "[OPN] %s\n", name);
			} else if (event->mask & IN_MODIFY) {
				//fprintf (stdout, "[MOD] %s\n", name);
			} else if (event->mask & IN_DELETE) {
				fprintf (stdout, "[DEL] %s\n", name);
			} else if (event->mask & IN_CLOSE_NOWRITE) {
				fprintf (stdout, "[CLS] %s\n", name);
			} else if (event->mask & IN_CLOSE_WRITE) {
			   	fprintf(stdout, "[*CLS] %s\n", name);
				sprintf (abs_file, "%s/%s", path, name);
				if (access(abs_file, F_OK))
					goto next;

				/* search hash table first */
				vlib_fkey_t *key;
				void *s = oryx_htable_lookup(file_hash_tab, name, strlen(name));
				if (s) {
					key = (vlib_fkey_t *) container_of (s, vlib_fkey_t, name);
					if (key != NULL) {
						fprintf (stdout, "[FID] %s\n", key->name);
						//BUG_ON(oryx_htable_del(file_hash_tab, key->name, strlen(key->name)) != 0);
						goto next;
					} else {
						fprintf (stdout, "Cannot assign a file for %s! exiting ...\n", key->name);
						exit (0);
					}
				} else {
					struct fq_element_t *fqe = fqe_alloc();
					if (fqe) {
						strcpy(fqe->name, abs_file);
						oryx_lq_enqueue(fmgr_q, fqe);

						/* Tring to add a file to hash table by fkey. */
						key = fkey_alloc();
						if (key) {
							memcpy(key->name, name, strlen(name));
							key->ul_flags |= FKEY_ADDED;
							BUG_ON(oryx_htable_add(file_hash_tab, key->name, strlen(key->name)) != 0);
						}
					}
				}
			}
	next:
			nread = nread + sizeof(struct inotify_event) + event->len;
			len = len - sizeof(struct inotify_event) - event->len;
	  	}
	}
	
finish:
	fprintf(stdout, "EXIT .....\n");
	oryx_task_deregistry_id(pthread_self());
	return NULL;
}

static int do_timeout_check(void *argv, char *pathname, char *filename)
{
	int			err,
				n = 0,
				timeout_sec = *(int *)argv;
	struct stat buf;
	vlib_fkey_t *key;	/* search hash table first */
	char		*p,
				move[256] = {0},
				newpath[256] = {0},
				mmename[32] = {0};
	time_t		now = time(NULL);	
	vlib_main_t	*vm = &vlib_main;

	/* Not a CSV file */
	if(!strstr(filename, ".csv"))
		return 0;

	err = stat(pathname, &buf);
	if(err) {
		fprintf(stdout, "stat %s\n", oryx_safe_strerror(errno));
		return 0;
	}
	/* file without any modifications in 60 seconds will be timeout and removed. */
	if(now < (buf.st_mtime + timeout_sec)) {
		fprintf(stdout, ".");
		return 0;
	}

#if 0
	/* find MME name by the given string */
	for(p = (filename + strlen(MME_CSV_PREFIX)); *p != '_'; ++ p) {
		mmename[n ++] = *p;
	}

	if(!mme_find(mmename, n)) {
		fprintf(stdout, "Cannot find mme named \"%s\" skip\n", mmename);
		return 0;
	}
#endif

	void *s = oryx_htable_lookup(file_hash_tab, filename, strlen(filename));
	if (s) {
		key = (vlib_fkey_t *) container_of (s, vlib_fkey_t, name);
		if (key != NULL) {
			/* To make sure that we cannot process files duplicated. */
			fprintf (stdout, "[FID] %s\n", key->name);
			//BUG_ON(oryx_htable_del(file_hash_tab, key->name, strlen(key->name)) != 0);
			return 0;
		} else {
			fprintf (stdout, "Cannot assign a file for %s! exiting ...\n", key->name);
			exit (0);
		}
	} else {
		struct fq_element_t *fqe = fqe_alloc();
		if (fqe) {
			strcpy(fqe->name, pathname);
			oryx_lq_enqueue(fmgr_q, fqe);

			/* Tring to add a file to hash table by fkey. */
			key = fkey_alloc();
			if (key) {
				memcpy(key->name, filename, strlen(filename));
				key->ul_flags |= FKEY_ADDED;
				BUG_ON(oryx_htable_add(file_hash_tab, key->name, strlen(key->name)) != 0);
			}
		}
	}

	return 0;
}

static void * inotify_handler0 (void __oryx_unused_param__ *r)
{
	const char *path = inotify_dir;
	const int timeout_sec = 10;


	file_hash_tab = oryx_htable_init(DEFAULT_HASH_CHAIN_SIZE, 
								fkey_hval, fkey_cmp, fkey_free, 0);

	FOREVER {
		foreach_directory_file (path,
					do_timeout_check, (void *)&timeout_sec, 0);
		sleep(1);
	}
	
	oryx_task_deregistry_id(pthread_self());
	return NULL;
}

struct oryx_task_t inotify = {
		.module 		= THIS,
		.sc_alias		= "Inotify Task",
		.fn_handler 	= inotify_handler0,
		.lcore_mask		= INVALID_CORE,
		.ul_prio		= KERNEL_SCHED,
		.argc			= 0,
		.argv			= NULL,
		.ul_flags		= 0,	/** Can not be recyclable. */
};


