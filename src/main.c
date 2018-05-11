
#include "oryx.h"

#if defined(HAVE_QUAGGA)
#include "zebra.h"
#include "memory.h"
#include "log.h"
#include "version.h"
#include "vector.h"
#include "vty.h"
#include "command.h"
#include "prefix.h"
#endif

#include "et1500.h"
#if defined(HAVE_DPDK)
#include "dpdk.h"
#endif
#include "dataplane.h"

vlib_main_t vlib_main;
static unsigned short int alternative_port = 12000;
char config_current[] = SYSCONFDIR "/usr/local/etc/current.conf";
char config_default[] = SYSCONFDIR "/usr/local/etc/default.conf";

struct thread_master *master;

#define max_dotfiles_per_line 5
char *dotfile_buff[max_dotfiles_per_line] = {NULL};
int dotfiles = 0;

static void
format_dot_file(char *f, char *sep, void *fp)
{	
	int i;
	char format_buf [1024] = {0};
	int s = 0;
	
	if (strstr(f, sep)) {
		i = dotfiles ++;
		if (i == max_dotfiles_per_line) goto flush;
		dotfile_buff[i] = kmalloc(64, MPF_CLR, __oryx_unused_val__);
		sprintf(dotfile_buff[i], "%s", f);
		return;
	}
	else return;

flush:
    {
		for (i = 0; i < dotfiles; i ++) {
			if(dotfile_buff[i]) {
				s += sprintf(format_buf + s, "%s ", dotfile_buff[i]);		
				kfree(dotfile_buff[i]);
			}
		}
		printf ("%s\n", format_buf);

		fprintf((oryx_file_t *)fp, "   %s\n", format_buf);
		dotfiles = 0;
	}
}

static void
format_dota_file(char*f, char *sep, void *fp)
{
	char *s, *d;
	char fs[32];
	
	s = f;
	d = fs;
	
	if (strstr(f, sep)) {
		s += 3;
		do {
            if ((*d++ = *s++) == 0)
                break;
        } while (*s != '.');
			
		*d = '\0';
		fprintf((oryx_file_t *)fp, "    %s", fs);
	}
}

static void 
format_files(char *path, void(*format)(char*f, char *s, void *fp), char *sep)
{
	oryx_dir_t *d;
	struct dirent *p;
	oryx_file_t *fp;

	fp = fopen ("./file", "a");

	d = opendir (path);
	if (d) {
		while(likely((p = readdir(d)) != NULL)) {
			/** skip . and .. */
			if(!strcmp(p->d_name, ".") || !strcmp(p->d_name, ".."))
				continue;
			format(p->d_name, sep, fp);
		}
	}
}

static TmEcode LogVersion(void)
{
#ifdef REVISION
    SCLogNotice("This is %s version %s (rev %s)", PROG_NAME, PROG_VER, xstr(REVISION));
#elif defined RELEASE
    SCLogNotice("This is %s version %s RELEASE", PROG_NAME, PROG_VER);
#else
    SCLogNotice("This is %s version %s", PROG_NAME, PROG_VER);
#endif
    return TM_ECODE_OK;
}

static __oryx_always_inline__ 
void tmr_default_handler(struct oryx_timer_t *tmr, int __oryx_unused__ argc, 
                char __oryx_unused__**argv)
{
    printf ("default %s-timer routine has occured on [%s, %u, %d]\n",
		tmr->ul_setting_flags & TMR_OPTIONS_ADVANCED ? "advanced" : "sig",
		tmr->sc_alias, tmr->tmr_id, tmr->ul_cyclical_times);
}

static int dp_main(__attribute__((unused)) void *ptr_data)
{
	while (1) {
		printf ("slave[%d]\n", rte_lcore_id());
		sleep (1);
	}
}

void register_port ()
{
	int portid = 0;
	int dpdk_ports;
	const int panel_port_off = ET1500_N_XE_PORTS;
	vlib_main_t *vm = &vlib_main;
	dpdk_main_t *dm = &dpdk_main;
	vlib_port_main_t *vp = &vlib_port_main;
	struct port_t *entry;

	if (vm->ul_flags & VLIB_DPDK_EAL_INITIALIZED) {

		if ((dpdk_ports = rte_eth_dev_count()) == 0) {
			rte_exit(EXIT_FAILURE, "No available NIC ports!\n");
		}
		
		if (dpdk_ports > MAX_PORTS) {
			SCLogNotice("Info: Using only %i of %i ports",
				dpdk_ports, MAX_PORTS
				);
			dpdk_ports = MAX_PORTS;
		}
		
		dm->master_lcore = rte_get_master_lcore();
		dm->n_lcores = rte_lcore_count();
		if (dm->n_lcores < 2) {
			rte_exit(EXIT_FAILURE, "No available slave core!\n");
		}

		for (portid = 0; portid < dpdk_ports; portid ++) {

			port_entry_new (&entry, portid, dpdk_port);
			if (!entry) {
				exit (0);
			}
			port_entry_setup(entry);
			port_table_entry_add (entry);
		}
		
		vp->ul_n_ports = dpdk_ports;
	}

	/** sw port */
	for (portid = panel_port_off;
		portid < (vp->ul_n_ports + ET1500_N_GE_PORTS); portid ++) {
			port_entry_new (&entry, portid, sw_port);
			if (!entry) {
				exit (0);
			}
			port_entry_setup(entry);
			port_table_entry_add (entry);
	}
}

int main (int argc, char **argv)
{
	uint32_t id_core;
	char *vty_addr = NULL;
	short vty_port = ZEBRA_VTY_PORT;
	struct thread thread;

	vlib_main.argc = argc;
	vlib_main.argv = argv;
	vlib_main.extra_priv_size = ET1500_BUFFER_HDR_SIZE;

	oryx_initialize();

	/* Initialize the configuration module. */
	ConfInit();
	if (ConfYamlLoadFile(CONFIG_PATH_YAML) == -1) {
		printf ("ConfYamlLoadFile error\n");
		return 0;
	}
	
	SCLogInitLogModule(NULL);
	ParseSizeInit();

	GlobalsInitPreConfig();
	SCLogLoadConfig(0, 0);

    /* Since our config is now loaded we can finish configurating the
     * logging module. */
    SCLogLoadConfig(0, 1);
	/** logging module. */
	LogVersion();
	UtilCpuPrintSummary();
	
#if defined(HAVE_STATS_COUNTERS)
	StatsInit();
	printf ("can you hear me ... ?");
#else
	printf ("no\n");
#endif

	DefragInit();
	FlowInitConfig(FLOW_QUIET);
	IPPairInitConfig(FLOW_QUIET);
	LogFilestoreInitConfig();

	/** Pre-RUN POST */
	RunModeInitializeOutputs();
		
#if defined(HAVE_STATS_COUNTERS)
	StatsSetupPostConfig();
#endif

	MpmTableSetup();

	/* master init. */
	master = thread_master_create();
	
	/* Library inits. */
	cmd_init(1);
	vty_init(master);
	memory_init();

	/* Make vty server socket. */
	if (alternative_port != 0) {
		vty_port = alternative_port;
	}

	printf ("Command Line Interface Initialization done (%d)!\n", vty_port);

	/* Create VTY socket */
	vty_serv_sock(vty_addr, vty_port, ZEBRA_VTYSH_PATH);

	/* Configuration file read*/
	vty_read_config(config_current, config_default);

	
	port_init(&vlib_main);
	appl_init();
	map_init();
	udp_init();

	dpdk_init(&vlib_main);
	//register_port();

	rte_eal_mp_remote_launch(dp_main, NULL, SKIP_MASTER);
	oryx_task_launch();
	FOREVER {
		printf ("master[%d]\n", rte_lcore_id());
		
		/* Fetch next active thread. */
		while (thread_fetch(master, &thread)) {
			thread_call(&thread);
#if defined(ENABLE_KILL_ALL_VTY)
			if (kill_all_vty)
				vty_kill_all();
#endif
		}
	}
	
	RTE_LCORE_FOREACH_SLAVE(id_core) {
		if (rte_eal_wait_lcore(id_core) < 0)
			return -1;
	}

	return 0;
}
