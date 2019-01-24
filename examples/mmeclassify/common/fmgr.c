#include "oryx.h"
#include "config.h"
#include <sys/inotify.h>

struct oryx_lq_ctx_t *fmgr_q = NULL;
static struct oryx_htable_t *file_hash_tab = NULL; 

uint64_t nr_classified_files = 0;
uint64_t nr_handlers = 0;
uint64_t nr_fopen_times = 0;
uint64_t nr_fopen_times_r = 0;
uint64_t nr_fopen_times_error = 0;

#if 0
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
#endif

static void fkey_free (const ht_value_t __oryx_unused__ v)
{
	/** Never free here! */
}

static ht_key_t fkey_hval (struct oryx_htable_t *ht,
		const ht_value_t v, uint32_t __oryx_unused__ s)
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

static vlib_conf_t *fkey_alloc(void)
{
	vlib_conf_t *v = malloc(sizeof (vlib_conf_t));
	BUG_ON(v == NULL);
	memset (v, 0, sizeof (vlib_conf_t));
	return v;
}

static __oryx_always_inline__
int skip_event(struct inotify_event *event)
{
	return (!event->len || !strncmp(event->name, ".", 1) || !strncmp(event->name, "..", 2));
}

static __oryx_always_inline__
void * fmgr_handler (void __oryx_unused__ *r)
{
	const char *path = inotify_home;
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
								fkey_hval, fkey_cmp, fkey_free, HTABLE_SYNCHRONIZED);

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
					fprintf(stdout, "Create DIR %s\n", name);
				else
					fprintf(stdout, "[CRT] %s\n", name);
			} else if (event->mask & IN_OPEN) {
				fprintf(stdout, "[OPN] %s\n", name);
			} else if (event->mask & IN_MODIFY) {
				//fprintf(stdout, "[MOD] %s\n", name);
			} else if (event->mask & IN_DELETE) {
				fprintf(stdout, "[DEL] %s\n", name);
			} else if (event->mask & IN_CLOSE_NOWRITE) {
				fprintf(stdout, "[CLS] %s\n", name);
			} else if (event->mask & IN_CLOSE_WRITE) {
			   	fprintf(stdout, "[*CLS] %s\n", name);
				sprintf (abs_file, "%s/%s", path, name);
				if (access(abs_file, F_OK))
					goto next;

				/* search hash table first */
				vlib_conf_t *key;
				void *s = oryx_htable_lookup(file_hash_tab, name, strlen(name));
				if (s) {
					key = (vlib_conf_t *) container_of (s, vlib_conf_t, name);
					if (key != NULL) {
						fprintf(stdout, "[FID] %s\n", key->name);
						//BUG_ON(oryx_htable_del(file_hash_tab, key->name, strlen(key->name)) != 0);
						goto next;
					} else {
						fprintf(stdout, "Cannot assign a file for %s! exiting ...\n", key->name);
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

static int fmgr_inotify_timedout(void *argv, char *pathname, char *filename)
{
	int			err,
				n = 0;
				//timeout_sec = *(int *)argv;
	vlib_tm_grid_t *vtg = (vlib_tm_grid_t *)argv;	
	vlib_conf_t *key;	/* search hash table first */
	char		*p,
				stime[16] = {0};
	
	/* Not a CSV file */
	if(!strstr(filename, ".csv")) {
		return 0;
	}
	if(!strstr(filename, "s1mmeSAMPLEMME")) {
		return 0;
	}

	/* We are going to wait for current TM grid out. */
	uint64_t	start;				
	p = strstr(filename, "_");
	for(n = 0, p += 1; *p != '.'; ++ p)
		stime[n ++] = *p;
	sscanf(stime, "%lu", &start);
	if((int64_t)vtg->start <= (int64_t)start) {
		fprintf(stdout, "#");
		return 0;
	}

#if 1
	struct stat buf;
	err = stat(pathname, &buf);
	if(err) {
		fprintf(stdout, "stat %s\n", oryx_safe_strerror(errno));
		return 0;
	}
	
	/* check file whether any modifications in 5 seconds or not. */
	if(time(NULL) < (buf.st_mtime + 5)) {
		fprintf(stdout, "@");
		return 0;
	}
#endif
	void *s = oryx_htable_lookup(file_hash_tab, filename, strlen(filename));
	if (s) {
		return 0;
	} else {
		struct fq_element_t *fqe = fqe_alloc();
		if (fqe) {
			fprintf(stdout, "fqe: %s\n", filename);
			/* Tring to add a file to hash table by fkey. */
			key = fkey_alloc();
			if (key) {
				memcpy(key->name, filename, strlen(filename));
				BUG_ON(oryx_htable_add(file_hash_tab, key->name, strlen(key->name)) != 0);
			}
			/* enqueue abslute pathname */
			strcpy(fqe->name, pathname);
			oryx_lq_enqueue(fmgr_q, fqe);
		}
	}

	return 0;
}

static int fmgr_classify_timedout (void *argv, char *pathname, char *filename)
{
	int 		err,
				n = 0;
	struct stat buf;	
	char		*p,
				move[256] = {0},
				newpath[256] = {0},
				mmename[32] = {0};

	vlib_main_t *vm = &vlib_main;
	int nr_blocks = (24 * 60) / vm->threshold;

	uint64_t start;
	vlib_tm_grid_t vtg, *vtg0 = (vlib_tm_grid_t *)argv;

	/* Not a CSV file */
	if(!strstr(filename, ".csv"))
		return 0;
	if(!strstr(filename, MME_CSV_PREFIX))
		return 0;

	err = stat(pathname, &buf);
	if(err) {
		fprintf(stdout, "stat %s\n", oryx_safe_strerror(errno));
		return 0;
	}
	
	/* file without any modifications in 60 seconds will be timeout and removed. */
	if(time(NULL) < (buf.st_mtime + 300)) {
		fprintf(stdout, ".");
		return 0;
	}

	/* find MME name by the given string */
	for(p = (filename + strlen(MME_CSV_PREFIX)); *p != '_'; ++ p) {
		mmename[n ++] = *p;
	}

	vlib_mme_t *mme = mme_find(mmename, n);
	if(!mme) {
		fprintf(stdout, "Cannot find mme named \"%s\"\n", mmename);
		return 0;
	}

	char	stime[16] = {0},
			etime[16] = {0};
	
	for(n = 0, p += 1; *p != '_'; ++ p)
		stime[n ++] = *p;
	for(n = 0, p += 1; *p != '.'; ++ p)
		etime[n ++] = *p;
	
	sscanf(stime, "%lu", &start);
	calc_tm_grid(&vtg, vm->threshold, start);

#if 0
	if((int64_t)vtg0->start <= (int64_t)(vtg.start + vm->threshold * 60)) {
		fprintf(stdout, ".");
		return 0;
	}
#endif

	file_close(&mme->farray[vtg.block % nr_blocks]);

	sprintf (newpath, "%s/%s/", vm->classdir, mmename);
	sprintf(move, "mv %s %s", pathname, newpath);
	fprintf(stdout, "\n(*)timeout %s\n", move);
	err = do_system(move);
	if (err) {
		fprintf(stdout, "\nmv: %s\n", oryx_safe_strerror(errno));
	}
	
	return 0;
}

void fmgr_move(const char *oldpath, const vlib_conf_t *vf)
{
	vlib_conf_t *fkey;	/* search hash table first */
	char	*f;
	
	f = strstr(oldpath, MME_CSV_PREFIX);
	if (!f) {
		fprintf(stdout, "Invalid %s\n", f);
		return;
	}

	void *s = oryx_htable_lookup(file_hash_tab, f, strlen(f));
	if (!s) {
		fprintf(stdout, "Cannot find %s in hash table\n", f);
		return;
	}
	
	fkey = (vlib_conf_t *) container_of (s, vlib_conf_t, name);
	if (fkey != NULL) {
		/* To make sure that we cannot process files duplicated. */
		fkey->nr_entries = vf->nr_entries;
		fkey->nr_size = vf->nr_size;
	} else {
		fprintf(stdout, "Cannot assign a file for %s! exiting ...\n", fkey->name);
		exit(0);
	}
}

void fmgr_remove(const char *oldpath)
{
	vlib_conf_t *fkey;	/* search hash table first */
	char	*f;
	int err;
	
	f = strstr(oldpath, MME_CSV_PREFIX);
	if (!f) {
		fprintf(stdout, "Invalid %s\n", f);
		return;
	}

	void *s = oryx_htable_lookup(file_hash_tab, f, strlen(f));
	if(!s) {		
		BUG_ON(s == NULL);
	} else {
		err = oryx_htable_del(file_hash_tab, f, strlen(f));
		if (err) {
			fprintf(stdout, "Cannot delete file %s from hash table\n", f);
		} else {
			fprintf(stdout, "Delete file %s from hash table\n", f);
		}
	}
}

static void fmgr_ht_handler(ht_value_t v,
				uint32_t s,
				void *opaque,
				int __oryx_unused__ opaque_size) {
	char pathname[256] = {0};
	
	vlib_conf_t *fkey = (vlib_conf_t *)v;
	fprintf(stdout, "==== %s\n", fkey->name);
	sprintf(pathname, "%s/%s", inotify_home, fkey->name);
   /* If this file removed successfully,
	* then delete it from hash table. */
   if (!oryx_path_exsit(pathname)) {
   }
}

static void * fmgr_handler0 (void __oryx_unused__ *r)
{
	int try_scan_dir = 0;
	vlib_main_t *vm = &vlib_main;

	vlib_tm_grid_t vtg = {
		.start = 0,
		.end = 0,
		.block = 0,
	};
		
	file_hash_tab = oryx_htable_init(DEFAULT_HASH_CHAIN_SIZE, 
								fkey_hval, fkey_cmp, fkey_free, HTABLE_SYNCHRONIZED);

	FOREVER {
		calc_tm_grid(&vtg, vm->threshold, time(NULL));
	
		oryx_foreach_directory_file (inotify_home,
					fmgr_inotify_timedout, (void *)&vtg, try_scan_dir);

#if defined(HAVE_CLASSIFY_HOME)
		oryx_foreach_directory_file (classify_home,
					fmgr_classify_timedout, (void *)&vtg, try_scan_dir);
#endif

#if 0
		int refcount = oryx_htable_foreach_elem(file_hash_tab,
					fmgr_ht_handler, NULL, 0);
#endif
		/* aging entries in table. */
		sleep(1);
		fprintf(stdout, ".");
	}
	
	oryx_task_deregistry_id(pthread_self());
	return NULL;
}

struct oryx_task_t inotify = {
		.module 		= THIS,
		.sc_alias		= "Inotify Task",
		.fn_handler 	= fmgr_handler0,
		.lcore_mask		= INVALID_CORE,
		.ul_prio		= KERNEL_SCHED,
		.argc			= 0,
		.argv			= NULL,
		.ul_flags		= 0,	/** Can not be recyclable. */
};


