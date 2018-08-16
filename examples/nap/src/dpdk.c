#include "oryx.h"
#include "dpdk.h"
#include "common_private.h"
#include "iface_private.h"

#define MAX_LCORE_PARAMS 1024

struct lcore_conf lconf_ctx[RTE_MAX_LCORE];

struct rte_eth_conf dpdk_eth_default_conf = {
	.rxmode = {
		.mq_mode = ETH_MQ_RX_RSS,
		.max_rx_pkt_len = ETHER_MAX_LEN,
		.split_hdr_size = 0,
		.header_split	= 0, /**< Header Split disabled */
		.hw_ip_checksum = 1, /**< IP checksum offload enabled */
		.hw_vlan_filter = 0, /**< VLAN filtering disabled */
		.jumbo_frame	= 0, /**< Jumbo Frame Support disabled */
		.hw_strip_crc	= 1, /**< CRC stripped by hardware */
	},
	.rx_adv_conf = {
		.rss_conf = {
			.rss_key = NULL,
			.rss_hf = ETH_RSS_IP | ETH_RSS_TCP | ETH_RSS_UDP,
		},
	},
	.txmode = {
		.mq_mode = ETH_MQ_TX_NONE,
	},
};

		
static dpdk_config_main_t dpdk_config_main = {
	.nchannels = 2, 	/** */		
	.coremask = 0xF,	/** c1,c2,c3 lcores. */
	.portmask = 0x07,	/** 3 xe ports */
	.uio_driver_name = (char *)"vfio-pci",
	.num_mbufs =		DPDK_DEFAULT_NB_MBUF,
	.mempool_cache_size =	DPDK_DEFAULT_MEMPOOL_CACHE_SIZE,
	.mempool_data_room_size = DPDK_DEFAULT_BUFFER_SIZE, 
	.mempool_priv_size = 0,
	.num_rx_desc = RTE_RX_DESC_DEFAULT,
	.num_tx_desc = RTE_TX_DESC_DEFAULT,
	.ethdev_default_conf = &dpdk_eth_default_conf,
};

dpdk_main_t dpdk_main = {
	.conf = &dpdk_config_main,
	.stat_poll_interval = DPDK_STATS_POLL_INTERVAL,
	.link_state_poll_interval = DPDK_LINK_POLL_INTERVAL,
	.ext_private = NULL
};

struct lcore_conf lcore_conf[RTE_MAX_LCORE];

/* A tsc-based timer responsible for triggering statistics printout */
uint64_t timer_period = 10; /* default period is 10 seconds */

static void
dpdk_eal_args_format(vlib_main_t *vm, const char *argv)
{
	char *t = kmalloc(strlen(argv) + 1, MPF_CLR, __oryx_unused_val__);
	sprintf (t, "%s", argv);
	vm->argv[vm->argc ++] = t;
}

static void
dpdk_eal_args_2string(vlib_main_t *vm, char *format_buffer) {
	int i;
	int l = 0;
	
	for (i = 0; i < vm->argc; i ++) {
		fprintf (stdout, "argc[%d] : %s\n", i, vm->argv[i]);
		if(!strcmp(vm->argv[i], "--")) {
			l += sprintf (format_buffer + l, " %s ", vm->argv[i]);
		}
		else {
			l += sprintf (format_buffer + l, "%s", vm->argv[i]);
		}
	}
}

void dpdk_format_eal_args (vlib_main_t *vm)
{
	int i;
	dpdk_config_main_t *conf = dpdk_main.conf;
	char argv_buf[128] = {0};
	const char *socket_mem = "256";
	const char *prgname = "napd";
	
	/** ARGS: < APPNAME >  */
	memset(argv_buf, 0, 128);
	sprintf (argv_buf, "%s", prgname);
	dpdk_eal_args_format(vm, argv_buf);

	/** ARGS: < -c $COREMASK > */
	memset(argv_buf, 0, 128);
	sprintf (argv_buf, "-c");
	dpdk_eal_args_format(vm, argv_buf);
	memset(argv_buf, 0, 128);
	sprintf (argv_buf, "0x%x", conf->coremask);	/** [core2:p0] [core3:p1] */
	dpdk_eal_args_format(vm, argv_buf);

	/** ARGS: < -- > */
	memset(argv_buf, 0, 128);
	sprintf (argv_buf, "--");
	dpdk_eal_args_format(vm, argv_buf);

	/** ARGS: < -p $PORTMASK > */
	memset(argv_buf, 0, 128);
	sprintf (argv_buf, "-p");
	dpdk_eal_args_format(vm, argv_buf);
	memset(argv_buf, 0, 128);
	sprintf (argv_buf, "0x%x", conf->portmask);	/** Port0 Port1 */
	dpdk_eal_args_format(vm, argv_buf);

	/** ARGS: < -q $QUEUE_PER_LOCRE > */
	//memset(argv_buf, 0, 128);
	//sprintf (argv_buf, "--config=\"%s\"",
	//	"(0,0,1),(0,1,2),(0,2,3),(1,0,1),(1,1,2),(1,2,3),(2,0,1),(2,1,2),(2,2,3)");
	//dpdk_eal_args_format(vm, argv_buf);

	char eal_args_format_buffer[1024] = {0};
	dpdk_eal_args_2string(vm, eal_args_format_buffer);

	oryx_logn("eal args[%d]= %s", vm->argc, eal_args_format_buffer);
}

