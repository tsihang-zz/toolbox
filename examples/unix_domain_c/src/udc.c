#include "oryx.h"
#include "config.h"

int unix_domain_sock;

static
void * unix_domain_client_handler (void __oryx_unused_param__ *v)
{
    int r;
	static uint64_t errors = 0;
    static struct sockaddr_un saddr;
	static char buf[VLIB_BUFSIZE] = ",,1540456948977,1538102098324,11,232011830178601,867246032451460,,,,,,,,1022472,,,,,,,,,,,,,,,,,,,,,,,,,,363333789,13759948,611A820A,200,3716589318,10.110.18.88";
	vlib_main_t *vm = (vlib_main_t *)v;

	while(!(vm->ul_flags & VLIB_UD_SERVER_STARTED)){
		sleep(1);
	}
	
	FOREVER {
		while(1) {
			unix_domain_sock = socket(PF_UNIX, SOCK_STREAM, 0);
			if(unix_domain_sock < 0) {
				fprintf(stdout, "socket: %s\n", oryx_safe_strerror(errno));			
				return NULL;
			}
			
			saddr.sun_family	=	AF_UNIX;
			strcpy(saddr.sun_path, VLIB_UNIX_DOMAIN);
			while ((r = connect(unix_domain_sock, (struct sockaddr*)&saddr, sizeof(saddr))) < 0) {
				fprintf(stdout, "connect: %s\n", oryx_safe_strerror(errno));
				sleep(3);
				continue;
			}
			break;
		}
		
		while (1) {
			if(!running)
				goto quit;
			
			ssize_t tx_bytes0 = 0;

			tx_bytes0 = send(unix_domain_sock, buf, VLIB_BUFSIZE, 0);
			if(tx_bytes0 > 0) {
				vm->tx_bytes += tx_bytes0;
				vm->tx_pkts ++;
			} else if(tx_bytes0 == 0) {
				fprintf(stdout, "send: peer quit\n");
				break;
			} else {
				fprintf(stdout, "%lu send: %s\n", errors ++, oryx_safe_strerror(errno));
				break;
			}
		}
		close(unix_domain_sock);
	}
quit:
	close(unix_domain_sock);
	oryx_task_deregistry_id(pthread_self());
	return NULL;
}

struct oryx_task_t unix_domain_client = {
		.module 		= THIS,
		.sc_alias		= "Local Client Task",
		.fn_handler 	= unix_domain_client_handler,
		.lcore_mask		= INVALID_CORE,//(1 << ENQUEUE_LCORE_ID),
		.ul_prio		= KERNEL_SCHED,
		.argc			= 0,
		.argv			= NULL,
		.ul_flags		= 0,	/** Can not be recyclable. */
};

