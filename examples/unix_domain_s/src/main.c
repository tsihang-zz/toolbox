#include "oryx.h"
#include "unix_domain_config.h"

int running = 1;

static void sigint_handler(int sig) {
	fprintf (stdout, "signal %d ...\n", sig);
	running = 0;
}


int main(
	int 	__oryx_unused__	argc,
	char	__oryx_unused__	** argv
)
{
	oryx_initialize();
	oryx_register_sighandler(SIGINT,	sigint_handler);
	oryx_register_sighandler(SIGTERM,	sigint_handler);

	vlib_unix_domain_t *vm = oryx_shm_get(VLIB_UNIX_DOMAIN_SHMKEY, sizeof(vlib_unix_domain_t));
	if(!vm) {
		return 0;
	}

	//oryx_task_registry(&unix_domain_client);
	oryx_task_registry(&unix_domain_server);
	unix_domain_server.argv = vm;
	unix_domain_server.argc = 1;
	
	oryx_task_launch();
	
	FOREVER {
		if(!running) {
			fprintf(stdout, "exit!\n");
			unlink(VLIB_UNIX_DOMAIN);
			break;
		}
		uint64_t	tx_pkts0 = vm->tx_pkts,
					rx_pkts0 = vm->rx_pkts;
		
		fprintf(stdout, "Tx %lu, Rx %lu, delta %lu\n", tx_pkts0, rx_pkts0, (tx_pkts0 - rx_pkts0));
		sleep(1);
	}
	
	return 0;
}

