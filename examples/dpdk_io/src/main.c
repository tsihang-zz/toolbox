#include "oryx.h"
#include "dpdk_cfg.h"


/**
 * Prints out usage information to stdout
 */
static void
usage(void)
{
	fprintf (stdout,
	    "%s [EAL options] -- -p PORTMASK -n NUM_CLIENTS\n"
	    " -p PORTMASK: hexadecimal bitmask of ports to use\n"
	    , oryx_cfg_get()->progname);
}


/**
 * The application specific arguments follow the DPDK-specific
 * arguments which are stripped by the DPDK init. This function
 * processes these application arguments, printing usage info
 * on error.
 */
int parse_args (int argc, char *argv[])
{
	int option_index, opt;
	char **argvopt = argv;
	static struct option lgopts[] = { /* no long options */
		{NULL, 0, 0, 0 }
	};
	oryx_cfg_get()->progname = argv[0];

	while ((opt = getopt_long(argc, argvopt, "p:", lgopts,
		&option_index)) != EOF){
		switch (opt){
			case 'p':
				/* get total number of ports which supported by DPDK.
				 * and parse them with a given $portmask. */
				if (dpdk_parse_portmask(rte_eth_dev_count(), optarg) != 0) {
					usage();
					return -1;
				}
				break;

			default:
				fprintf(stdout, "ERROR: Unknown option '%c'\n", opt);
				usage();
				return -1;
		}
	}

	return 0;
}

int main (
	int argc,
    char ** argv)
{
	int retval;
	
	oryx_initialize();

	/* init EAL, parsing EAL args */
	retval = rte_eal_init(argc, argv);
	if (retval < 0)
		return -1;

	argc -= retval;
	argv += retval;

	parse_args(argc, argv);

	/* Rx queue. */
	dpdk_init_lcore_rx_queues();
	/* initialise mbuf pools */
	dpdk_init_mbuf_pools();
	dpdk_init_ports();
	
	return 0;
}

