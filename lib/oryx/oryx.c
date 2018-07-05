#include "oryx.h"

oryx_status_t oryx_initialize(void) 
{
	struct oryx_fmt_buff_t fb = FMT_BUFF_INITIALIZATION;
	struct timeval start, end;
	
	gettimeofday(&start,NULL);

#if defined(HAVE_APR)
	apr_initialize();
	oryx_format(&fb, "%s", "apr_initialize, ");
#endif

	oryx_pcre_initialize();
	oryx_format(&fb, "%s", "oryx_pcre_initialize, ");

	oryx_atomic_initialize(NULL);
	oryx_format(&fb, "%s", "oryx_atomic_initialize, ");

	oryx_log_init();
	oryx_format(&fb, "%s", "oryx_log_init, ");

	oryx_counter_init();
	oryx_format(&fb, "%s", "oryx_counter_init, ");

	/** task management module. */
	oryx_task_initialize();
	oryx_format(&fb, "%s", "oryx_task_initialize, ");

	/** timer management module. */
	oryx_tmr_initialize();
	oryx_format(&fb, "%s", "oryx_tmr_initialize, ");
	
	/* Initialize the configuration module. */
	ConfInit();
	oryx_format(&fb, "%s", "ConfInit, ");

	MpmTableSetup();
	oryx_format(&fb, "%s", "MpmTableSetup");

	gettimeofday(&end, NULL);

	oryx_format(&fb, "oryx cost %lu, initialized success", 
		tm_elapsed_us(&start, &end));

	printf ("%s", fb.fmt_data);
	oryx_system_preview ();

	oryx_format_free(&fb);

	return ORYX_SUCCESS;
}


