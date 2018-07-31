#include "oryx.h"

static __oryx_always_inline__
void * t0_handler (void *r)
{
		FOREVER {
			
		}

		oryx_task_deregistry_id(pthread_self());
		return NULL;
}

static struct oryx_task_t t0 = {
		.module 		= THIS,
		.sc_alias		= "T0 Task",
		.fn_handler 	= t0_handler,
		.ul_lcore_mask	= 0x0c,
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

	oryx_task_registry(&t0);


	oryx_task_launch();

	FOREVER {
		sleep (3);
	};
	
	return 0;
}

