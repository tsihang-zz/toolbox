#include "oryx.h"

static int quit;

static void task_sigint(int sig)
{
	fprintf (stdout, "signal %d ...\n", sig);
	quit = 1;
}

static __oryx_always_inline__
void * t0_handler (void __oryx_unused_param__ *r)
{
		FOREVER {
			if (quit)
				break;
		}

		oryx_task_deregistry_id(pthread_self());
		return NULL;
}

static struct oryx_task_t t0 = {
		.module 		= THIS,
		.sc_alias		= "T0 Task",
		.fn_handler 	= t0_handler,
		.lcore_mask	= 0x0c,
		.ul_prio		= KERNEL_SCHED,
		.argc			= 0,
		.argv			= NULL,
		.ul_flags		= 0,	/** Can not be recyclable. */
};


int main (
        int     __oryx_unused_param__   argc,
        char    __oryx_unused_param__   ** argv
)

{
	oryx_initialize();
	
	oryx_register_sighandler(SIGINT, task_sigint);
	oryx_register_sighandler(SIGTERM, task_sigint);

	oryx_task_registry(&t0);

	oryx_task_launch();

	FOREVER {
		if (quit) {
			fprintf (stdout, "waiting for thread exit\n");
			break;
		}
		
		sleep (3);
	};
	
	return 0;
}

