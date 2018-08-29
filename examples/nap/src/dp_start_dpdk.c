#include "oryx.h"
#include "dp_decode.h"
#include "dp_flow.h"
#include "dp_decode_tcp.h"
#include "dp_decode_udp.h"
#include "dp_decode_sctp.h"
#include "dp_decode_gre.h"
#include "dp_decode_icmpv4.h"
#include "dp_decode_icmpv6.h"
#include "dp_decode_ipv4.h"
#include "dp_decode_ipv6.h"
#include "dp_decode_pppoe.h"
#include "dp_decode_mpls.h"
#include "dp_decode_arp.h"
#include "dp_decode_vlan.h"
#include "dp_decode_marvell_dsa.h"
#include "dp_decode_eth.h"

#include "iface_private.h"
#include "dpdk.h"

extern ThreadVars g_tv[];
extern DecodeThreadVars g_dtv[];
extern PacketQueue g_pq[];

/* main processing loop */
extern int main_loop (void *ptr_data);

static const char *enp5s0fx[] = {
	"enp5s0f1",
	"enp5s0f2",
	"enp5s0f3"
};

static int dp_dpdk_check_port(vlib_main_t *vm)
{
	uint8_t portid;
	struct iface_t *this;
	vlib_iface_main_t *pm = &vlib_iface_main;
	int n_ports_now = vec_count(pm->entry_vec);
	int nr_ports = rte_eth_dev_count();

	if(!(vm->ul_flags & VLIB_PORT_INITIALIZED)) {
		oryx_panic(-1, "Run port initialization first.");
	}
	
	/* register to vlib_iface_main. */
	for (portid = 0; portid < nr_ports; portid ++) {
		iface_lookup_id(pm, portid, &this);
		if(!this) {
			oryx_panic(-1, "no such ethdev.");
		}
		if(strcmp(this->sc_alias_fixed, enp5s0fx[portid])) {
			oryx_panic(-1, "no such ethdev named %s.", this->sc_alias_fixed);
		}
	}

	return 0;
}

void dp_end_dpdk(vlib_main_t *vm)
{
	dpdk_main_t *dm = &dpdk_main;
	u8 portid;
	
	for (portid = 0; portid < dm->n_ports; portid++) {
		if ((dm->conf->portmask & (1 << portid)) == 0)
			continue;
		fprintf (stdout, "Closing port %d...", portid);
		rte_eth_dev_stop(portid);
		rte_eth_dev_close(portid);
		fprintf (stdout, " Done\n");
	}
	fprintf (stdout, "Bye...\n");
}

void dp_init_dpdk(vlib_main_t *vm)
{
	dpdk_main_t *dm = &dpdk_main;

	dm->conf->mempool_priv_size = vm->extra_priv_size;	
	dp_dpdk_check_port(vm);
	dpdk_env_setup(vm);
}

void dp_start_dpdk(vlib_main_t *vm) {

	fprintf (stdout, "Master Lcore @ %d/%d\n", rte_get_master_lcore(),
		vm->nb_lcores);

	/* launch per-lcore init on every lcore */
	rte_eal_mp_remote_launch(main_loop, vm, CALL_MASTER);
}




