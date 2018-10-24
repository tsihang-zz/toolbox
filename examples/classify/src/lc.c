#include "oryx.h"
#include "config.h"

int fd;
uint64_t tx_pkts, tx_bytes;

#if 0
int sendto_ls(const void *buf, size_t buflen)
{
	ssize_t tx_bytes0 = 0;
	
	tx_bytes0 = send(fd, buf, strlen(buf), 0);
	if(tx_bytes0 > 0){
		tx_bytes += tx_bytes0;
		tx_pkts ++;
	} else {
		fprintf(stdout, "CLIENT %s\n", tx_bytes0 == 0 ? "peer server quit" : oryx_safe_strerror(errno));
		return -1;
	}
	return 0;
}

static __oryx_always_inline__
int classify_offline(const char *fname,
	ssize_t (*proc)(const void *buf, size_t buflen))
{
	FILE			*fp = NULL;
	size_t			llen = 0;
	uint64_t		nr_local_lines = 0;
	char			oldpath[128] = {0},
		 			newpath[128] = {0},
		 			line[lqe_valen] = {0};
	static uint64_t	nr_lines = 0;
	struct timeval	start, end;	
	vlib_main_t		*vm = &vlib_main;

	gettimeofday(&start, NULL);
	memset(inotify_file, 0, BUFSIZ);
	sprintf(inotify_file, "%s", fname);
	sprintf(oldpath, "%s", fname);
	if (!fp) {
		fp = fopen(oldpath, "r");
		if(!fp) {
			fprintf(stdout, "Cannot open %s \n", fname);
			return -1;
		}
	}

	vm->nr_rx_files ++;
	
	fprintf(stdout, "reading %s\n", oldpath);
	while (fgets (line, lqe_valen, fp)) {
		llen = strlen(line);
		nr_local_lines ++;
		/* skip first line. */
		if (nr_local_lines == 1)
			continue;

		if (proc(line, llen))
			return -1;
		
		memset (line, 0, lqe_valen);
	}
	fclose(fp);
	
	gettimeofday(&end, NULL);

	nr_lines += nr_local_lines;	
	
	/* break after end of file. */
	fprintf(stdout, "Finish read %s, %lu/%lu line(s) , cost %lu !\n",
			oldpath, nr_local_lines, nr_lines, tm_elapsed_us(&start, &end));
	
	sprintf (newpath, "%s/%s", vm->savdir, fname);
	rename(oldpath, newpath);
	fprintf(stdout, "* rename %s -> %s\n", oldpath, newpath);

	return 0;
}

static
void * local_client (void __oryx_unused_param__ *v)
{
    int r;
    static struct sockaddr_un saddr;
	static char buf[bufsize] = "1234567890qwertyuiopasdfghjklzxcvbnm";

	FOREVER {
		while(1) {
			fd = socket(PF_UNIX, SOCK_STREAM, 0);
			if(fd < 0) {
				fprintf(stdout, "CLIENT %s\n", oryx_safe_strerror(errno));
				return -1;
			}
			
			saddr.sun_family	=	AF_UNIX;
			strcpy(saddr.sun_path, UNIX_DOMAIN);

			r = connect(fd, (struct sockaddr*)&saddr, sizeof(saddr));
			if(r < 0){
				fprintf(stdout, "CLIENT %s\n", oryx_safe_strerror(errno));
				sleep(1);
				continue;
			}
			break;
		}
		
		while (1) {
			if (classify_offline("/data/DataExport.s1mmeSAMPLEMME_1540090678.csv",
				sendto_ls) < 0)
				break;
			sleep(1000);
			break;
		}
		close(fd);
	}
		
	oryx_task_deregistry_id(pthread_self());
	return NULL;
}

struct oryx_task_t lclient = {
		.module 		= THIS,
		.sc_alias		= "Local Client Task",
		.fn_handler 	= local_client,
		.lcore_mask		= INVALID_CORE,//(1 << ENQUEUE_LCORE_ID),
		.ul_prio		= KERNEL_SCHED,
		.argc			= 0,
		.argv			= NULL,
		.ul_flags		= 0,	/** Can not be recyclable. */
};
#endif

