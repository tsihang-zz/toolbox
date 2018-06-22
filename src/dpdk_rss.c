#include "oryx.h"
#include "dpdk_classify.h"

extern volatile bool force_quit;

#define MAX_TX_QUEUE_PER_PORT RTE_MAX_ETHPORTS
#define MAX_RX_QUEUE_PER_PORT 128

#define MAX_LCORE_PARAMS 1024

/* Static global variables used within this file. */
static uint16_t nb_rxd = RTE_RX_DESC_DEFAULT;
static uint16_t nb_txd = RTE_TX_DESC_DEFAULT;

/**< Ports set in promiscuous mode off by default. */
static int promiscuous_on = 1;

static int numa_on = 1; /**< NUMA is enabled by default. */
static int parse_ptype = 1; /**< Parse packet type using rx callback, and */
			/**< disabled by default */

/* Global variables. */

//volatile bool force_quit;

/* ethernet addresses of ports */
uint64_t dest_eth_addr[RTE_MAX_ETHPORTS];
struct ether_addr ports_eth_addr[RTE_MAX_ETHPORTS];


/* mask of enabled ports */
uint32_t enabled_port_mask;

#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 90 /* 9s (90 * 100ms) in total */

#define MAX_JUMBO_PKT_LEN  9600
#define MEMPOOL_CACHE_SIZE 256

static const char short_options[] =
	"p:"  /* portmask */
	"P"   /* promiscuous */
	"L"   /* enable long prefix match */
	"E"   /* enable exact match */
	;

#define CMD_LINE_OPT_CONFIG "config"
#define CMD_LINE_OPT_ETH_DEST "eth-dest"
#define CMD_LINE_OPT_NO_NUMA "no-numa"
#define CMD_LINE_OPT_IPv6 "ipv6"
#define CMD_LINE_OPT_ENABLE_JUMBO "enable-jumbo"
#define CMD_LINE_OPT_HASH_ENTRY_NUM "hash-entry-num"
#define CMD_LINE_OPT_PARSE_PTYPE "parse-ptype"
enum {
	/* long options mapped to a short option */

	/* first long only option value must be >= 256, so that we won't
	 * conflict with short options */
	CMD_LINE_OPT_MIN_NUM = 256,
	CMD_LINE_OPT_CONFIG_NUM,
	CMD_LINE_OPT_ETH_DEST_NUM,
	CMD_LINE_OPT_NO_NUMA_NUM,
	CMD_LINE_OPT_IPv6_NUM,
	CMD_LINE_OPT_ENABLE_JUMBO_NUM,
	CMD_LINE_OPT_HASH_ENTRY_NUM_NUM,
	CMD_LINE_OPT_PARSE_PTYPE_NUM,
};

static const struct option lgopts[] = {
	{CMD_LINE_OPT_CONFIG, 1, 0, CMD_LINE_OPT_CONFIG_NUM},
	{CMD_LINE_OPT_ETH_DEST, 1, 0, CMD_LINE_OPT_ETH_DEST_NUM},
	{CMD_LINE_OPT_NO_NUMA, 0, 0, CMD_LINE_OPT_NO_NUMA_NUM},
	{CMD_LINE_OPT_IPv6, 0, 0, CMD_LINE_OPT_IPv6_NUM},
	{CMD_LINE_OPT_ENABLE_JUMBO, 0, 0, CMD_LINE_OPT_ENABLE_JUMBO_NUM},
	{CMD_LINE_OPT_HASH_ENTRY_NUM, 1, 0, CMD_LINE_OPT_HASH_ENTRY_NUM_NUM},
	{CMD_LINE_OPT_PARSE_PTYPE, 0, 0, CMD_LINE_OPT_PARSE_PTYPE_NUM},
	{NULL, 0, 0, 0}
};

struct lcore_params {
	uint8_t port_id;
	uint8_t queue_id;
	uint8_t lcore_id;
} __rte_cache_aligned;

static struct lcore_params lcore_params_array[MAX_LCORE_PARAMS];
static struct lcore_params lcore_params_array_default[] = {
/* start lcore 1 */	
	{0, 0, 1},
	{0, 1, 1},
	{0, 2, 1},
/* start lcore 2 */	
	{1, 0, 2},
	{1, 1, 2},
	{1, 2, 2},
/* start lcore 3 */	
	{2, 0, 3},
	{2, 1, 3},
	{2, 2, 3},
};

static struct lcore_params * lcore_params = lcore_params_array_default;
static uint16_t nb_lcore_params = sizeof(lcore_params_array_default) /
				sizeof(lcore_params_array_default[0]);

static struct rte_mempool * pktmbuf_pool[NB_SOCKETS];


static int
check_lcore_params(void)
{
	uint8_t queue, lcore;
	uint16_t i;
	int socketid;

	for (i = 0; i < nb_lcore_params; ++i) {
		queue = lcore_params[i].queue_id;
		if (queue >= MAX_RX_QUEUE_PER_PORT) {
			oryx_loge(-1, "invalid queue number: %hhu\n", queue);
			return -1;
		}
		lcore = lcore_params[i].lcore_id;
		if (!rte_lcore_is_enabled(lcore)) {
			oryx_loge(-1, "error: lcore %hhu is not enabled in lcore mask\n", lcore);
			return -1;
		}
		if ((socketid = rte_lcore_to_socket_id(lcore) != 0) &&
			(numa_on == 0)) {
			oryx_loge(-1, "warning: lcore %hhu is on socket %d with numa off \n",
				lcore, socketid);
		}
	}
	return 0;
}

static int
check_port_config(const unsigned nb_ports)
{
	unsigned portid;
	uint16_t i;

	for (i = 0; i < nb_lcore_params; ++i) {
		portid = lcore_params[i].port_id;
		if ((enabled_port_mask & (1 << portid)) == 0) {
			printf("port %u is not enabled in port mask\n", portid);
			return -1;
		}
		if (portid >= nb_ports) {
			printf("port %u is not present on the board\n", portid);
			return -1;
		}
	}
	return 0;
}

static uint8_t
get_port_n_rx_queues(const uint8_t port)
{
	int queue = -1;
	uint16_t i;

	for (i = 0; i < nb_lcore_params; ++i) {
		if (lcore_params[i].port_id == port) {
			if (lcore_params[i].queue_id == queue + 1)
				queue = lcore_params[i].queue_id;
			else
				rte_exit(EXIT_FAILURE, "queue ids of the port %d must be"
						" in sequence and must start with 0\n",
						lcore_params[i].port_id);
		}
	}
	return (uint8_t)(++queue);
}

static int
init_lcore_rx_queues(void)
{
	uint16_t i, nb_rx_queue;
	uint8_t lcore;

	for (i = 0; i < nb_lcore_params; ++i) {
		lcore = lcore_params[i].lcore_id;
		nb_rx_queue = lcore_conf[lcore].n_rx_queue;
		if (nb_rx_queue >= MAX_RX_QUEUE_PER_LCORE) {
			printf("error: too many queues (%u) for lcore: %u\n",
				(unsigned)nb_rx_queue + 1, (unsigned)lcore);
			return -1;
		} else {
			lcore_conf[lcore].rx_queue_list[nb_rx_queue].port_id =
				lcore_params[i].port_id;
			lcore_conf[lcore].rx_queue_list[nb_rx_queue].queue_id =
				lcore_params[i].queue_id;
			lcore_conf[lcore].n_rx_queue++;
		}
	}
	return 0;
}

/* display usage */
static void
print_usage(const char *prgname)
{
	printf("%s [EAL options] --"
		" -p PORTMASK"
		" [-P]"
		" [-E]"
		" [-L]"
		" --config (port,queue,lcore)[,(port,queue,lcore)]"
		" [--eth-dest=X,MM:MM:MM:MM:MM:MM]"
		" [--enable-jumbo [--max-pkt-len PKTLEN]]"
		" [--no-numa]"
		" [--hash-entry-num]"
		" [--ipv6]"
		" [--parse-ptype]\n\n"

		"  -p PORTMASK: Hexadecimal bitmask of ports to configure\n"
		"  -P : Enable promiscuous mode\n"
		"  -E : Enable exact match\n"
		"  -L : Enable longest prefix match (default)\n"
		"  --config (port,queue,lcore): Rx queue configuration\n"
		"  --eth-dest=X,MM:MM:MM:MM:MM:MM: Ethernet destination for port X\n"
		"  --enable-jumbo: Enable jumbo frames\n"
		"  --max-pkt-len: Under the premise of enabling jumbo,\n"
		"                 maximum packet length in decimal (64-9600)\n"
		"  --no-numa: Disable numa awareness\n"
		"  --hash-entry-num: Specify the hash entry number in hexadecimal to be setup\n"
		"  --ipv6: Set if running ipv6 packets\n"
		"  --parse-ptype: Set to use software to analyze packet type\n\n",
		prgname);
}

static int
parse_max_pkt_len(const char *pktlen)
{
	char *end = NULL;
	unsigned long len;

	/* parse decimal string */
	len = strtoul(pktlen, &end, 10);
	if ((pktlen[0] == '\0') || (end == NULL) || (*end != '\0'))
		return -1;

	if (len == 0)
		return -1;

	return len;
}

static int
parse_portmask(const char *portmask)
{
	char *end = NULL;
	unsigned long pm;

	/* parse hexadecimal string */
	pm = strtoul(portmask, &end, 16);
	if ((portmask[0] == '\0') || (end == NULL) || (*end != '\0'))
		return -1;

	if (pm == 0)
		return -1;

	return pm;
}

static int
parse_hash_entry_number(const char *hash_entry_num)
{
	char *end = NULL;
	unsigned long hash_en;
	/* parse hexadecimal string */
	hash_en = strtoul(hash_entry_num, &end, 16);
	if ((hash_entry_num[0] == '\0') || (end == NULL) || (*end != '\0'))
		return -1;

	if (hash_en == 0)
		return -1;

	return hash_en;
}

static int
parse_config(const char *q_arg)
{
	char s[256];
	const char *p, *p0 = q_arg;
	char *end;
	enum fieldnames {
		FLD_PORT = 0,
		FLD_QUEUE,
		FLD_LCORE,
		_NUM_FLD
	};
	unsigned long int_fld[_NUM_FLD];
	char *str_fld[_NUM_FLD];
	int i;
	unsigned size;

	nb_lcore_params = 0;

	while ((p = strchr(p0,'(')) != NULL) {
		++p;
		if((p0 = strchr(p,')')) == NULL)
			return -1;

		size = p0 - p;
		if(size >= sizeof(s))
			return -1;

		snprintf(s, sizeof(s), "%.*s", size, p);
		if (rte_strsplit(s, sizeof(s), str_fld, _NUM_FLD, ',') != _NUM_FLD)
			return -1;
		for (i = 0; i < _NUM_FLD; i++){
			errno = 0;
			int_fld[i] = strtoul(str_fld[i], &end, 0);
			if (errno != 0 || end == str_fld[i] || int_fld[i] > 255)
				return -1;
		}
		if (nb_lcore_params >= MAX_LCORE_PARAMS) {
			printf("exceeded max number of lcore params: %hu\n",
				nb_lcore_params);
			return -1;
		}
		lcore_params_array[nb_lcore_params].port_id =
			(uint8_t)int_fld[FLD_PORT];
		lcore_params_array[nb_lcore_params].queue_id =
			(uint8_t)int_fld[FLD_QUEUE];
		lcore_params_array[nb_lcore_params].lcore_id =
			(uint8_t)int_fld[FLD_LCORE];
		++nb_lcore_params;
	}
	lcore_params = lcore_params_array;
	return 0;
}

/* Parse the argument given in the command line of the application */
static int
parse_args(int argc, char **argv)
{
	int opt, ret;
	char **argvopt;
	int option_index;
	char *prgname = argv[0];

	argvopt = argv;

	/* Error or normal output strings. */
	const char *str1 = "L3FWD: Invalid portmask";
	const char *str2 = "L3FWD: Promiscuous mode selected";
	const char *str3 = "L3FWD: Exact match selected";
	const char *str4 = "L3FWD: Longest-prefix match selected";
	const char *str5 = "L3FWD: Invalid config";
	const char *str6 = "L3FWD: NUMA is disabled";
	const char *str7 = "L3FWD: IPv6 is specified";
	const char *str8 =
		"L3FWD: Jumbo frame is enabled - disabling simple TX path";
	const char *str9 = "L3FWD: Invalid packet length";
	const char *str10 = "L3FWD: Set jumbo frame max packet len to ";
	const char *str11 = "L3FWD: Invalid hash entry number";
	const char *str12 =
		"L3FWD: LPM and EM are mutually exclusive, select only one";
	const char *str13 = "L3FWD: LPM or EM none selected, default LPM on";

	while ((opt = getopt_long(argc, argvopt, short_options,
				lgopts, &option_index)) != EOF) {

		switch (opt) {
		/* portmask */
		case 'p':
			enabled_port_mask = parse_portmask(optarg);
			if (enabled_port_mask == 0) {
				printf("%s\n", str1);
				print_usage(prgname);
				return -1;
			}
			break;

		case 'P':
			break;

		case 'E':
			break;

		case 'L':
			break;

		/* long options */
		case CMD_LINE_OPT_CONFIG_NUM:
			printf ("----------------------------------\n");
			ret = parse_config(optarg);
			if (ret) {
				printf("XXXXX   %s\n", str5);
				print_usage(prgname);
				return -1;
			}
			break;

		case CMD_LINE_OPT_ETH_DEST_NUM:
			break;

		case CMD_LINE_OPT_NO_NUMA_NUM:
			printf("%s\n", str6);
			numa_on = 0;
			break;

		case CMD_LINE_OPT_IPv6_NUM:
			printf("%sn", str7);
			break;

		case CMD_LINE_OPT_ENABLE_JUMBO_NUM: {
			struct option lenopts = {
				"max-pkt-len", required_argument, 0, 0
			};

			printf("%s\n", str8);
			dpdk_eth_default_conf.rxmode.jumbo_frame = 1;

			/*
			 * if no max-pkt-len set, use the default
			 * value ETHER_MAX_LEN.
			 */
			if (getopt_long(argc, argvopt, "",
					&lenopts, &option_index) == 0) {
				ret = parse_max_pkt_len(optarg);
				if ((ret < 64) ||
					(ret > MAX_JUMBO_PKT_LEN)) {
					printf("%s\n", str9);
					print_usage(prgname);
					return -1;
				}
				dpdk_eth_default_conf.rxmode.max_rx_pkt_len = ret;
			}
			printf("%s %u\n", str10,
				(unsigned int)dpdk_eth_default_conf.rxmode.max_rx_pkt_len);
			break;
		}

		case CMD_LINE_OPT_HASH_ENTRY_NUM_NUM:
			break;

		case CMD_LINE_OPT_PARSE_PTYPE_NUM:
			printf("soft parse-ptype is enabled\n");
			parse_ptype = 1;
			break;

		default:
			print_usage(prgname);
			return -1;
		}
	}
				
	if (optind >= 0)
		argv[optind-1] = prgname;

	ret = optind-1;
	optind = 1; /* reset getopt lib */
	return ret;
}

static void
print_ethaddr(const char *name, const struct ether_addr *eth_addr)
{
	char buf[ETHER_ADDR_FMT_SIZE];
	ether_format_addr(buf, ETHER_ADDR_FMT_SIZE, eth_addr);
	printf("%s%s", name, buf);
}

static int
init_mem()
{
	dpdk_main_t *dm = &dpdk_main;
	dpdk_config_main_t *conf = dm->conf;

	struct lcore_conf *qconf;
	int socketid;
	unsigned lcore_id;
	char s[64];

	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
		if (rte_lcore_is_enabled(lcore_id) == 0)
			continue;

		if (numa_on)
			socketid = rte_lcore_to_socket_id(lcore_id);
		else
			socketid = 0;

		if (socketid >= NB_SOCKETS) {
			rte_exit(EXIT_FAILURE,
				"Socket %d of lcore %u is out of range %d\n",
				socketid, lcore_id, NB_SOCKETS);
		}

		if (pktmbuf_pool[socketid] == NULL) {
			snprintf(s, sizeof(s), "mbuf_pool_%d", socketid);
			pktmbuf_pool[socketid] =
				rte_pktmbuf_pool_create(s, conf->num_mbufs,
					conf->mempool_cache_size, conf->mempool_priv_size,
					conf->mempool_data_room_size, socketid);
			if (pktmbuf_pool[socketid] == NULL)
				rte_exit(EXIT_FAILURE,
					"Cannot init mbuf pool on socket %d\n",
					socketid);
			else
				printf("Allocated mbuf pool on socket %d\n",
					socketid);
			classify_setup_em (socketid);
			classify_setup_acl(socketid);
		}
	}
	return 0;
}

/* Check the link status of all ports in up to 9s, and print them finally */
static void
check_all_ports_link_status(uint8_t port_num, uint32_t port_mask)
{
	uint8_t portid, count, all_ports_up, print_flag = 0;
	struct rte_eth_link link;

	printf("\nChecking link status");
	fflush(stdout);
	for (count = 0; count <= MAX_CHECK_TIME; count++) {
		if (force_quit)
			return;
		all_ports_up = 1;
		for (portid = 0; portid < port_num; portid++) {
			if (force_quit)
				return;
			if ((port_mask & (1 << portid)) == 0)
				continue;
			memset(&link, 0, sizeof(link));
			rte_eth_link_get_nowait(portid, &link);
			/* print link status if flag set */
			if (print_flag == 1) {
				if (link.link_status)
					printf("Port %d Link Up - speed %u "
						"Mbps - %s\n", (uint8_t)portid,
						(unsigned)link.link_speed,
				(link.link_duplex == ETH_LINK_FULL_DUPLEX) ?
					("full-duplex") : ("half-duplex\n"));
				else
					printf("Port %d Link Down\n",
						(uint8_t)portid);
				continue;
			}
			/* clear all_ports_up flag if any link down */
			if (link.link_status == ETH_LINK_DOWN) {
				all_ports_up = 0;
				break;
			}
		}
		/* after finally printing all link status, get out */
		if (print_flag == 1)
			break;

		if (all_ports_up == 0) {
			printf(".");
			fflush(stdout);
			rte_delay_ms(CHECK_INTERVAL);
		}

		/* set the print_flag if all ports up or timeout */
		if (all_ports_up == 1 || count == (MAX_CHECK_TIME - 1)) {
			print_flag = 1;
			printf("done\n");
		}
	}
}

static void
signal_handler(int signum)
{
	if (signum == SIGINT || signum == SIGTERM) {
		oryx_logw(0,"\n\nSignal %d received, preparing to exit...\n",
				signum);
		force_quit = true;
	}
}

#if defined(HAVE_DPDK_BUILT_IN_PARSER)
static __oryx_always_inline__
void parse_ptype0(struct rte_mbuf *m, struct lcore_conf *lconf)
{
	struct ether_hdr *eth_hdr;
	uint32_t packet_type = RTE_PTYPE_UNKNOWN;
	uint16_t ether_type;
	void *l3;
	int hdr_len;
	struct ipv4_hdr *ipv4h;
	struct ipv6_hdr *ipv6h;
	ThreadVars *tv = &g_tv[lconf->lcore_id];
	DecodeThreadVars *dtv = &g_dtv[lconf->lcore_id];

	oryx_counter_inc(&tv->perf_private_ctx0, dtv->counter_pkts);
	oryx_counter_add(&tv->perf_private_ctx0, dtv->counter_bytes, m->pkt_len);
	
	eth_hdr = rte_pktmbuf_mtod(m, struct ether_hdr *);

	oryx_counter_inc(&tv->perf_private_ctx0, dtv->counter_eth);

	ether_type = eth_hdr->ether_type;
	l3 = (uint8_t *)eth_hdr + sizeof(struct ether_hdr);
	if (ether_type == rte_cpu_to_be_16(ETHER_TYPE_IPv4)) {
		ipv4h = (struct ipv4_hdr *)l3;
		hdr_len = (ipv4h->version_ihl & IPv4_HDR_IHL_MASK) *
			  IPv4_IHL_MULTIPLIER;
		oryx_counter_inc(&tv->perf_private_ctx0, dtv->counter_ipv4);
		if (hdr_len == sizeof(struct ipv4_hdr)) {
			packet_type |= RTE_PTYPE_L3_IPv4;
			if (ipv4h->next_proto_id == IPPROTO_TCP){
				oryx_counter_inc(&tv->perf_private_ctx0, dtv->counter_tcp);
				packet_type |= RTE_PTYPE_L4_TCP;
			}
			else if (ipv4h->next_proto_id == IPPROTO_UDP){
				oryx_counter_inc(&tv->perf_private_ctx0, dtv->counter_udp);
				packet_type |= RTE_PTYPE_L4_UDP;
			}
			else if (ipv4h->next_proto_id == IPPROTO_SCTP){
				oryx_counter_inc(&tv->perf_private_ctx0, dtv->counter_sctp);
				packet_type |= RTE_PTYPE_L4_SCTP;
			}
		} else
			packet_type |= RTE_PTYPE_L3_IPv4_EXT;
	} else if (ether_type == rte_cpu_to_be_16(ETHER_TYPE_IPv6)) {
		ipv6h = (struct ipv6_hdr *)l3;
		if (ipv6h->proto == IPPROTO_TCP)
			packet_type |= RTE_PTYPE_L3_IPv6 | RTE_PTYPE_L4_TCP;
		else if (ipv6h->proto == IPPROTO_UDP)
			packet_type |= RTE_PTYPE_L3_IPv6 | RTE_PTYPE_L4_UDP;
		else if (ipv6h->proto == IPPROTO_SCTP)
			packet_type |= RTE_PTYPE_L3_IPv6 | RTE_PTYPE_L4_SCTP;
		else
			packet_type |= RTE_PTYPE_L3_IPv6_EXT_UNKNOWN;
	}

	m->packet_type = packet_type;
}

static void parse_ptype1(struct rte_mbuf *m)
{
	struct ether_hdr *eth_hdr;
	uint32_t packet_type = RTE_PTYPE_UNKNOWN;
	uint16_t ether_type;

	eth_hdr = rte_pktmbuf_mtod(m, struct ether_hdr *);
	ether_type = eth_hdr->ether_type;
	if (ether_type == rte_cpu_to_be_16(ETHER_TYPE_IPv4))
		packet_type |= RTE_PTYPE_L3_IPv4_EXT_UNKNOWN;
	else if (ether_type == rte_cpu_to_be_16(ETHER_TYPE_IPv6))
		packet_type |= RTE_PTYPE_L3_IPv6_EXT_UNKNOWN;

	m->packet_type = packet_type;
}

static int check_ptype0(int portid)
{
  int i, ret;
  int ptype_l3_ipv4 = 0;
  int ptype_l3_ipv6 = 0;
  int ptype_l3_ipv4_ext = 0;
  int ptype_l3_ipv6_ext = 0;
  int ptype_l4_tcp = 0;
  int ptype_l4_udp = 0;
  uint32_t ptype_mask = RTE_PTYPE_L3_MASK | RTE_PTYPE_L4_MASK;

  ret = rte_eth_dev_get_supported_ptypes(portid, ptype_mask, NULL, 0);
  if (ret <= 0)
	  return 0;

  uint32_t ptypes[ret];

  ret = rte_eth_dev_get_supported_ptypes(portid, ptype_mask, ptypes, ret);
  for (i = 0; i < ret; ++i) {
	  switch (ptypes[i]) {
	  case RTE_PTYPE_L3_IPv4:
	  	  ptype_l3_ipv4 = 1;
	  	  break;
	  case RTE_PTYPE_L3_IPv6:
	  	  ptype_l3_ipv6 = 1;
	  	  break;
	  case RTE_PTYPE_L3_IPv4_EXT:
		  ptype_l3_ipv4_ext = 1;
		  break;
	  case RTE_PTYPE_L3_IPv6_EXT:
		  ptype_l3_ipv6_ext = 1;
		  break;
	  case RTE_PTYPE_L4_TCP:
		  ptype_l4_tcp = 1;
		  break;
	  case RTE_PTYPE_L4_UDP:
		  ptype_l4_udp = 1;
		  break;
	  }
  }

  if(ptype_l3_ipv4 == 0)
  	 oryx_logw(0, "port %d cannot parse RTE_PTYPE_L3_IPv4\n", portid);
  if(ptype_l3_ipv6 == 0)
  	 oryx_logw(0, "port %d cannot parse RTE_PTYPE_L3_IPv6\n", portid);
  if (ptype_l3_ipv4_ext == 0)
	  oryx_logw(0, "port %d cannot parse RTE_PTYPE_L3_IPv4_EXT\n", portid);
  if (ptype_l3_ipv6_ext == 0)
	  oryx_logw(0, "port %d cannot parse RTE_PTYPE_L3_IPv6_EXT\n", portid);
  if (!ptype_l3_ipv4_ext || !ptype_l3_ipv6_ext)
	  return 0;
  if (ptype_l4_tcp == 0)
	  oryx_logw(0, "port %d cannot parse RTE_PTYPE_L4_TCP\n", portid);
  if (ptype_l4_udp == 0)
	  oryx_logw(0, "port %d cannot parse RTE_PTYPE_L4_UDP\n", portid);
  if (ptype_l4_tcp && ptype_l4_udp)
	  return 1;

  return 0;
}

static int check_ptype1(int portid)
{
  int i, ret;
  int ptype_l3_ipv4 = 0, ptype_l3_ipv6 = 0;
  uint32_t ptype_mask = RTE_PTYPE_L3_MASK;

  ret = rte_eth_dev_get_supported_ptypes(portid, ptype_mask, NULL, 0);
  if (ret <= 0)
	  return 0;

  uint32_t ptypes[ret];

  ret = rte_eth_dev_get_supported_ptypes(portid, ptype_mask, ptypes, ret);
  for (i = 0; i < ret; ++i) {
	  if (ptypes[i] & RTE_PTYPE_L3_IPv4)
		  ptype_l3_ipv4 = 1;
	  if (ptypes[i] & RTE_PTYPE_L3_IPv6)
		  ptype_l3_ipv6 = 1;
  }

  if (ptype_l3_ipv4 == 0)
	  printf("port %d cannot parse RTE_PTYPE_L3_IPv4\n", portid);

  if (ptype_l3_ipv6 == 0)
	  printf("port %d cannot parse RTE_PTYPE_L3_IPv6\n", portid);

  if (ptype_l3_ipv4 && ptype_l3_ipv6)
	  return 1;

  return 0;

}

static uint16_t cb_parse_ptype0(uint8_t port __rte_unused, uint16_t queue __rte_unused,
		  struct rte_mbuf *pkts[], uint16_t nb_pkts,
		  uint16_t max_pkts __rte_unused,
		  void *user_param __rte_unused)
{
	unsigned i;
	struct lcore_conf *lconf = (struct lcore_conf*)user_param;
	struct iface_t *iface;
	vlib_port_main_t *pm = &vlib_port_main;

	iface_lookup_id(pm, port, &iface);

#if defined(BUILD_DEBUG)
	BUG_ON(iface == NULL);
	if(nb_pkts) {
		oryx_logd("port=%d, queue=%d, lcore=%d", port, queue, lconf->lcore_id);
	}
#endif

	if (iface_support_marvell_dsa(iface)) {
		/** Marvell DSA frame decode. */
	} else {
		for (i = 0; i < nb_pkts; ++i) {
			rss_parse_ptype0(pkts[i], lconf);	
		}
	}
	
	return nb_pkts;
}

static uint16_t cb_parse_ptype1(uint8_t port __rte_unused, uint16_t queue __rte_unused,
		struct rte_mbuf *pkts[], uint16_t nb_pkts,
		uint16_t max_pkts __rte_unused,
		void *user_param __rte_unused)
{
  unsigned i;

  for (i = 0; i < nb_pkts; ++i)
	  rss_parse_ptype1(pkts[i]);

  return nb_pkts;
}
		
int prepare_ptype_parser(uint8_t portid, uint16_t queueid, void *usr_param)
{
	oryx_logn("registering ptype parser ...");
	
	if (parse_ptype) {
		oryx_logd("Port %d: softly parse packet type info\n", portid);
		if (rte_eth_add_rx_callback(portid, queueid,
					    cb_parse_ptype0,
					    usr_param))
			return 1;

		oryx_loge(-1,
				"Failed to add rx callback: port=%d\n", portid);
		return 0;
	}

	if (check_ptype0(portid))
		return 1;

	oryx_logw(0, "port %d cannot parse packet type, please add --%s\n",
	       portid, CMD_LINE_OPT_PARSE_PTYPE);

	return 0;
}
#endif

/**
./build/l3fwd -c e -- -p 0x7 --config="(0,0,1),(0,1,2),(0,2,3),(1,0,1),(1,1,2),(1,2,3),(2,0,1),(2,1,2),(2,2,3)"

*/
int
dpdk_env_setup(vlib_main_t *vm)
{
	struct lcore_conf *qconf;
	struct rte_eth_dev_info dev_info;
	struct rte_eth_txconf *txconf;
	int ret;
	unsigned nb_ports;
	uint16_t queueid;
	unsigned lcore_id;
	uint32_t n_tx_queue, nb_lcores;
	uint8_t portid, nb_rx_queue, queue, socketid;
	int argc = vm->argc;
	char **argv = vm->argv;

	dpdk_main_t *dm = &dpdk_main;
	dpdk_config_main_t *conf = dm->conf;
	
	enabled_port_mask = conf->portmask;
	
	/* init EAL */
	ret = rte_eal_init(argc, argv);
	if (ret < 0) {
		log_usage();
		rte_exit(EXIT_FAILURE, "Invalid EAL parameters\n");
	}
	argc -= ret;
	argv += ret;

	force_quit = false;
	
	/* convert to number of cycles */
	timer_period *= rte_get_timer_hz();

	nb_lcores = rte_lcore_count();
	nb_ports  = rte_eth_dev_count();
	if (nb_ports == 0) {
		log_usage();
		rte_exit(EXIT_FAILURE, "No Ethernet ports - bye\n");
	}
	
	vm->nb_lcores = nb_lcores;
	vm->nb_dpdk_ports = nb_ports;

	/* parse application arguments (after the EAL ones) */
	ret = parse_args(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid L3FWD parameters\n");

	conf->portmask = enabled_port_mask;
	oryx_logn("================ DPDK INIT ARGS =================");
	oryx_logn("%20s%15x", "ul_core_mask", conf->coremask);
	oryx_logn("%20s%15x", "ul_port_mask", conf->portmask);
	oryx_logn("%20s%15d", "ul_priv_size", vm->extra_priv_size);
	oryx_logn("%20s%15d", "n_pool_mbufs", conf->num_mbufs);
	oryx_logn("%20s%15d", "rte_cachelin", RTE_CACHE_LINE_SIZE);
	oryx_logn("%20s%15d", "mp_data_room", conf->mempool_data_room_size);
	oryx_logn("%20s%15d", "mp_cache_siz", conf->mempool_cache_size);
	oryx_logn("%20s%15d", "mp_priv_size", conf->mempool_priv_size);
	oryx_logn("%20s%15d", "n_rx_desc", (int)conf->num_rx_desc);
	oryx_logn("%20s%15d", "n_tx_desc", (int)conf->num_tx_desc);
	oryx_logn("%20s%15d", "socket_id", rte_socket_id());	
	if (check_lcore_params() < 0)
		rte_exit(EXIT_FAILURE, "check_lcore_params failed\n");

	ret = init_lcore_rx_queues();
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "init_lcore_rx_queues failed\n");


	if (check_port_config(nb_ports) < 0)
		rte_exit(EXIT_FAILURE, "check_port_config failed\n");

	printf ("Master Lcore %d, nb_lcores %d\n", rte_get_master_lcore(),
		nb_lcores);

	/* initialize all ports */
	for (portid = 0; portid < nb_ports; portid++) {
		/* skip ports that are not enabled */
		if ((enabled_port_mask & (1 << portid)) == 0) {
			printf("\nSkipping disabled port %d\n", portid);
			continue;
		}

		/* init port */
		printf("Initializing port %d ... ", portid);
		fflush(stdout);

		nb_rx_queue = get_port_n_rx_queues(portid);
		BUG_ON(nb_rx_queue == 0);
		n_tx_queue = nb_lcores;
		if (n_tx_queue > MAX_TX_QUEUE_PER_PORT)
			n_tx_queue = MAX_TX_QUEUE_PER_PORT;
		printf("Creating queues: nb_rxq=%d nb_txq=%u... ",
			nb_rx_queue, (unsigned)n_tx_queue );
		ret = rte_eth_dev_configure(portid, nb_rx_queue,
					(uint16_t)n_tx_queue, &dpdk_eth_default_conf);
		if (ret < 0)
			rte_exit(EXIT_FAILURE,
				"Cannot configure device: err=%d, port=%d\n",
				ret, portid);

		rte_eth_macaddr_get(portid, &ports_eth_addr[portid]);
		print_ethaddr(" Address:", &ports_eth_addr[portid]);
		printf("\n");

		/* init memory */
		ret = init_mem();
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "init_mem failed\n");

		/* init one TX queue per couple (lcore,port) */
		queueid = 0;
		for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
			if (rte_lcore_is_enabled(lcore_id) == 0)
				continue;

			if (numa_on)
				socketid =
				(uint8_t)rte_lcore_to_socket_id(lcore_id);
			else
				socketid = 0;

			printf("txq=%u,%d,%d ", lcore_id, queueid, socketid);
			fflush(stdout);

			rte_eth_dev_info_get(portid, &dev_info);
			txconf = &dev_info.default_txconf;
			if (dpdk_eth_default_conf.rxmode.jumbo_frame)
				txconf->txq_flags = 0;
			ret = rte_eth_tx_queue_setup(portid, queueid, nb_txd,
						     socketid, txconf);
			if (ret < 0)
				rte_exit(EXIT_FAILURE,
					"rte_eth_tx_queue_setup: err=%d, "
					"port=%d\n", ret, portid);

			qconf = &lcore_conf[lcore_id];
			qconf->lcore_id = lcore_id;
			qconf->tx_queue_id[portid] = queueid;
			queueid++;

			qconf->tx_port_id[qconf->n_tx_port] = portid;
			qconf->n_tx_port++;
		}
		printf("\n");
	}

	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
		if (rte_lcore_is_enabled(lcore_id) == 0)
			continue;
		qconf = &lcore_conf[lcore_id];
		printf("\nInitializing rx queues on lcore %u ... ", lcore_id );
		fflush(stdout);
		/* init RX queues */
		for(queue = 0; queue < qconf->n_rx_queue; ++queue) {
			portid = qconf->rx_queue_list[queue].port_id;
			queueid = qconf->rx_queue_list[queue].queue_id;

			if (numa_on)
				socketid =
				(uint8_t)rte_lcore_to_socket_id(lcore_id);
			else
				socketid = 0;

			printf("rxq=(%d,%d,%d) ", portid, queueid, lcore_id);
			fflush(stdout);

			ret = rte_eth_rx_queue_setup(portid, queueid, nb_rxd,
					socketid,
					NULL,
					pktmbuf_pool[socketid]);
			if (ret < 0)
				rte_exit(EXIT_FAILURE,
				"rte_eth_rx_queue_setup: err=%d, port=%d\n",
				ret, portid);
		}
	}

	printf("\n");

	/* start ports */
	for (portid = 0; portid < nb_ports; portid++) {
		if ((enabled_port_mask & (1 << portid)) == 0) {
			continue;
		}
		/* Start device */
		ret = rte_eth_dev_start(portid);
		if (ret < 0)
			rte_exit(EXIT_FAILURE,
				"rte_eth_dev_start: err=%d, port=%d\n",
				ret, portid);

		/*
		 * If enabled, put device in promiscuous mode.
		 * This allows IO forwarding mode to forward packets
		 * to itself through 2 cross-connected  ports of the
		 * target machine.
		 */
		if (promiscuous_on)
			rte_eth_promiscuous_enable(portid);
	}

	printf("\n");

	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
		if (rte_lcore_is_enabled(lcore_id) == 0)
			continue;
		qconf = &lcore_conf[lcore_id];
		for (queue = 0; queue < qconf->n_rx_queue; ++queue) {
			portid = qconf->rx_queue_list[queue].port_id;
			queueid = qconf->rx_queue_list[queue].queue_id;
		#if defined(HAVE_DPDK_BUILT_IN_PARSER)
			if (prepare_ptype_parser(portid, queueid, (void*)qconf) == 0)
				rte_exit(EXIT_FAILURE, "ptype check fails\n");
		#endif
		}
	}


	//check_all_ports_link_status((uint8_t)nb_ports, enabled_port_mask);

#if 0
	ret = 0;
	/* launch per-lcore init on every lcore */
	rte_eal_mp_remote_launch(l3fwd_lkp.main_loop, NULL, CALL_MASTER);
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		if (rte_eal_wait_lcore(lcore_id) < 0) {
			ret = -1;
			break;
		}
	}

	/* stop ports */
	for (portid = 0; portid < nb_ports; portid++) {
		if ((enabled_port_mask & (1 << portid)) == 0)
			continue;
		printf("Closing port %d...", portid);
		rte_eth_dev_stop(portid);
		rte_eth_dev_close(portid);
		printf(" Done\n");
	}
	printf("Bye...\n");
#endif
	return ret;
}

