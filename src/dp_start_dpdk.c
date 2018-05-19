#include "oryx.h"
#include "dp_decode.h"
#include "dpdk.h"

extern dp_private_t dp_private_main;
extern ThreadVars g_tv[];
extern DecodeThreadVars g_dtv[];
extern PacketQueue g_pq[];
extern bool force_quit;

extern void DecodeUpdateCounters(ThreadVars *tv,
                                const DecodeThreadVars *dtv, const Packet *p);

static dpdk_config_main_t dpdk_config_main = {
	.nchannels = 2,		/** */		
	.coremask = 0x0f,	/** 4 lcores. */
	.portmask = 0x07,	/** 3 xe ports */
	.n_rx_q_per_lcore = 1,
	.uio_driver_name = (char *)"vfio-pci",
	.num_mbufs =		DPDK_DEFAULT_NB_MBUF,
	.cache_size =		DPDK_DEFAULT_CACHE_SIZE,
	.data_room_size =	DPDK_DEFAULT_BUFFER_SIZE
};

static int
dp_dpdk_running_fn(__attribute__((unused)) void *ptr_data)
{
	dpdk_main_t *dm = &dpdk_main;
	u32 lcore = rte_lcore_id();
	
	ThreadVars *tv = &g_tv[lcore % RTE_MAX_LCORE];
	DecodeThreadVars *dtv = &g_dtv[lcore % RTE_MAX_LCORE];
	PacketQueue *pq = &g_pq[lcore % RTE_MAX_LCORE];

	while (!force_quit) {
		/** rx */
		oryx_logn ("dp running @ lcore [%d] ...", lcore);
		sleep(1);

	}

	oryx_logn ("dp_main[%d] exiting ...", lcore);
}

static char *eal_init_argv[1024] = {0};
static int eal_init_args = 0;
static int eal_args_offset = 0;

static void
dpdk_eal_args_format(const char *argv)
{
	char *t = kmalloc(strlen(argv) + 1, MPF_CLR, __oryx_unused_val__);
	sprintf (t, "%s%c", argv, 0);
	eal_init_argv[eal_init_args ++] = t;
}

static void
dpdk_eal_args_2string(char *format_buffer) {
	int i;
	int l = 0;
	
	for (i = 0; i < eal_init_args; i ++) {
		l += sprintf (format_buffer + l, "%s ", eal_init_argv[i]);
	}
}

/**
 * DPDK simple EAL init args : $prgname -c 0x0c -n 2 -- -p 3 -q 1 CT0
 */
static void
dpdk_format_eal_args (vlib_main_t *vm)
{
	int i;
	dpdk_config_main_t *conf = dpdk_main.conf;
	char argv_buf[128] = {0};
	const char *socket_mem = "256";
	const char *file_prefix = "et1500";
	const char *prgname = "et1500";
	
	/** ARGS: < APPNAME >  */
	memset(argv_buf, 0, 128);
	sprintf (argv_buf, "%s", prgname);
	dpdk_eal_args_format(argv_buf);

	/** ARGS: < -c $COREMASK > */
	memset(argv_buf, 0, 128);
	sprintf (argv_buf, "-c");
	dpdk_eal_args_format(argv_buf);
	memset(argv_buf, 0, 128);
	sprintf (argv_buf, "0x%x", conf->coremask);	/** [core2:p0] [core3:p1] */
	dpdk_eal_args_format(argv_buf);

	/** ARGS: < -n $NCHANNELS >  */
	memset(argv_buf, 0, 128);
	sprintf (argv_buf, "-n");
	dpdk_eal_args_format(argv_buf);
	memset(argv_buf, 0, 128);
	sprintf (argv_buf, "%d", conf->nchannels);
	dpdk_eal_args_format(argv_buf);

	/** ARGS: < -- > */
	memset(argv_buf, 0, 128);
	sprintf (argv_buf, "--");
	dpdk_eal_args_format(argv_buf);

	/** ARGS: < -p $PORTMASK > */
	memset(argv_buf, 0, 128);
	sprintf (argv_buf, "-p");
	dpdk_eal_args_format(argv_buf);
	memset(argv_buf, 0, 128);
	sprintf (argv_buf, "0x%x", conf->portmask);	/** Port0 Port1 */
	dpdk_eal_args_format(argv_buf);

	/** ARGS: < -q $QUEUE_PER_LOCRE > */
	memset(argv_buf, 0, 128);
	sprintf (argv_buf, "-q");
	dpdk_eal_args_format(argv_buf);
	memset(argv_buf, 0, 128);
	sprintf (argv_buf, "%d", conf->n_rx_q_per_lcore);	/** nx_queues_per_lcore */
	dpdk_eal_args_format(argv_buf);

	char eal_args_format_buffer[1024] = {0};
	dpdk_eal_args_2string(eal_args_format_buffer);

	vm->argc = eal_init_args;	
	vm->argv = eal_init_argv;

	oryx_logn("eal args[%d]= %s", eal_init_args, eal_args_format_buffer);
}

void dpdk_env_setup(vlib_main_t *vm)
{
	struct oryx_fmt_buff_t setup_env = FMT_BUFF_INITIALIZATION;
	dpdk_config_main_t *conf = dpdk_main.conf;
	
	/** run env-setup script. */
	oryx_format(&setup_env, "/bin/sh %s/%s", CONFIG_PATH, DPDK_SETUP_ENV_SH);
	oryx_logn("%s", FMT_DATA(setup_env));
	do_system (FMT_DATA(setup_env));

	/** prepare eal args for dpdk. */
	oryx_logn("%s", FMT_DATA(setup_env));
	oryx_format_free(&setup_env);
	dpdk_format_eal_args(vm);

	oryx_logn("================ DPDK INIT ARGS =================");
	oryx_logn("%20s%15x", "core_mask", conf->coremask);
	oryx_logn("%20s%15x", "port_mask", conf->portmask);
	oryx_logn("%20s%15d", "num_mbufs", conf->num_mbufs);
	oryx_logn("%20s%15d", "nxqslcore", conf->n_rx_q_per_lcore);
	oryx_logn("%20s%15d", "cachesize", conf->cache_size);
	oryx_logn("%20s%15d", "priv_size", conf->priv_size);
	oryx_logn("%20s%15d", "cacheline", RTE_CACHE_LINE_SIZE);
	//oryx_logn("%20s%15d", "packetsiz", sizeof(struct Packet_));
	//oryx_logn("%20s%15d", "bufferhdr", DPDK_BUFFER_HDR_SIZE);
	//oryx_logn("%20s%15d", "bhdrrouup", RTE_CACHE_LINE_ROUNDUP(DPDK_BUFFER_HDR_SIZE));
	oryx_logn("%20s%15d", "datarmsiz", conf->data_room_size);
	oryx_logn("%20s%15d", "socket_id", rte_socket_id());
	
	/** real initialization. */
	init_dpdk_env(vm);
}


dpdk_main_t dpdk_main = {
	.conf = &dpdk_config_main,
	.stat_poll_interval = DPDK_STATS_POLL_INTERVAL,
	.link_state_poll_interval = DPDK_LINK_POLL_INTERVAL,
	.ext_private = &dp_private_main
};

void dp_end_dpdk(struct vlib_main_t *vm)
{
	dpdk_main_t *dm = &dpdk_main;
	u8 portid;
	
	for (portid = 0; portid < dm->n_ports; portid++) {
		if ((dm->conf->portmask & (1 << portid)) == 0)
			continue;
		printf("Closing port %d...", portid);
		rte_eth_dev_stop(portid);
		rte_eth_dev_close(portid);
		printf(" Done\n");
	}
	printf("Bye...\n");
}

void dp_start_dpdk(struct vlib_main_t *vm) {
	vm->dp_running_fn = dp_dpdk_running_fn;
	dpdk_main_t *dm = &dpdk_main;
	printf ("Master Lcore %d\n", rte_get_master_lcore());
	/* launch per-lcore init on every lcore */
	rte_eal_mp_remote_launch(vm->dp_running_fn, NULL, SKIP_MASTER);
}




