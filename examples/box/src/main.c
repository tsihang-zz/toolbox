#include "oryx.h"
#include "config.h"
#include "dpdk.h"
#include "iface.h"
#include "dp.h"
#include "cli.h"

static unsigned short int alternative_port = 12000;
struct thread_master *master;

extern int box_init
(
	IN int argc,
	IN char **argv
);

static void
sigint_handler
(
	IN int signum
)
{
	vlib_main_t *vm = &vlib_main;
	
	if (signum == SIGINT || signum == SIGTERM) {
		notify_dp(vm, signum);
		/** */
		vm->ul_flags |= VLIB_QUIT;
	}
}

static
void *run_cli (void __oryx_unused__*pv_par)
{
	char *vty_addr = NULL;
	short vty_port = ZEBRA_VTY_PORT;
	struct thread thread;

	/* Make vty server socket. */
	if (alternative_port != 0) {
		vty_port = alternative_port;
	}
	
	fprintf (stdout, "Command Line Interface Initialization done (%d)!\n", vty_port);
	
	/* Create VTY socket */
	vty_serv_sock(vty_addr, vty_port, ZEBRA_VTYSH_PATH);
	
	/* Configuration file read*/
	vty_read_config(config_current, config_default);
	
	FOREVER {
		/* Fetch next active thread. */
		while (thread_fetch(master, &thread))
			thread_call(&thread);
	}
	oryx_task_deregistry_id(pthread_self());
	return NULL;
}

static struct oryx_task_t cli_register =
{
	.module = THIS,
	.sc_alias = "CLI Task",
	.fn_handler = run_cli,
	.lcore_mask = INVALID_CORE,
	.ul_prio = KERNEL_SCHED,
	.argc = 0,
	.argv = NULL,
	.ul_flags = 0,	/** Can not be recyclable. */
};


int main (
	int		__oryx_unused__	argc,
	char	__oryx_unused__	** argv
)
{
	int err;
	uint32_t id_core;
	vlib_main_t *vm = &vlib_main;
	
	oryx_initialize();
	oryx_register_sighandler(SIGINT, sigint_handler);
	oryx_register_sighandler(SIGTERM, sigint_handler);

	/* master init. */
	master = thread_master_create();
	/* Library inits. */
	cmd_init(1);
	vty_init(master);
	oryx_task_registry(&cli_register);
	
	vlib_iface_init(vm);

	err = box_init (argc, argv);
	if (err) exit(0);
	
	oryx_task_launch();

	dp_start(vm);

#if defined(HAVE_DPDK)
	RTE_LCORE_FOREACH_SLAVE(id_core) {
		if (rte_eal_wait_lcore(id_core) < 0)
			return -1;
		/* wait for dataplane quit. */
		if(vm->ul_core_mask & VLIB_QUIT)
			break;
		
		/* wait for a little while. */
		sleep(3);
	}
#else
	FOREVER {
		/* wait for dataplane quit. */
		if(vm->ul_core_mask & VLIB_QUIT)
			break;
		sleep(3);
	}
#endif
	
	dp_stop(vm);
	dp_stats(vm);

	return 0;
}

