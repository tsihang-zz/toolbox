#include "oryx.h"

static quit;

static void tmr_sigint(int sig)
{
	fprintf (stdout, "signal %d ...\n", sig);
	quit = 1;
}

int main (
	int		__oryx_unused__	argc,
	char	__oryx_unused__	** argv
)
{
	struct oryx_timer_t *tmr0, *tmr1, *tmr2;
	uint32_t ul_tmr_setting_flags0, ul_tmr_setting_flags1, ul_tmr_setting_flags2;

	oryx_initialize();

	oryx_register_sighandler(SIGINT, tmr_sigint);
	oryx_register_sighandler(SIGTERM, tmr_sigint);

	ul_tmr_setting_flags0 = TMR_OPTIONS_PERIODIC;
	tmr0 = oryx_tmr_create (1, "test_timer0", ul_tmr_setting_flags0,
											  oryx_tmr_default_handler, 0, NULL, 3000);
	oryx_tmr_start(tmr0);


	ul_tmr_setting_flags1 = TMR_OPTIONS_PERIODIC | TMR_OPTIONS_ADVANCED;
	tmr1 = oryx_tmr_create (1, "test_timer1", ul_tmr_setting_flags1,
											  oryx_tmr_default_handler, 0, NULL, 3000);
	oryx_tmr_start(tmr1);


	ul_tmr_setting_flags2 = TMR_OPTIONS_PERIODIC;
	tmr2 = oryx_tmr_create (1, "test_timer2", ul_tmr_setting_flags2,
											  oryx_tmr_default_handler, 0, NULL, 3000);
	oryx_tmr_start(tmr2);


	oryx_task_launch();
	
	FOREVER {
		if (quit)
			break;
	}

	oryx_tmr_destroy(tmr0);
	oryx_tmr_destroy(tmr1);
	
	return 0;
}

