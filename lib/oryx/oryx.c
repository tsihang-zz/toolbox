#include "oryx.h"

static void oryx_system(void)
{
	struct passwd *pw;
	struct utsname uts;

	if (uname(&uts) < 0) {
		fprintf(stderr, "Unable to get the host information.\n");
		return ;
	}

	fprintf (stdout, "\r\nSystem Preview\n");
	fprintf (stdout, "%30s:%60s\n", "Login User", getlogin());	
	fprintf (stdout, "%30s:%60s\n", "Runtime User", (((pw = getpwuid(getuid())) != NULL) ? pw ->pw_name : "Unknown"));
	fprintf (stdout, "%30s:%60s\n", "Host", uts.nodename);
	fprintf (stdout, "%30s:%60s\n", "Arch", uts.machine);
	fprintf (stdout, "%30s:%60d\n", "Bits/LONG", __BITS_PER_LONG);
	fprintf (stdout, "%30s:%60s\n", "Platform", uts.sysname);
	fprintf (stdout, "%30s:%60s\n", "Kernel", uts.release);
	fprintf (stdout, "%30s:%60s\n", "OS", uts.version);
	fprintf (stdout, "%30s:%60d\n", "CPU", (int)sysconf(_SC_NPROCESSORS_ONLN));

	fprintf (stdout, "\r\n\n");
 
}

oryx_status_t oryx_initialize(void) 
{
	struct oryx_fmt_buff_t fb = FMT_BUFF_INITIALIZATION;
	struct timeval start, end;
	
	gettimeofday(&start,NULL);

	oryx_pcre_initialize();
	oryx_format(&fb, "%s", "oryx_pcre_initialize, ");

	oryx_atomic_initialize();
	oryx_format(&fb, "%s", "oryx_atomic_initialize, ");

	oryx_log_initialize();
	oryx_format(&fb, "%s", "oryx_log_init, ");

	oryx_counter_initialize();
	oryx_format(&fb, "%s", "oryx_counter_init, ");

	/** task management module. */
	oryx_task_initialize();
	oryx_format(&fb, "%s", "oryx_task_initialize, ");

	/** timer management module. */
	oryx_tmr_initialize();
	oryx_format(&fb, "%s", "oryx_tmr_initialize, ");
	
	//mpm_table_setup();
	//oryx_format(&fb, "%s", "mpm_table_setup");

	gettimeofday(&end, NULL);

	oryx_format(&fb, "oryx cost %lu, initialized success", 
		tm_elapsed_us(&start, &end));

	fprintf (stdout, "%s", fb.fmt_data);
	oryx_system();

	oryx_format_free(&fb);

	return ORYX_SUCCESS;
}


