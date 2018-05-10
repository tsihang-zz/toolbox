#include "oryx.h"
#include "dpdk.h"

char *eal_init_argv[1024] = {0};
int eal_init_args = 0;
int eal_args_offset = 0;
dpdk_main_t dpdk_main;
vlib_pci_main_t pci_main;

void dpdk_init(vlib_main_t * vm)
{
	int rv;
	uint32_t id_core;
	uint32_t cnt_ports;
	/* Init runtime enviornment */
	rv = rte_eal_init(vm->argc, vm->argv);
	if (rv < 0)
		rte_exit(EXIT_FAILURE, "rte_eal_init(): Failed");

	cnt_ports = rte_eth_dev_count();
	printf("Number of NICs: %i\n", cnt_ports);
	if (cnt_ports == 0)
		rte_exit(EXIT_FAILURE, "No available NIC ports!\n");
	if (cnt_ports > MAX_PORTS) {
		printf("Info: Using only %i of %i ports\n",
			cnt_ports, MAX_PORTS
			);
		cnt_ports = MAX_PORTS;
	}

}

