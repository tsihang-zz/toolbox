#include "oryx.h"

oryx_status_t oryx_initialize(void) 
{
#if defined(HAVE_APR)
	apr_initialize();
#endif
	oryx_log_init();

	/** task management module. */
	oryx_task_initialize();

	/** timer management module. */
	oryx_tmr_initialize();

	oryx_atomic_initialize(NULL);

	oryx_pcre_initialize();
	
	/* Initialize the configuration module. */
	ConfInit();


	printf ("oryx initialized success.\n");

	oryx_system_preview ();
	
	return ORYX_SUCCESS;
}


