
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

static __oryx_always_inline__ 
void tmr_default_handler(struct oryx_timer_t *tmr, int __oryx_unused__ argc, 
                char __oryx_unused__**argv)
{
    printf ("default %s-timer routine has occured on [%s, %u, %d]\n",
		tmr->ul_setting_flags & TMR_OPTIONS_ADVANCED ? "advanced" : "sig",
		tmr->sc_alias, tmr->tmr_id, tmr->ul_cyclical_times);
}

int main (int argc, char **argv)
{
	char *vty_addr = NULL;
	short vty_port = ZEBRA_VTY_PORT;
	struct thread thread;

	vlib_main.argc = argc;
	vlib_main.argv = argv;

#if 0	
	format_files("/home/tsihang/Source/et1500/deps/suricata/src", 
			format_dot_file, ".c");
#endif
	/* Initialize the configuration module. */
	ConfInit();
	if (ConfYamlLoadFile(CONFIG_PATH_YAML) == -1) {
		printf ("ConfYamlLoadFile error\n");
		return 0;
	}
	
	oryx_initialize();

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

	MpmTableSetup();
	port_init();
	appl_init();
	map_init();
	udp_init();

	dataplane_init(&vlib_main);
	
	oryx_task_launch();
	FOREVER {
			/* Fetch next active thread. */
			while (thread_fetch(master, &thread)) {
				thread_call(&thread);
#if defined(ENABLE_KILL_ALL_VTY)
				if (kill_all_vty)
					vty_kill_all();
#endif
			}
	}

	return 0;
}
