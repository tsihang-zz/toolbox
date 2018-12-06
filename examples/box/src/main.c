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
		vm->force_quit = true;

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
	
	oryx_initialize();
	oryx_register_sighandler(SIGINT, sigint_handler);
	oryx_register_sighandler(SIGTERM, sigint_handler);

	vlib_iface_init(&vlib_main);

	err = box_init (argc, argv);
	if (err) exit(0);

	dp_start(&vlib_main);
	
	oryx_task_launch();
	FOREVER;

	return 0;
}

