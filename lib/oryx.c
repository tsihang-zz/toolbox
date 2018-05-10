#include "oryx.h"

oryx_status_t oryx_initialize(void) 
{
#if defined(HAVE_APR)
	apr_initialize();
#endif

	/** task management module. */
	oryx_task_initialize ();

	/** timer management module. */
	oryx_tmr_initialize ();

	oryx_atomic_initialize (NULL);

	printf ("oryx initialized success.\n");

	oryx_system_preview ();
	
	return ORYX_SUCCESS;
}


