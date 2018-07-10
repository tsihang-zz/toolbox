#include "oryx.h"


int main(int argc, char ** argv)
{
	uint32_t val_start, val_end;
	int ret;

	oryx_initialize();
	
	struct oryx_timer_t *tmr0, *tmr1, *tmr2;
	uint32_t ul_tmr_setting_flags0, ul_tmr_setting_flags1, ul_tmr_setting_flags2;

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
	
	FOREVER;
	
	return 0;
}

