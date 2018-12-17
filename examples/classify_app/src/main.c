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
const char *progname;
char sc_raw_file[BUFSIZ] = {0};

static void sigint_handler(int sig) {
	fprintf (stdout, "signal %d ...\n", sig);
	running = 0;
}


/**
 * Prints out usage information to stdout
 */
static void
usage(void)
{
	fprintf (stdout,
	    "%s -f FILE\n"
	    " -f FILE: RAW CDR CSV file (example: $APP DataExport.s1mmeSAMPLEMME_1544154900.csv)\n"
	    , progname);
}

static int
vlib_args_parse
(
	IN int argc,
	IN char **argv
)
{
	int	c;
	
	while ((c = getopt(argc, argv, "f:")) != -1) {
		switch (c){
			case 'f':
				sprintf(sc_raw_file, "%s", optarg);
				break;
			default:
				printf("ERROR: Unknown option '%c'\n", c);
				return -1;
		}
	}
	
	if (sc_raw_file[0] == 0) {
		return -1;
	}
	
	return 0;
}

int main (
        int     __oryx_unused__   argc,
        char    __oryx_unused__   ** argv
)
{

	int err;
	vlib_main_t *vm = &vlib_main;

	progname = argv[0];

	err = vlib_args_parse(argc, argv);
	if (err){		
		usage();
		exit(0);
	}

	oryx_initialize();
	oryx_register_sighandler(SIGINT, sigint_handler);
	oryx_register_sighandler(SIGTERM, sigint_handler);

	epoch_time_sec = time(NULL);

	vm->mme_htable = oryx_htable_init(DEFAULT_HASH_CHAIN_SIZE, 
								mmekey_hval, mmekey_cmp, mmekey_free, 0/* HTABLE_SYNCHRONIZED is unused,																		* because the table is no need to update.*/);	
	classify_initialization(vm);

	vlib_conf_t conf = {
		.ul_flags = VLIB_CONF_NONE
	};
	classify_offline(sc_raw_file, &conf);
	classify_result(sc_raw_file, &conf);

	classify_terminal();

	return 0;
}

