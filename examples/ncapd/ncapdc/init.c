#include "dpdk.h"
#include "oryx.h"
#include "config.h"

vlib_queue_t	*vlib_queues;
vlib_main_t		*vlib_main;
const char *progname;

/**
 * Set up the DPDK rings which will be used to pass packets, via
 * pointers, between the multi-process server and ncapdc processes.
 * Each ncapdc needs one RX queue.
 */
static int
ncapdc_init_rings(void)
{
	int			i,
				socket_id;
	const char * q_name;
	vlib_main_t	*vm = vlib_main;
	vlib_queue_t *q;

	vlib_queues = rte_malloc("ncapdc details",
						sizeof(vlib_queue_t) * vm->nr_ports, 0);
	if (vlib_queues == NULL) {
		rte_exit(EXIT_FAILURE,
			"%s: Cannot allocate memory for ncapdc program details\n", progname);
	}

	for (i = 0; i < vm->nr_ports; i++) {
		q = &vlib_queues[i];
		
		/* Create an RX queue for each ncapdc */
		socket_id = rte_socket_id();
		q_name = ncapd_get_rx_queue_name(i);

		fprintf(stdout, "%s: Lookup RING %s ...\n", progname,
			q_name);

		q->rx_q = rte_ring_lookup(ncapd_get_rx_queue_name(i));
		if (q->rx_q == NULL)
			rte_exit(EXIT_FAILURE, "%s: Cannot get RX ring \"%s\" - is server process running?\n",
				progname, ncapd_get_rx_queue_name(i));
	}

	return 0;
}

/**
 * Main init function for the multi-process server app,
 * calls subfunctions to do each stage of the initialisation.
 */
__oryx_always_extern__
int ncapdc_init
(
	IN int argc,
	IN char **argv
)
{
	int		i,
			id,
			err,
			retval;
	
	vlib_main_t *vm = vlib_main;

	progname = argv[0];
	
	/* init EAL, parsing EAL args */
	retval = rte_eal_init(argc, argv);
	if (retval < 0) {
		rte_exit(EXIT_FAILURE,
			"Invalid EAL parameters\n");
	}
	argc -= retval;
	argv += retval;

	/* parse APP args */

	/* initialise the ncapdc queues/rings for inter-eu comms */
	ncapdc_init_rings();

	vm->ul_flags |= NCPAD_SERV_INIT_DONE;
	return 0;
}


