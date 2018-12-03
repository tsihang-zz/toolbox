#include "dpdk.h"
#include "oryx.h"
#include "config.h"

extern int ncapds_init
(
	IN int argc,
	IN char **argv
);

/* One buffer per client rx queue - dynamically allocate array */
static struct vlib_rx_buffer_t *vlib_rx_buf;

static int running = 1;

static __oryx_always_inline__
const char * ethport_mac(uint8_t port)
{
	static const char err_address[] = "00:00:00:00:00:00";
	static char addresses[RTE_MAX_ETHPORTS][sizeof(err_address)];

	if (unlikely(port >= RTE_MAX_ETHPORTS))
		return err_address;
	if (unlikely(addresses[port][0] == '\0')) {
		struct ether_addr mac;
		rte_eth_macaddr_get(port, &mac);
		snprintf(addresses[port], sizeof(addresses[port]),
				"%02x:%02x:%02x:%02x:%02x:%02x\n",
				mac.addr_bytes[0], mac.addr_bytes[1], mac.addr_bytes[2],
				mac.addr_bytes[3], mac.addr_bytes[4], mac.addr_bytes[5]);
	}
	return addresses[port];
}


/*
 * This function displays the recorded statistics for each port
 * and for each client. It uses ANSI terminal codes to clear
 * screen when called. It is called from a single non-master
 * thread in the server process, when the process is run with more
 * than one lcore enabled.
 */
static __oryx_always_inline__
void ncapd_display_stats (void)
{
	unsigned i, j;
	uint64_t	port_tx[RTE_MAX_ETHPORTS] = {0},
			port_tx_drop[RTE_MAX_ETHPORTS] = {0};
	uint64_t	ncapdc_tx[VLIB_NCAPDC_NUM] = {0},
			ncapdc_tx_drop[VLIB_NCAPDC_NUM] = {0},
			ncapdc_fw_drop[VLIB_NCAPDC_NUM] = {0},
			ncapdc_tx_arp[VLIB_NCAPDC_NUM] = {0};
	struct rte_eth_link link;
	struct rte_eth_stats stats;
	FILE *fd = NULL;
	const char filename[] = "/root/ncapd_runtime.log";
	static uint64_t		rxp[RTE_MAX_ETHPORTS] = {0},
				rxbp[RTE_MAX_ETHPORTS] = {0},
				rx[RTE_MAX_ETHPORTS] = {0},
				rxb[RTE_MAX_ETHPORTS] = {0};
	static struct timeval prev = {0, 0};
	struct timeval now = {0, 0};
	vlib_main_t *vm = vlib_main;

	/* to get TX stats, we need to do some summing calculations */
	
	memset(&link, 0, sizeof(link));
	fd = fopen(filename,"w");
	if (fd == NULL) {
		printf("line %d, Error opening %s: %s\n", __LINE__, filename, strerror(errno));
		fd = stdout;
	}

	for (i = 0; i < vm->nr_ports; i++){
		const volatile struct vlib_tx_stats *tx = &vm->tx[i];
		for (j = 0; j < vm->nr_ports; j++){
			/* assign to local variables here, save re-reading volatile vars */
			const uint64_t tx_val		= tx->tx[vm->id[j]];
			const uint64_t drop_val		= tx->tx_drop[vm->id[j]];
			const uint64_t fw_drop_val	= tx->fw_drop[vm->id[j]];	
			const uint64_t arp_val		= tx->tx_arp_resp[vm->id[j]] + tx->free_arp[vm->id[j]];			
			port_tx[j] += tx_val;
			port_tx_drop[j] += drop_val;
			ncapdc_tx[i] += tx_val;
			ncapdc_tx_drop[i] += drop_val;
			ncapdc_fw_drop[i] += fw_drop_val;
			ncapdc_tx_arp[i] += arp_val;
		}
	}

	/* Clear screen and move to top left */
	//printf("%s%s", clr, topLeft);

	fprintf(fd, "PORTS\n");
	fprintf(fd, "-----\n");
	for (i = 0; i < vm->nr_ports; i++)
		fprintf(fd, "Port %u: '%s'\t", (unsigned)vm->id[i],
				ethport_mac(vm->id[i]));

	fprintf(fd, "\n\n");
	for (i = 0; i < vm->nr_ports; i++){
		rte_eth_link_get_nowait(vm->id[i], &link);
		rte_eth_stats_get(vm->id[i], &stats);
		rx[i]  = vm->rx[i].rx;
		rxb[i] = vm->rx[i].rxb;	

		fprintf(fd, "\tLink status: %s\n", (link.link_status) ? ("up") : ("down"));
		fprintf(fd, "\tLink speed: %u Mbps\n", (unsigned) link.link_speed);
		fprintf(fd, "\tLink duplex: %s\n", (link.link_duplex == ETH_LINK_FULL_DUPLEX) ?
				("full-duplex") : ("half-duplex"));

		fprintf(fd, "\tRX-packets: %10"PRIu64"    RX-errors: %10"PRIu64"    RX-bytes: %10"PRIu64"\n", 
				stats.ipackets, stats.ierrors, stats.ibytes);
		if(stats.ierrors)
		{
			fprintf(fd , "\tRX-errors :\n\t mpc: %10"PRIu64" crcerrs: %10"PRIu64" rlec: %10"PRIu64" ruc: %10"PRIu64"\n\t roc: %10"PRIu64" rxerrc: %10"PRIu64" algnerrc: %10"PRIu64" cexterr: %10"PRIu64"\n",
				stats.mpc ,stats.crcerrs,stats.rlec,stats.ruc,
				stats.roc,stats.rxerrc,stats.algnerrc, stats.cexterr);			
		}
		fprintf(fd,	"\tTX-packets: %10"PRIu64"    TX-errors: %10"PRIu64"    TX-bytes: %10"PRIu64"\n", 
				stats.opackets, stats.oerrors, stats.obytes);
		
		
		fprintf(fd, "\t- rx: %9"PRIu64"\t"
				"tx: %9"PRIu64"\t"
				"\n",
				rx[i],
				port_tx[i]);
		fprintf(fd, "\t- rxb: %9"PRIu64"\t"
				"txb: %9"PRIu64"\n",
				rxb[i] << 3, (uint64_t) 0);

		gettimeofday(&now, NULL);
		fprintf(fd, "\t- rxpps: %9"PRIu64"\t"
				"rxbps: %9"PRIu64"\n",
				(rx[i] - rxp[i])/(now.tv_sec - prev.tv_sec),
				((rxb[i] - rxbp[i]) << 3)/(now.tv_sec - prev.tv_sec));
		
		rxp[i] = rx[i];
		rxbp[i] = rxb[i];
	}
	prev = now;

	fprintf(fd, "\nQUEUE\n");
	fprintf(fd, "-------\n");
	for (i = 0; i < vm->nr_ports; i++) {
		vlib_queue_t *q = &vlib_queues[i];
		const unsigned long long rx = q->stats.rx;
		const unsigned long long rx_drop = q->stats.rx_drop;
		fprintf(fd, " %2u - rx: %9llu, rx_drop: %9llu , fw_drop: %9"PRIu64"\n"
				"            tx: %9"PRIu64", tx_drop: %9"PRIu64" ,tx_arp: %9"PRIu64"\n",
				i, rx, rx_drop, ncapdc_fw_drop[i], ncapdc_tx[i], ncapdc_tx_drop[i], ncapdc_tx_arp[i]);
	}

finish:
	fprintf(fd, "\n");
	fflush(fd);
	fclose(fd);
	fd = NULL;
}


/*
 * Function to set all the client statistic values to zero.
 * Called at program startup.
 */
static void
ncapd_clear_stats (void)
{
	unsigned i , j;
	vlib_main_t *vm = vlib_main;
	vlib_queue_t *q;
	
	for (i = 0; i < vm->nr_ports; i++) {
		q = &vlib_queues[i];
		q->stats.rx = 0;
		q->stats.rx_drop = 0;
	
		struct vlib_tx_stats *t = &vm->tx[i];
		for (j = 0; j < RTE_MAX_ETHPORTS; j ++) {
			t->tx[j] = 0;
			t->txb[j] = 0;
			t->fw_drop[j] = 0;
			t->tx_drop[j] = 0;
			t->free_arp[j] = 0;
			t->tx_arp_resp[j] = 0;
		}
	}
}


/*
 * The function called from each non-master lcore used by the process.
 * The test_and_set function is used to randomly pick a single lcore on which
 * the code to display the statistics will run. Otherwise, the code just
 * repeatedly sleeps.
 */
static int
ncapd_sleep_lcore(void)
{
	/* Used to pick a display thread - static, so zero-initialised */
	static rte_atomic32_t display_stats;

	/* Only one core should display stats */
	if (rte_atomic32_test_and_set(&display_stats)) {
		const unsigned sleeptime = 1;
		printf("Core %u displaying statistics\n", rte_lcore_id());

		/* Longer initial pause so above printf is seen */
		sleep(sleeptime * 3);

		/* Loop forever: sleep always returns 0 or <= param */
		while (sleep(sleeptime) <= sleeptime) {
			if (!running)
				break;
			ncapd_display_stats ();
		}
	} else {
		const unsigned sleeptime = 100;
		printf("Putting core %u to sleep\n", rte_lcore_id());
		while (sleep(sleeptime) <= sleeptime) {
			if (!running)
				break;
			; /* loop doing nothing */
		}
	}
	return 0;
}

/*
 * send a burst of traffic to a client, assuming there are packets
 * available to be sent to this client
 */
static __oryx_always_inline__
void ncapd_flush_rx_queue(uint16_t c)
{
	int err,
		j;
	struct vlib_queue_t *q;
	vlib_main_t *vm = vlib_main;

	if (vlib_rx_buf[c].count == 0)
		return;

	q = &vlib_queues[c];

	err = rte_ring_enqueue_bulk (q->rx_q, (void **)vlib_rx_buf[c].buffer, vlib_rx_buf[c].count);
	if (err) {
		for (j = 0; j < vlib_rx_buf[c].count; j++)
			rte_pktmbuf_free(vlib_rx_buf[c].buffer[j]);
		q->stats.rx_drop += vlib_rx_buf[c].count;
	} else {
		q->stats.rx += vlib_rx_buf[c].count;
	}

	vlib_rx_buf[c].count = 0;
}

/*
 * marks a packet down to be sent to a particular client process
 */
static inline void
ncapd_enqueue_rx_packet(uint8_t c, struct rte_mbuf *buf)
{
	vlib_rx_buf[c].buffer[vlib_rx_buf[c].count++] = buf;
}

/*
 * This function takes a group of packets and routes them
 * individually to the client process. Very simply round-robins the packets
 * without checking any of the packet contents.
 */
static __oryx_always_inline__
void ncapd_process_packets (uint32_t port,
		struct rte_mbuf *pkts[], uint16_t nr_pkts)
{
	uint16_t i;
	vlib_main_t *vm = vlib_main;
	uint8_t c = port % vm->nr_ports;
	
	for (i = 0; i < nr_pkts; i++) {
		ncapd_enqueue_rx_packet (c, pkts[i]);
		vm->rx[port].rxb += rte_pktmbuf_data_len(pkts[i]);
	}

	ncapd_flush_rx_queue (c);
}


/*
 * Function called by the master lcore of the DPDK process.
 */
static int
ncapd_packet_forwarding (__attribute__((unused)) void *dummy)
{
	unsigned port_num = 0; /* indexes the port[] array */
	unsigned lcore_id;
	unsigned slave_core_index;
	unsigned slave_lcore_num;

	vlib_main_t *vm = vlib_main;
	
	lcore_id = rte_lcore_id();
	slave_core_index = rte_lcore_index(lcore_id) - 1;
	slave_lcore_num = rte_lcore_count() - 1;/*skip mastor core*/

	
	FOREVER {
		struct rte_mbuf *buf[VLIB_RX_BURST_SIZE];
		uint16_t nr_rx_pkts;

		if ((port_num % slave_lcore_num)  == slave_core_index) {
			uint16_t queue_id = 0;
			/* read a port */
			nr_rx_pkts = rte_eth_rx_burst(vm->id[port_num], queue_id, \
					buf, VLIB_RX_BURST_SIZE);
			vm->rx[port_num].rx += nr_rx_pkts;

			/* Now process the NIC packets read */
			if (likely(nr_rx_pkts > 0))
				ncapd_process_packets (port_num, buf, nr_rx_pkts);
		}
		
		/* move to next port */
		if (++ port_num >= vm->nr_ports)
			port_num = 0;
	}
	return 0;
}

static void sigint_handler(int sig) {
	fprintf (stdout, "signal %d ...\n", sig);
	running = 0;
}

int main(
	int __oryx_unused__ argc,
	char __oryx_unused__ ** argv)
{
	int err;

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
	vlib_main = (vlib_main_t *)shm.addr;
	
	err = ncapds_init (argc, argv);
	if (err) exit(0);
	
	vlib_rx_buf = calloc(vlib_main->nr_ports, sizeof(vlib_rx_buf[0]));
	if (unlikely(!vlib_rx_buf)) {
		rte_exit(EXIT_FAILURE,
			"calloc: %s\n", oryx_safe_strerror(errno));
	}
	
	ncapd_clear_stats();

	/* put all other cores to sleep bar master */
	rte_eal_mp_remote_launch (
		ncapd_packet_forwarding,
		NULL,
		SKIP_MASTER
	);
		
	ncapd_sleep_lcore();

	/* Should we destroy shared memory ? */	
	oryx_shm_destroy(&shm);
	
	fprintf(stdout, "exiting ... \n");

	return 0;
}

