#include "dpdk.h"
#include "oryx.h"
#include "config.h"

static int running = 1;

extern int ncapdc_init
(
	IN int argc,
	IN char **argv
);

static void sigint_handler(int sig) {
	fprintf (stdout, "signal %d ...\n", sig);
	running = 0;
}

/*
 * print a usage message
 */
static void
usage(void)
{
	printf (
		"%s [EAL options] --  -p <pcap_datadir> -t <win_time (> 1)> -D -P <pidfile> -i port_ip\n"
		" -p <pcap_datadir>: xxxxxx\n"
		" -t <win_time>: great than 1\n"
		" -D \n"
		" -P \n"
		" -i \n"
		, progname);
}

/*
 * This function performs routing of packets
 * Just sends each input packet out an output port based solely on the input
 * port it arrived on.
 */
static __oryx_always_inline__
void handle_packet(struct rte_mbuf *pkt, uint8_t port)
{
	vlib_main_t *vm = vlib_main;

	vm->rx0[port].rx += 1;
	vm->rx0[port].rxb += rte_pktmbuf_data_len(pkt);

}

static int
do_work(__attribute__((unused)) void *dummy)
{
	uint64_t	cur_time = 0,
				end_time = 0,
				hz = 0;
	int64_t diff = 0;
	int 	i,
			need_flush[RTE_MAX_ETHPORTS] = {0}; /* indicates whether we have unsent packets */
	
	unsigned lcore_id = rte_lcore_id();
	unsigned lcore_index = rte_lcore_index(lcore_id);
	unsigned lcore_num = rte_lcore_count();
	uint8_t port;
	void *pkts[VLIB_RX_BURST_SIZE] = {0};
	vlib_main_t *vm = vlib_main;
	vlib_queue_t *q;
	uint16_t rx_pkts;
	
	hz = rte_get_tsc_hz();

	FOREVER {
		cur_time = rte_get_tsc_cycles();
		for (rx_pkts = VLIB_RX_BURST_SIZE, port = 0;
							port < vm->nr_ports; port++) {
			if((port % lcore_num) != lcore_index) {
				continue;
			}
			if(unlikely(diff <= 0 || !running)) {
				/* calculate the "end of test" time */
				end_time = cur_time + (hz * 2);
			}

			q = &vlib_queues[port];
			
			/* try dequeuing max possible packets first, if that fails, get the
			 * most we can. Loop body should only execute once, maximum */			
			while (rx_pkts > 0 &&
					unlikely(rte_ring_dequeue_bulk(q->rx_q, pkts, rx_pkts) != 0))
				rx_pkts = (uint16_t)RTE_MIN(rte_ring_count(q->rx_q), VLIB_RX_BURST_SIZE);

			if (unlikely(rx_pkts == 0))
				continue;

			for (i = 0; i < rx_pkts; i++)
				handle_packet(pkts[i], port);
		}

		diff = end_time - cur_time;
	}
	
}

int main(
	int __oryx_unused__ argc,
	char __oryx_unused__ ** argv)
{
	int i,
		err,
		retval;
	vlib_main_t *vm;
	vlib_shm_t	shm = {
		.shmid = 0,
		.addr  = 0,
	};
	
	oryx_initialize();
	oryx_register_sighandler(SIGINT,	sigint_handler);
	oryx_register_sighandler(SIGTERM,	sigint_handler);

	err = oryx_shm_get(VLIB_SHM_BASE_KEY, sizeof(vlib_main_t), &shm);
	if (err) {
		fprintf(stdout,
			"Shared memory failure.\n");
		exit(0);
	}
	vm = vlib_main = (vlib_main_t *)shm.addr;

	ncapdc_init(argc, argv);

	fprintf(stdout, "%s: lauching ...\n", progname);
	
	rte_eal_mp_remote_launch(do_work, NULL, CALL_MASTER);
	
}


