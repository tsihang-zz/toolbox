#include "oryx.h"
#include "config.h"
#include "fmgr.h"
#include <sys/inotify.h>

static struct oryx_htable_t *file_hash_tab = NULL; 

char *inotify_home = NULL;
char *store_home = NULL;

struct oryx_lq_ctx_t *fmgr_q = NULL;

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
	int err = 0;
	
	if (!v1 || !v2 || s1 != s2 ||
		memcmp(v1, v2, s2))
		err = 1;	/** Compare failed. */

	return err;
}

static vlib_conf_t *fkey_alloc(void)
{
	vlib_conf_t *v = malloc(sizeof (vlib_conf_t));
	BUG_ON(v == NULL);
	memset (v, 0, sizeof (vlib_conf_t));
	return v;
}


static __oryx_always_inline__
int fmgr_fchk (const char *f, char *date, char *port, bool chkfmt)
{
	char *p;
	int c = 0,
		i = 0,
		j = 0,
		pass = 0;
	
	for (p = (char *)&f[0], c = 0;
				*p != '\0' && *p != '\t'; ++ p) {
		if (*p == '_') c ++;
		if (c == 2) {
			if (*p != '_')
				date[i ++] = *p;
		}
		if (c == 3) {
			if (*p == '_')
				continue;
			if (*p == '.') {
				pass = 1;
				break;
			}
			port[j ++] = *p;
		}
	}

	if (chkfmt && !strncmp(p, "AVL", 3)) {		
		pass = 0;
	}

	//fprintf(stdout, "-%s-%s-pass(%d)\n", date, port, pass);

	return pass;
}

extern time_t	regx_time_from;
extern time_t	regx_time_to;

static int fmgr_inotify_timedout
(
	IN void *argv,
	IN char *pathname,
	char *filename
)
{
	int err;
	struct stat buf;
	vlib_conf_t *key;	/* search hash table first */
	vlib_main_t *vm = (vlib_main_t *)argv;
	char date[1024] = {0}, port[8] = {0};
	bool skip = false;

	if (!fmgr_fchk(filename, date, port, (vm->ul_flags & VLIB_REGX_FNAME_CHK))) {
		skip = true;
	}

	if (!skip && (vm->ul_flags & VLIB_REGX_FTIME_CHK)) {
		time_t t = regx_date2stamp(date);
		fprintf(stdout, "-%s-%s-%lu\n", date, port, t);
		if (t < regx_time_from || t > regx_time_to) {
			skip = true;
		}
	}

	if (skip) {
		fprintf(stdout, "%s -\n", pathname);
		return 0;
	}
	
	void *s = oryx_htable_lookup(file_hash_tab, filename, strlen(filename));
	if (s) {
		return 0;
	} else {
		static int n = 0;

		err = stat(pathname, &buf);
		if(err) {
			fprintf(stdout, "stat %s\n", oryx_safe_strerror(errno));
			return 0;
		}
		vm->nr__size += buf.st_size;

		struct fq_element_t *fqe = fqe_alloc();
		if (fqe) {
			/* Tring to add a file to hash table by fkey. */
			key = fkey_alloc();
			if (!key)
				return 0;

			memcpy(key->name, filename, strlen(filename));
			BUG_ON(oryx_htable_add(file_hash_tab, key->name, strlen(key->name)) != 0);
			/* enqueue abslute pathname */
			strcpy(fqe->name, filename);
			oryx_lq_enqueue(fmgr_q, fqe);
			vm->nr__files ++;
			fprintf(stdout, "%s +\n", pathname);
		}
	}

	return 0;
}

void fmgr_move(const char *oldpath, const vlib_conf_t *vf)
{
	vlib_conf_t *fkey;	/* search hash table first */
	char	*f;
	
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
	char	*f;
	int err;
	
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

void vlib_fmgr_init(vlib_main_t *vm)
{
	int try_scan_dir = 0;
	
	file_hash_tab = oryx_htable_init(DEFAULT_HASH_CHAIN_SIZE, 
								fkey_hval, fkey_cmp, fkey_free, HTABLE_SYNCHRONIZED);
	oryx_lq_new("fmgr queue", 0, &fmgr_q);
	
	oryx_foreach_directory_file (inotify_home,
					fmgr_inotify_timedout, vm, try_scan_dir);
	
	vm->ul_flags |= VLIB_REGX_LD_FIN;
}


