#include "oryx.h"
#include "unix_domain_config.h"

static FILE *fp;

static __oryx_always_inline__
int baker_entry(const char *value, size_t vlen)
{
	char		*p,
				time[32] = {0};
	const char	sep = ',';
	int 		sep_refcnt = 0;
	size_t		step, tlen = 0;
	time_t		tv_usec = 0;
	bool		keep_cpy				= 1,
				event_start_time_cpy	= 0,
				find_imsi				= 0;
	
	for (p = (const char *)&value[0], step = 0, sep_refcnt = 0;
				*p != '\0' && *p != '\n'; ++ p, step ++) {			

		/* skip entries without IMSI */
		if (!find_imsi && sep_refcnt == 5) {
			if (*p == sep) {
				//break;
			}
			else
				find_imsi = 1;
		}

		/* skip first 2 seps and copy event_start_time */
		if (sep_refcnt == 2) {
			event_start_time_cpy = 1;
		}

		/* skip last three columns */
		if (sep_refcnt == 43)
			keep_cpy = 0;

		/* valid entry dispatch ASAP */
		if (sep_refcnt == 45)
			goto dispatch;

		if (*p == sep)
			sep_refcnt ++;

		/* stop copy */
		if (sep_refcnt == 3) {
			event_start_time_cpy = 0;
		}

		/* soft copy.
		 * Time stamp is 13 bytes-long */
		if (event_start_time_cpy) {
			time[tlen ++] = *p;
		}
	}

	return -1;

dispatch:
	return 0;
}

static
void * unix_domain_server_handler (void __oryx_unused_param__ *v)
{
    int fd0 = -1,
		fd = -1,
		err,
		len;
    struct sockaddr_un caddr;
    struct sockaddr_un saddr;
	uint64_t nr_cost_usec;
	char buf[VLIB_BUFSIZE] = {0};
	vlib_unix_domain_t *vm = (vlib_unix_domain_t *)v;

	fp = fopen("./unix_domain.txt", "a+");
	if(!fp) goto quit;

    fd0 = socket(PF_UNIX, SOCK_STREAM, 0);
    if(fd0 < 0) {
		fprintf(stdout, "socket: %s\n", oryx_safe_strerror(errno));
        return NULL;
    }  

    saddr.sun_family = AF_UNIX;
    strncpy(saddr.sun_path, VLIB_UNIX_DOMAIN, sizeof(saddr.sun_path) - 1);

    err = bind(fd0, (struct sockaddr*)&saddr, sizeof(saddr));
    if(err < 0) {
		fprintf(stdout, "bind: %s\n", oryx_safe_strerror(errno));
        goto quit;
    }
	
    err = listen(fd0, 1);
    if(err < 0) {
		fprintf(stdout, "listen: %s\n", oryx_safe_strerror(errno));
        goto quit;
    }

	vm->ul_flags |= VLIB_UD_SERVER_STARTED;
	
	FOREVER {
		while(1) {
			len = sizeof(caddr);			
			fd = accept(fd0, (struct sockaddr*)&caddr, &len);
			if(fd < 0) {
				fprintf(stdout, "accpet: %s\n", oryx_safe_strerror(errno));
				sleep(1);
				continue;
			}
			break;
		}
		fprintf(stdout, "new connection %d\n", fd);
		while (1) {
			if(!running)
				goto quit;	

			ssize_t rx_bytes0 = 0;

			rx_bytes0 = recv(fd, buf, VLIB_BUFSIZE, 0);
			if(rx_bytes0 > 0) {
				vm->rx_bytes += rx_bytes0;
				vm->rx_pkts ++;
				baker_entry(buf, rx_bytes0);
				fwrite(buf, sizeof(char), rx_bytes0, fp);
			} else if(rx_bytes0 == 0) {
				fprintf(stdout, "recv: %s\n", "client quit");
				break;
			} else {
				fprintf(stdout, "recv: %s\n", oryx_safe_strerror(errno));
				break;
			}
		}
		
		close(fd);
	}
	
quit:
	vm->ul_flags &= ~VLIB_UD_SERVER_STARTED;
	if (fd0 > 0)close(fd0);
	if (fd > 0) close(fd);
	if(oryx_path_exsit(VLIB_UNIX_DOMAIN))unlink(VLIB_UNIX_DOMAIN);
	oryx_task_deregistry_id(pthread_self());
	return NULL;
}

struct oryx_task_t unix_domain_server = {
		.module 		= THIS,
		.sc_alias		= "Local Server Task",
		.fn_handler 	= unix_domain_server_handler,
		.lcore_mask		= INVALID_CORE,//(1 << ENQUEUE_LCORE_ID),
		.ul_prio		= KERNEL_SCHED,
		.argc			= 1,
		.argv			= NULL,
		.ul_flags		= 0,	/** Can not be recyclable. */
};

