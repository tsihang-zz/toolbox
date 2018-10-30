#include "oryx.h"
#include "config.h"

vlib_main_t vlib_main = {
	.argc		=	0,
	.argv		=	NULL,
	.prgname	=	"mme_classify",
	.nr_mmes	=	0,
	.mme_htable	=	NULL,
	.nr_threads =	0,
	.dictionary =	"/vsu/data2/zidian/classifyMme",
	.classdir   =	"/vsu1/db/cdr_csv/event_GEO_LTE/formated",
	.savdir     =	"/vsu1/db/cdr_csv/event_GEO_LTE/sav",
	.threshold  =	5,
	.dispatch_mode        = HASH,
	.max_entries_per_file = 80000,

};

int running = 1;

static void lq_sigint(int sig)
{
	fprintf (stdout, "signal %d ...\n", sig);
	running = 0;
}

int main (
        int     __oryx_unused_param__   argc,
        char    __oryx_unused_param__   ** argv
)
{
	vlib_main_t *vm = &vlib_main;
#if 0
	vlib_main_t *vm = oryx_shm_get(VLIB_MAIN_SHMKEY, sizeof(vlib_main));
	if(!(vm->ul_flags & VLIB_INIT_DONE))
		return 0;
	
	/* vlib_main is controlled by caller */
	memcpy(&vlib_main, vm, sizeof(vlib_main));
	fprintf(stdout, "%s\n", vm->inotifydir);
#endif

	oryx_register_sighandler(SIGINT, lq_sigint);
	oryx_register_sighandler(SIGTERM, lq_sigint);

	oryx_initialize();
	
	vm->argc = argc;
	vm->argv = argv;
	epoch_time_sec = time(NULL);

	vm->mme_htable = oryx_htable_init(DEFAULT_HASH_CHAIN_SIZE, 
								mmekey_hval, mmekey_cmp, mmekey_free, 0/* HTABLE_SYNCHRONIZED is unused,																		* because the table is no need to update.*/);	
	classify_env_init(vm);
	oryx_task_launch();
	FOREVER {
		if (!running) {
			/* wait for handlers of enqueue and dequeue finish. */
			sleep (3);
			classify_runtime();
			break;
		}
		memset(inotify_file, 0, BUFSIZ);
		sprintf(inotify_file, "%s", argv[1]);
		const vlib_fkey_t fkey = {
			.name = " ",
			.ul_flags = 0,
			.nr_size = 0,
			.nr_entries = 0,
		};
		classify_offline(inotify_file, &fkey);
		sleep(3);
		break;
	};
		
	classify_terminal();

	return 0;
}

