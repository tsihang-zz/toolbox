#include "oryx.h"
#include "dpdk_classify.h"

#define IPv4_L3FWD_LPM_MAX_RULES         1024
#define IPv4_L3FWD_LPM_NUMBER_TBL8S (1 << 8)
#define IPv6_L3FWD_LPM_MAX_RULES         1024
#define IPv6_L3FWD_LPM_NUMBER_TBL8S (1 << 16)

struct rte_lpm *ipv4_l3fwd_lpm_lookup_struct[NB_SOCKETS];
struct rte_lpm6 *ipv6_l3fwd_lpm_lookup_struct[NB_SOCKETS];

/*
 * Initialize Longest Prefix Match (lpm) parameters.
 */
void classify_setup_lpm(const int socketid)
{
	struct rte_lpm6_config config;
	struct rte_lpm_config config_ipv4;
	unsigned i;
	int ret;
	char s[64];

	/* create the LPM table */
	config_ipv4.max_rules = IPv4_L3FWD_LPM_MAX_RULES;
	config_ipv4.number_tbl8s = IPv4_L3FWD_LPM_NUMBER_TBL8S;
	config_ipv4.flags = 0;
	snprintf(s, sizeof(s), "IPv4_L3FWD_LPM_%d", socketid);
	ipv4_l3fwd_lpm_lookup_struct[socketid] =
			rte_lpm_create(s, socketid, &config_ipv4);
	if (ipv4_l3fwd_lpm_lookup_struct[socketid] == NULL)
		rte_exit(EXIT_FAILURE,
			"Unable to create the l3fwd LPM table on socket %d\n",
			socketid);
	/* create the LPM6 table */
	snprintf(s, sizeof(s), "IPv6_L3FWD_LPM_%d", socketid);

	config.max_rules = IPv6_L3FWD_LPM_MAX_RULES;
	config.number_tbl8s = IPv6_L3FWD_LPM_NUMBER_TBL8S;
	config.flags = 0;
	ipv6_l3fwd_lpm_lookup_struct[socketid] = rte_lpm6_create(s, socketid,
				&config);
	if (ipv6_l3fwd_lpm_lookup_struct[socketid] == NULL)
		rte_exit(EXIT_FAILURE,
			"Unable to create the l3fwd LPM table on socket %d\n",
			socketid);
}


