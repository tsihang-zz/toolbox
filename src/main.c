
#include "oryx.h"

#include "version.h"

#include "zebra.h"
#include "memory.h"
#include "log.h"
#include "version.h"
#include "vector.h"
#include "vty.h"
#include "command.h"
#include "prefix.h"

#include "dpdk.h"

#include "dp_decode.h"
#include "dp_main.h"

/** CLI decalre */
extern void appl_init(vlib_main_t *vm);
extern void port_init(vlib_main_t *vm);
extern void udp_init(vlib_main_t *vm);
extern void map_init(vlib_main_t *vm);

extern void common_cli(void);

/** How to define a priv size within a packet before rte_pktmbuf_pool_create. */
#define DPDK_BUFFER_HDR_SIZE  (RTE_CACHE_LINE_ROUNDUP(sizeof(struct Packet_)) - DPDK_BUFFER_PRE_DATA_SIZE)

static vlib_main_t vlib_main = {
	.prgname = "et1500",
	.extra_priv_size = DPDK_BUFFER_HDR_SIZE,
};

static unsigned short int alternative_port = 12000;
char config_current[] = SYSCONFDIR "/usr/local/etc/current.conf";
char config_default[] = SYSCONFDIR "/usr/local/etc/default.conf";

struct thread_master *master;

#if 0
#define max_dotfiles_per_line 5
char *dotfile_buff[max_dotfiles_per_line] = {NULL};
int dotfiles = 0;

void
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

void
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

void 
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
#endif

static TmEcode LogVersion(void)
{
#ifdef REVISION
    oryx_logn("This is %s version %s (rev %s)", PROG_NAME, PROG_VER, xstr(REVISION));
#elif defined RELEASE
    oryx_logn("This is %s version %s RELEASE", PROG_NAME, PROG_VER);
#else
    oryx_logn("This is %s version %s", PROG_NAME, PROG_VER);
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

static __oryx_always_inline__
void * run_cli (void __oryx_unused__*pv_par)
{
	char *vty_addr = NULL;
	short vty_port = ZEBRA_VTY_PORT;
	struct thread thread;

	/* Make vty server socket. */
	if (alternative_port != 0) {
		vty_port = alternative_port;
	}
	
	printf ("Command Line Interface Initialization done (%d)!\n", vty_port);
	
	/* Create VTY socket */
	vty_serv_sock(vty_addr, vty_port, ZEBRA_VTYSH_PATH);
	
	/* Configuration file read*/
	vty_read_config(config_current, config_default);
	
	FOREVER {
		/* Fetch next active thread. */
		while (thread_fetch(master, &thread))
			thread_call(&thread);
	}
	oryx_task_deregistry_id(pthread_self());
	return NULL;
}


static struct oryx_task_t cli_register =
{
	.module = THIS,
	.sc_alias = "CLI Task",
	.fn_handler = run_cli,
	.ul_lcore = INVALID_CORE,
	.ul_prio = KERNEL_SCHED,
	.argc = 0,
	.argv = NULL,
	.ul_flags = 0,	/** Can not be recyclable. */
};

#if 0
static void register_port ()
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
			oryx_logn("Info: Using only %i of %i ports",
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
extern void
notify_dp(int signum);
#endif

static void
sig_handler(int signum) {

	vlib_main_t *vm = &vlib_main;
	
	if (signum == SIGINT || signum == SIGTERM) {
		//notify_dp(signum);
		/** */
		vm->ul_flags |= VLIB_QUIT;
	}
}

int main (int argc, char **argv)
{
	uint32_t id_core;

	vlib_main.argc = argc;
	vlib_main.argv = argv;

	//signal(SIGINT, sig_handler);
	//signal(SIGTERM, sig_handler);

	oryx_initialize();

	LogVersion();

	if (ConfYamlLoadFile(CONFIG_PATH_YAML) == -1) {
		printf ("ConfYamlLoadFile error\n");
		return 0;
	}

	/* master init. */
	master = thread_master_create();
	/* Library inits. */
	cmd_init(1);
	vty_init(master);
	memory_init();

	//StatsInit();

	port_init(&vlib_main);
	udp_init(&vlib_main);
	appl_init(&vlib_main);
	map_init(&vlib_main);

	common_cli();
	
	dp_init(&vlib_main);


#if 0	
	register_port();
	dataplane_start();
	RTE_LCORE_FOREACH_SLAVE(id_core) {
		if (rte_eal_wait_lcore(id_core) < 0)
			return -1;
	}
#else
oryx_task_registry(&cli_register);
oryx_task_launch();
	FOREVER{
		;
	}
#endif
	return 0;
}
