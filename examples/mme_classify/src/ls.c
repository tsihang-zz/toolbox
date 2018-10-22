#include "oryx.h"
#include "main.h"
#include "cfg.h"

uint64_t rx_pkts, rx_bytes;

#if 0
static struct timeval start, end;

static
void * local_server (void __oryx_unused_param__ *v)
{
    int fd0;
    int fd;
    int r;
    int len;
    struct sockaddr_un caddr;
    struct sockaddr_un saddr;
	uint64_t nr_cost_usec;
	char buf[bufsize] = {0};

    fd0 = socket(PF_UNIX,SOCK_STREAM,0);
    if(fd0 < 0) {
		fprintf (stdout, "SERVER %s\n", oryx_safe_strerror(errno));
        return -1;
    }  

    unlink(UNIX_DOMAIN);
    
    saddr.sun_family = AF_UNIX;
    strncpy(saddr.sun_path, UNIX_DOMAIN, sizeof(saddr.sun_path) - 1);

    r = bind(fd0, (struct sockaddr*)&saddr, sizeof(saddr));
    if(r < 0) {
		fprintf(stdout, "SERVER %s\n", oryx_safe_strerror(errno));
        close(fd0);
        unlink(UNIX_DOMAIN);
        return -1;
    }
	
    r = listen(fd0, 1);
    if(r < 0) {
		fprintf(stdout, "SERVER %s\n", oryx_safe_strerror(errno));
        close(fd0);
        unlink(UNIX_DOMAIN);
        return -1;
    }

	FOREVER {
		while(1) {
			len = sizeof(caddr);			
			fd = accept(fd0, (struct sockaddr*)&caddr, &len);
			if(fd < 0) {
				fprintf(stdout, "SERVER %s\n", oryx_safe_strerror(errno));
				sleep(1);
				continue;
			}
			break;
		}
		while (1) {
			ssize_t rx_bytes0 = 0;

			rx_bytes0 = recv(fd, buf, bufsize, 0);
			if(rx_bytes0 > 0) {
				rx_bytes += rx_bytes0;
				rx_pkts ++;
			} else {
				fprintf(stdout, "SERVER %s\n", rx_bytes0 == 0 ? "peer client quit" : oryx_safe_strerror(errno));
				break;
			}
		}
		
		close(fd);
	}
		
	oryx_task_deregistry_id(pthread_self());
	return NULL;
}

struct oryx_task_t lserver = {
		.module 		= THIS,
		.sc_alias		= "Local Server Task",
		.fn_handler 	= local_server,
		.lcore_mask		= INVALID_CORE,//(1 << ENQUEUE_LCORE_ID),
		.ul_prio		= KERNEL_SCHED,
		.argc			= 0,
		.argv			= NULL,
		.ul_flags		= 0,	/** Can not be recyclable. */
};
#endif

