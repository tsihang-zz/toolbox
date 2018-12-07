#include "oryx.h"
#include "config.h"
#include "dpdk.h"
#include "iface.h"
#include "dp.h"

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

int main (
	int		__oryx_unused__	argc,
	char	__oryx_unused__	** argv
)
{
	int err;
	uint32_t id_core;
	
	oryx_initialize();
	oryx_register_sighandler(SIGINT, sigint_handler);
	oryx_register_sighandler(SIGTERM, sigint_handler);

	vlib_iface_init(&vlib_main);

	err = box_init (argc, argv);
	if (err) exit(0);
	
	oryx_task_launch();

	dp_start(&vlib_main);

#if defined(HAVE_DPDK)
	RTE_LCORE_FOREACH_SLAVE(id_core) {
		if (rte_eal_wait_lcore(id_core) < 0)
			return -1;
		/* wait for dataplane quit. */
		if(vlib_main.ul_core_mask & VLIB_QUIT)
			break;
		sleep(3);
	}
#else
	FOREVER {
		/* wait for dataplane quit. */
		if(vlib_main.ul_core_mask & VLIB_QUIT)
			break;
		sleep(3);
	}
#endif
	
	dp_stop(&vlib_main);

	return 0;
}

