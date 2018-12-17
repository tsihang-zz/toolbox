#include "oryx.h"
#include "config.h"
#include "fmgr.h"

#define VLIB_MAX_PARALLELS	16
#define VLIB_BUFSIZE	4096

enum {
	REGX_MD_NONE,
	REGX_MD_S1AP_HANDOVER_SIGNAL,
	REGX_MD_S1_EMM_SIGNAL,
	REGX_MD_S1_MME_SIGNAL,
	REGX_MD_WAP_SIGNAL,
};

static char *progname;
static int nr_regxs = 1;
static char	regx_module = REGX_MD_NONE;
vlib_main_t *vlib_main;

struct vlib_lqe_t {
	char value[VLIB_BUFSIZE];
	size_t valen;
};

/*
 * print a usage message
 */
static void
usage(void)
{
	printf (
		"%s -m SIGNAL -n <THREADNUM> -s SRCDIR -d DSTDIR\n"
		" -m SIGNAL     : S1AP_HANDOVER_SIGNAL,S1_EMM_SIGNAL,S1_MME_SIGNAL,WAP_SIGNAL\n"
		" -t <THREADNUM>: Number of threads in parallels\n"
		" -s SRCDIR     : A directory where the recoverying CSV files come from\n"
		" -d DSTDIR     : A directory Where the recovered CSV files stored to \n"
		, progname);
}

static __oryx_always_inline__
void sigint_handler(int sig) {
	fprintf (stdout, "signal %d ...\n", sig);
	vlib_main->ul_flags |= VLIB_REGX_QUIT;
}

static __oryx_always_inline__
void sigchld_handler(int num) {
	num = num;
    int status;   
    pid_t pid = waitpid(-1, &status, 0);   
    if (WIFEXITED(status)) {
		if(WEXITSTATUS(status) != 0)
        	printf("The child %d exit with code %d\n", pid, WEXITSTATUS(status));   
    }   
}

static FILE *regx_open_logfile(void)
{
	vlib_main_t *vm = vlib_main;
	char tmstr[100] = {0};
	char logfile[64] = {0};
	
	oryx_fmt_time (vm->epoch_time_sec_start, "%Y%m%d%H%M%S", (char *)&tmstr[0], 100);
	sprintf(logfile, "%s.%s", VLIB_REGX_LOGFILE, tmstr);
	return fopen(logfile, "a+");
}

static __oryx_always_inline__
void logprgname(void)
{
	char logzomb[1024] = {0};
	char prgname[32] = {0};
	const char *zombie = "./zombie.txt";
		
	if (oryx_path_exsit(zombie))
		remove(zombie);
	
	oryx_get_prgname(getpid(), prgname);
#if defined(REGX_DEBUG)	
	fprintf(stdout, "$$> hello, it's %s\n", prgname);
#endif
	sprintf(logzomb, "ps -ef | grep %s | grep -v grep | grep defunct | awk '{print$2}' >> %s", prgname, zombie);
	do_system(logzomb);

}

static __oryx_always_inline__
void logprogress(void) {
	vlib_main_t *vm = vlib_main;
	const char *f = "/tmp/regx.progress";
	uint64_t now = time(NULL);
	char st_size_buf[20] = {0}, st_rx_size_buf[20] = {0};
	
	if (oryx_path_exsit(f))
		remove(f);	
	FILE *fp = fopen(f, "a+");
	if (!fp) {
		return;
	}

	oryx_fmt_program_counter(vm->nr__size, st_size_buf, 0, 0);
	oryx_fmt_program_counter(vm->nr_rx_size, st_rx_size_buf, 0, 0);

	fprintf(fp, "Cost %lu secs, Recoveried %lu/%lu file(s), %s/%s (%.2f%%)\n",
		(now-vm->epoch_time_sec_start), vm->nr_rx_files, vm->nr__files,
		st_rx_size_buf, st_size_buf, ratio_of(vm->nr_rx_size, vm->nr__size));
	fprintf(stdout, "Cost %lu secs, Recoveried %lu/%lu file(s), %s/%s (%.2f%%)\n",
		(now-vm->epoch_time_sec_start), vm->nr_rx_files, vm->nr__files,
		st_rx_size_buf, st_size_buf, ratio_of(vm->nr_rx_size, vm->nr__size));

	fflush(fp);
	fclose(fp);	
}

static void regx_parse_dir(const char *s, char **d)
{
	size_t i;
	size_t l = strlen(s) + 1;
	char *v = malloc (l);
	memset (v, 0, l);
	
	for (i = 0; i < strlen(s); i ++)
		v[i] = s[i];

	if (v[l] == '/')
		v[l] = '\0';
	
	(*d) = v;
}

static __oryx_always_inline__
int baker_entry (vlib_conf_t *conf,
	char *value, struct vlib_lqe_t *lqe)
{
	char		*p, *pn;
	int			c = 0;
	int			pt = 0;		/* the number of char '(' */

	if (regx_module == REGX_MD_S1AP_HANDOVER_SIGNAL) {
		for (p = (char *)&value[0];
					*p != '\0' && *p != '\t'; ++ p) {
			if (*p == ',') c ++;
			if (c == VLIB_REGX_S1AP_HANDOVER_SP_NUM
				&& (*p == '\t' || *p == ' '))
				break;
			lqe->value[lqe->valen ++] = (*p == ',') ? 0x01 : *p;
		}
#if defined(REGX_DT_FROM_DB)
		lqe->value[lqe->valen ++]		= '\n';
#endif
	} else if (regx_module == REGX_MD_S1_EMM_SIGNAL) {
		for (p = (char *)&value[0];
					*p != '\0' && *p != '\t'; ++ p) {
			if (*p == ',') c ++;
			if (c == VLIB_REGX_EMM_SP_NUM
				&& (*p == '\t' || *p == ' '))
				break;
			lqe->value[lqe->valen ++] = (*p == ',') ? 0x01 : *p;
		}
	} else if (regx_module == REGX_MD_S1_MME_SIGNAL) {
		for (p = (char *)&value[0];
					*p != '\0' && *p != '\t'; ++ p) {
			if (*p == ',') c ++;
			if (c == VLIB_REGX_MME_SP_NUM
				&& (*p == '\t' || *p == ' '))
				break;
			lqe->value[lqe->valen ++] = (*p == ',') ? 0x01 : *p;
		}
	} else if (regx_module == REGX_MD_WAP_SIGNAL) {
		for (p = (char *)&value[0], pn = (char *)&value[1];
					*p != '\0'; ++ p, ++pn) {
			if (*p == '(') pt ++;
			if (*p == ')' && pt) {
				pt --;
				BUG_ON(pt < 0);	
			}
			if (*p == ',' && !pt) c ++;
			lqe->value[lqe->valen ++] = (*p == ',' && !pt) ? 0x01 : *p;
		}
		if (c > VLIB_REGX_WAP_SP_NUM) {			
			conf->nr_err_skip_entries ++;
			lqe->valen = 0;	/* skip this entry. */
		}
	}
	
	return 0;
}

static __oryx_always_inline__
int baker_entry0 (char *value, struct vlib_lqe_t *lqe)
{
	char		*p;

	
	for (p = (char *)&value[0];
				*p != '\0'; ++ p)
		lqe->value[lqe->valen ++] = (*p == ',') ? 0x01 : *p;
	
	lqe->value[lqe->valen ++]		= '\n';
	//lqe->value[lqe->valen - 1]	= '\0';
	
	return 0;
}

static __oryx_always_inline__
void baker_first_line (char *value)
{
	char		*p;
	int c = 0;
	
	for (p = (char *)&value[0];
				*p != '\0'; ++ p) {
		if (*p == ',') {
			c++;
			fprintf(stdout, "\n");
		}
		else {
			fprintf(stdout, "%c", *p);
		}
	}
	fprintf(stdout, "\n Total %d c\n", c);
}

static __oryx_always_inline__
int do_regx_entries (
	vlib_conf_t *conf,
	IN FILE *fp,
	IN char *value,
	OUT size_t *valen
)
{
	int err;
	struct vlib_lqe_t *lqe,
						lqe0 = {
							.value	= {0},
							.valen	= 0,
						};
	lqe = &lqe0;
	baker_entry (conf, value, lqe);
	if (lqe->valen) {
		err = lqe->valen != fwrite(lqe->value, sizeof(char), lqe->valen, fp);
		if (!err)
			(*valen) = lqe->valen;
	}

	return err;
}

static int regx_result (vlib_main_t *vm, const char *f, vlib_conf_t *conf)
{
	FILE				*fp = NULL;
	char				pps_str0[20],
						pps_str1[20],
						file_size_buf[20] = {0},
						tmstr[100] = {0};

#if defined(REGX_DEBUG)
	fprintf(stdout, "\nRegx Result\n");
	fprintf(stdout, "%3s%12s%s\n",	" ", "File: ",	f);
	fprintf(stdout, "%3s%12s%lu/%lu, cost %lu usec\n", " ", "Entries: ",
		conf->nr_entries, conf->nr_entries, conf->tv_usec);
	fprintf(stdout, "%3s%12s%s/s, avg %s/s\n",	" ", "Speed: ",
		oryx_fmt_program_counter(fmt_pps(conf->tv_usec, conf->nr_entries), pps_str0, 0, 0),
		oryx_fmt_program_counter(fmt_pps(conf->tv_usec, conf->nr_entries), pps_str1, 0, 0));
#endif

	if (conf->ul_flags & VLIB_CONF_LOG) {
		fp = regx_open_logfile();
		if (!fp) return 0;

		oryx_fmt_time (time(NULL), "%Y-%m-%d %H:%M:%S", (char *)&tmstr[0], 100);
		fprintf(fp, "%s, %.2f%%, %s/%s, %lu entries (PC %.2f%%), %s, cost %lu usec, %s/s\n",
				tmstr,
				ratio_of(vm->nr_rx_size, vm->nr__size),
				inotify_home, f,
				conf->nr_entries,
				ratio_of(conf->nr_entries - conf->nr_err_skip_entries, conf->nr_entries),
				oryx_fmt_program_counter(conf->nr_size, file_size_buf, 0, 0),
				conf->tv_usec,
				oryx_fmt_program_counter(fmt_pps(conf->tv_usec, conf->nr_entries), pps_str0, 0, 0));
		fclose(fp);
	}
	
	return 0;
}

static int do_regx(const char *f, vlib_conf_t *conf)
{
	FILE				*fp = NULL,
						*fw = NULL;
	char				value[VLIB_BUFSIZE]	= {0};
	size_t				valen;
	struct timeval		start,
						end;
	char				s[256] = {0}, d[256] = {0};
	int err;

	vlib_shm_t shm = {
		.shmid = 0,
		.addr = 0,
	};
	err = oryx_shm_get(VLIB_SHM_BASE_KEY, sizeof(vlib_main_t), &shm);
	if (err) {
		return -1;
	}	
	vlib_main_t *vm = (vlib_main_t *)shm.addr;
	
	sprintf(s, "%s/%s", inotify_home, f);
	sprintf(d, "%s/%s", store_home, f);
	
	if ((fp = fopen(s, "r")) == NULL) {
		fprintf (stdout, "fopen: %s \n", oryx_safe_strerror(errno));
		return -1;
	}
	
	if ((fw = fopen(d, "a+")) == NULL) {
		fprintf (stdout, "fopen: %s \n", oryx_safe_strerror(errno));
		fclose(fp);
		return -1;
	}

	vm->nr_sub_regx_proc ++;
	
	err = gettimeofday(&start, NULL);
	if(err) {
		fprintf(stdout, "gettimeofday: %s\n", oryx_safe_strerror(errno));
	}

#if defined(REGX_DEBUG)
	fprintf (stdout, "\nReading %s\n", s);
#endif

	/* A loop read the part of lines. */
	FOREVER {
		memset (value, 0, VLIB_BUFSIZE);
		valen = 0;

		/* wait for current file finished. */
		if(!fgets (value, VLIB_BUFSIZE, fp)
			/* || regx_is_quit(vm)*/)
			break;
#if 0
		static int first_line = 1;
		if (first_line) {
			baker_first_line(value);
			first_line = 0;
			continue;
		}
#endif
		do_regx_entries(conf, fw, value, &valen);
		conf->nr_entries ++;
		conf->nr_size += valen;
	}
	
	fclose(fp);
	fclose(fw);

	gettimeofday(&end, NULL);	
	conf->tv_usec = oryx_elapsed_us(&start, &end);
	
	/* clean up all file handlers */

	if (conf->ul_flags & VLIB_CONF_RM) {
		err = remove(s);
		if(err) {
			fprintf(stdout, "\nremove: %s\n", oryx_safe_strerror(errno));
		}
	}

	regx_result(vm, f, conf);

	vm->nr_sub_regx_proc --;
	vm->nr_rx_files ++;
	vm->nr_rx_entries += conf->nr_entries;
	vm->nr_rx_size += conf->nr_size;
	vm->nr_rx_err_skip_entries += conf->nr_err_skip_entries;
	
	return 0;
}


static int regx_init(int argc, char **argv)
{
	char c;
	vlib_main_t *vm = vlib_main;

	vm->epoch_time_sec_start = time(NULL);
	progname = argv[0];
	
	while ((c = getopt(argc, argv, "VF:s:d:n:m:")) != -1) {
		switch ((char)c) {
			case 'V':
			{
				printf("%s Verson %s. BUILD %s\n", progname, "1", "aaa");
				exit(0);
			}

			case 'm':
				{
					if (!strcmp(optarg, "S1_MME_SIGNAL"))
						regx_module = REGX_MD_S1_MME_SIGNAL;
					if (!strcmp(optarg, "S1_EMM_SIGNAL"))
						regx_module = REGX_MD_S1_EMM_SIGNAL;
					if (!strcmp(optarg, "S1AP_HANDOVER_SIGNAL"))
						regx_module = REGX_MD_S1AP_HANDOVER_SIGNAL;
					if (!strcmp(optarg, "WAP_SIGNAL"))
						regx_module = REGX_MD_WAP_SIGNAL;
				}
				break;
			
			case 's':
				regx_parse_dir(optarg, &inotify_home);	
				break;
			
			case 'd':
				regx_parse_dir(optarg, &store_home);
				break;
				
			case 'n':
				{
					char *chkptr;
					nr_regxs = strtol(optarg, &chkptr, 10);
					if ((chkptr != NULL && *chkptr == 0)) {
						break;
					}
					return -1;
				}
			default:
				fprintf(stdout, "error\n");
				return -1;
		}
	}

	if (nr_regxs > VLIB_MAX_PARALLELS) {
		fprintf(stdout, "%d nr_regx(s), out of range %d\n", nr_regxs, VLIB_MAX_PARALLELS);
		return -1;
	}

	if (!inotify_home || !store_home) {
		fprintf(stdout, "undefined src or dst home\n");
		return -1;
	}

	if (regx_module == REGX_MD_NONE) {
		fprintf(stdout, "undefined regx_module %d\n", regx_module);
		return -1;
	}

	if (!oryx_path_exsit(inotify_home)) {
		fprintf(stdout, "Cannot find inotify_home %s\n", inotify_home);
		return -1;
	}
	
	if (!oryx_path_exsit(store_home)) {
		char mkdir0[256] = {0};
		sprintf(mkdir0, "mkdir -p %s", store_home);
		do_system(mkdir0);
	}
	
	fprintf(stdout, "%s %s, module %d\n", inotify_home, store_home, regx_module);
	return 0;
}

static void regx_finish(void)
{
	vlib_main_t *vm = vlib_main;	
	uint64_t now = time(NULL);
	char tsstr[100] = {0}, testr[100] = {0};
	char st_size_buf[20] = {0}, st_rx_size_buf[20] = {0};	
	FILE *fp = regx_open_logfile();
	if (!fp) return;
		
	oryx_fmt_program_counter(vm->nr__size, st_size_buf, 0, 0);
	oryx_fmt_program_counter(vm->nr_rx_size, st_rx_size_buf, 0, 0);
	
	oryx_fmt_time (vm->epoch_time_sec_start,
		"%Y-%m-%d %H:%M:%S", (char *)&tsstr[0], 100);
	oryx_fmt_time (now,
		"%Y-%m-%d %H:%M:%S", (char *)&testr[0], 100);

	fprintf(fp, "Start %s, End %s, Cost %lu secs, Recover %lu/%lu file(s), %s/%s (%.2f%%)\n",
		tsstr, testr, (now-vm->epoch_time_sec_start), vm->nr_rx_files, vm->nr__files,
		st_rx_size_buf, st_size_buf, ratio_of(vm->nr_rx_size, vm->nr__size));

	fprintf(stdout, "Start %s, End %s, Cost %lu secs, Recover %lu/%lu file(s), %s/%s (%.2f%%)\n",
		tsstr, testr, (now-vm->epoch_time_sec_start), vm->nr_rx_files, vm->nr__files,
		st_rx_size_buf, st_size_buf, ratio_of(vm->nr_rx_size, vm->nr__size));


	fflush(fp);
	fclose(fp);
}

int main (
        int     __oryx_unused__   argc,
        char    __oryx_unused__   ** argv
)
{
	struct fq_element_t *fqe = NULL;	
	int err;

	vlib_shm_t shm = {
		.shmid = 0,
		.addr = 0,
			
	};
	err = oryx_shm_get(VLIB_SHM_BASE_KEY, sizeof(vlib_main_t), &shm);
	if (err) {
		exit (0);
	}
	
	vlib_main = (vlib_main_t *)shm.addr;
	vlib_main_t *vm = vlib_main;
	memset (vm, 0, sizeof(vlib_main_t));	

	oryx_initialize();
	oryx_register_sighandler(SIGINT,	sigint_handler);
	oryx_register_sighandler(SIGTERM,	sigint_handler);
	//oryx_register_sighandler(SIGCHLD,	sigchld_handler);
	signal(SIGCHLD,	SIG_IGN);

	err = regx_init(argc, argv);
	if (err)  {
		usage();
		exit(0);
	}

	vlib_fmgr_init(vm);
	
	oryx_task_launch();
	FOREVER {
		logprgname();
		logprogress();
		//sleep (1);
		
		if (regx_is_quit(vm)) {
			/* wait for all subproc quit. */
			while (vm->nr_sub_regx_proc);
			fprintf (stdout, "quit\n");
			break;
		}

		/* source file is not prepared */
		if (!(vm->ul_flags & VLIB_REGX_LD_FIN))
			continue;
		
	#if defined(REGX_DEBUG)
		fprintf(stdout, "%lu/%lu file(s), %d\n",
			vm->nr_rx_files, vm->nr__files, vm->nr_sub_regx_proc);
	#endif
	
		if (vm->nr_sub_regx_proc > nr_regxs) {
			continue;
		}
		
		fqe = oryx_lq_dequeue(fmgr_q);
		if (fqe == NULL && vm->nr_sub_regx_proc == 0) {
			fprintf(stdout, "No more files (%lu/%lu), breaking ...\n",
				 vm->nr_rx_files, vm->nr__files);
			vm->ul_flags |= VLIB_REGX_QUIT;
			break;
		}
	/*
		if(!oryx_path_exsit(fqe->name)){
			free(fqe);
			continue;
		}
	*/
		pid_t p = fork();
		if (p < 0) {
			fprintf(stdout, "fork: %s\n", oryx_safe_strerror(errno));
			free(fqe);
			continue;
		} else if (p == 0) {
	#if defined(REGX_DEBUG)
			fprintf(stdout, "Children %d -> %s\n", getpid(), fqe->name);
	#endif
			vlib_conf_t conf = {
				.ul_flags = VLIB_CONF_LOG
			};
			do_regx(fqe->name, &conf);
			exit(0);
		}
		
		free(fqe);
	}

	logprogress();
	regx_finish();
	
	return 0;
}

