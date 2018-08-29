#include "oryx.h"
#include "version.h"
#include "zebra.h"
#include "memory.h"
#include "log.h"
#include "version.h"
#include "vector.h"
#include "vty.h"
#include "command.h"
#include "prefix.h"
#include "dp_decode.h"
#include "dp_main.h"
#include "cli_appl.h"
#include "cli_iface.h"
#include "cli_map.h"

/** CLI decalre */
extern void common_cli(vlib_main_t *vm);

vlib_main_t vlib_main = {
	.prgname = "napd",
};

static unsigned short int alternative_port = 12000;
struct thread_master *master;

static __oryx_always_inline__ 
void tmr_default_handler(struct oryx_timer_t *tmr, int __oryx_unused_param__ argc, 
                char __oryx_unused_param__**argv)
{
    fprintf (stdout, "default %s-timer routine has occured on [%s, %u, %d]\n",
		tmr->ul_setting_flags & TMR_OPTIONS_ADVANCED ? "advanced" : "sig",
		tmr->sc_alias, tmr->tmr_id, tmr->ul_cyclical_times);
}

static __oryx_always_inline__
void * run_cli (void __oryx_unused_param__*pv_par)
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

static void
sig_handler(int signum) {

	vlib_main_t *vm = &vlib_main;
	
	if (signum == SIGINT || signum == SIGTERM) {
		vm->force_quit = true;

		notify_dp(vm, signum);
		/** */
		vm->ul_flags |= VLIB_QUIT;
	}
}

int main (
	int		argc,
	char ** argv
)
{
	vlib_main.argc = argc;
	vlib_main.argv = argv;
	vlib_main.prgname = argv[0];
	
	uint32_t id_core;

	oryx_register_sighandler(SIGINT, sig_handler);
	oryx_register_sighandler(SIGTERM, sig_handler);

	oryx_initialize();

	if (ConfYamlLoadFile(CONFIG_PATH_YAML) == -1) {
		fprintf (stdout, "ConfYamlLoadFile error\n");
		return 0;
	}

	/* master init. */
	master = thread_master_create();
	/* Library inits. */
	cmd_init(1);
	vty_init(master);
	memory_init();
	
	vlib_iface_init(&vlib_main);
	vlib_appl_init(&vlib_main);
	vlib_map_init(&vlib_main);

	common_cli(&vlib_main);
	oryx_task_registry(&cli_register);

	oryx_task_launch();
	sleep(1);
	dp_start(&vlib_main);

#if defined(HAVE_DPDK)
	RTE_LCORE_FOREACH_SLAVE(id_core) {
		if (rte_eal_wait_lcore(id_core) < 0)
			return -1;
		/* wait for dataplane quit. */
		if(vlib_main.force_quit)
			break;
	}
#else
	FOREVER {
		/* wait for dataplane quit. */
		if(vlib_main.force_quit)
			break;
	}
#endif

	dp_end(&vlib_main);

	return 0;

}
