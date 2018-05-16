#include "oryx.h"
#include "dpdk.h"

char *eal_init_argv[1024] = {0};
int eal_init_args = 0;
int eal_args_offset = 0;

extern int
init_dpdk_env(dpdk_main_t *dm);

static int
vlib_sysfs_read (char *file_name, const char *fmt, ...) {
	char s[4096] = {0};
	int fd;
	ssize_t sz;

	fd = open (file_name, O_RDONLY);
	if (fd < 0)
		return -1;

	sz = read (fd, s, 4096);
	if (sz < 0) {
		close (fd);
		return -1;
	}

	va_list va;
	va_start (va, fmt);
	vsprintf (s, fmt, va);
	va_end (va);

	close(fd);

	return 0;
}


static int
vlib_sysfs_get_free_hugepages (unsigned int numa_node, int page_size) {
	struct stat sb;
	char p[1024] = {0};
	int r = -1;
	int l = 0;

	memset (p, 0, 1024);
	l += sprintf (p + l, "/sys/devices/system/node/node%u", numa_node);
	if (stat ((char *) p, &sb) == 0) {
		if (S_ISDIR (sb.st_mode) == 0)
			goto done;
	} 
	else if (numa_node == 0) {
		memset (p, 0, 1024);
		l = 0;

		l += sprintf (p + l, "/sys/kernel/mm");
		if (stat ((char *) p, &sb) < 0 || S_ISDIR (sb.st_mode) == 0)
			goto done;
    }
	else goto done;

	l += sprintf (p + l, "/hugepages/hugepages-%ukB/free_hugepages", page_size);
	vlib_sysfs_read ((char *) p, "%d", &r);

done:
	oryx_logd("read hugepages %s, free_hugepages = %d", p, r);
	return r;
}

static void
unformat_pci_addr(char *pci_addr_str, vlib_pci_addr_t *addr) {
	u32 x[4] = {0};

	sscanf(pci_addr_str, "%x:%x:%x.%x", &x[0], &x[1], &x[2], &x[3]);
	
	addr->domain = x[0];
	addr->bus = x[1];
	addr->slot = x[2];
	addr->function = x[3];
}

static void
format_pci_addr(char *pci_addr_str, vlib_pci_addr_t *addr){
	sprintf(pci_addr_str, "%04x:%02x:%02x.%x", addr->domain, addr->bus,
		 addr->slot, addr->function);
}

static void
dpdk_bind_devices_to_uio (dpdk_config_main_t * conf)
{
	char dpdk_devbind_cmd[1024] = {0};
	int i;
	char addr[32] = {0};
	
	for (i = 0; i < (int)conf->device_config_index_by_pci_addr; i ++) {
		dpdk_device_config_t *devconf;
		devconf = conf->dev_confs + i;

		/** skip unused dev. */
		if(!devconf->enable)
			continue;

		format_pci_addr(addr, &devconf->pci_addr);
		sprintf(dpdk_devbind_cmd, "dpdk-devbind.py --bind=%s %s", conf->uio_driver_name, addr);
		do_system(dpdk_devbind_cmd);
	}
}


static void
dpdk_mount_hugedir (dpdk_config_main_t *conf)
{
	conf = conf;
#if defined(HAVE_DPDK_SCRIPT_MOUNT)
	char dpdk_mount_hugedir_cmd[1024] = {0};
	sprintf(dpdk_mount_hugedir_cmd, "/bin/sh %s/%s", CONFIG_PATH, DPDK_SCRIPT_MOUNT);
	do_system (dpdk_mount_hugedir_cmd);
#else
	uword c = 0;
	u8 use_1g = 1;
	u8 use_512m = 1;
	u8 use_2m = 1;
	u8 less_than_1g = 1;
	int rv;
	int pages_avail, page_size, mem = 0;
	int pages_avail_1g, pages_avail_512m, pages_avail_2m;
	

	umount (DPDK_DEFAULT_HUGE_DIR);
	
	page_size = 1024;
	pages_avail_1g = pages_avail = vlib_sysfs_get_free_hugepages(c, page_size * 1024);
	if (pages_avail < 0 || page_size * pages_avail < mem)
	  use_1g = 0;
	
	page_size = 512;
	pages_avail_512m = pages_avail = vlib_sysfs_get_free_hugepages(c, page_size * 1024);
	if (pages_avail < 0 || page_size * pages_avail < mem)
	  use_512m = 0;

	page_size = 2;
	pages_avail_2m = pages_avail = vlib_sysfs_get_free_hugepages(c, page_size * 1024);
	if (pages_avail < 0 || page_size * pages_avail < mem)
	  use_2m = 0;
	
	oryx_logn("free-hugepages availiable:");
	oryx_logn(" @ 1024MB, %2d", pages_avail_1g);
	oryx_logn(" @ -512MB, %2d", pages_avail_512m);
	oryx_logn(" @ ---2MB, %2d", pages_avail_2m);
	
	rv = mkdir (DPDK_DEFAULT_RUN_DIR, 0755);
	if (rv && errno != EEXIST)
		goto done;
	
	rv = mkdir (DPDK_DEFAULT_HUGE_DIR, 0755);
	if (rv && errno != EEXIST)
		goto done;

	if (use_1g && !(less_than_1g && use_512m && use_2m)) {
		oryx_logn ("Mounting ... 1 GB (%s)", DPDK_DEFAULT_HUGE_DIR);
		rv = mount ("none", DPDK_DEFAULT_HUGE_DIR, "hugetlbfs", 0, "pagesize=1G");
	} 
	else if (use_512m) {
		oryx_logn ("Mounting ... 512 MB (%s)", DPDK_DEFAULT_HUGE_DIR);
		rv = mount ("none", DPDK_DEFAULT_HUGE_DIR, "hugetlbfs", 0, NULL);
	}
	else if (use_2m) {
		oryx_logn ("Mounting ... 2 MB (%s)", DPDK_DEFAULT_HUGE_DIR);
		rv = mount ("none", DPDK_DEFAULT_HUGE_DIR, "hugetlbfs", 0, NULL);
	}
	else {
		oryx_logn ("no enough free hugepages");
		goto done;
	}
	
	if (rv){
		oryx_logn ("mount failed %d", errno);
		goto done;
	}

done:
	return;
#endif
}

static void
dpdk_eal_args_2string(dpdk_main_t *dm, char *format_buffer) {
	dm = dm;
	int i;
	int l = 0;
	
	for (i = 0; i < eal_init_args; i ++) {
		l += sprintf (format_buffer + l, "%s ", eal_init_argv[i]);
	}
}

static void
dpdk_eal_args_format(const char *argv) {
	char *t = kmalloc(strlen(argv) + 1, MPF_CLR, __oryx_unused_val__);
	sprintf (t, "%s%c", argv, 0);
	eal_init_argv[eal_init_args ++] = t;
}

/**
 * DPDK simple EAL init args : $prgname -c 0x0c -n 2 -- -p 3 -q 1 CT0
 */
static void
dpdk_format_eal_args (dpdk_main_t *dm)
{
	int i;
	vlib_main_t *vm = dm->vm;
	dpdk_config_main_t *conf = dm->conf;
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
	dpdk_eal_args_2string(dm, eal_args_format_buffer);

	vm->argc = eal_init_args;	
	vm->argv = eal_init_argv;

	oryx_logn("eal args[%d]= %s", eal_init_args, eal_args_format_buffer);
}


static void
dpdk_device_config (dpdk_config_main_t *conf)
{
	/* Check if we have memcap and hash_size defined at config */
	dpdk_device_config_t *devconf;
    ConfNode *ethdev_pci;	
	/** config dpdk-device */
	/* Find initial node */
	ConfNode *dpdk_config_devices;
	int ethdev_activity = 0;
	
    dpdk_config_devices = ConfGetNode("dpdk-config-devices");
    if (dpdk_config_devices == NULL) {
        oryx_logn ("Unable to find dpdk config device using default value");
        return;
    }

	conf->device_config_index_by_pci_addr = 0;
	devconf = conf->dev_confs;
	
    TAILQ_FOREACH(ethdev_pci, &dpdk_config_devices->head, next) {
		if (ethdev_pci->val != NULL && strcmp(ethdev_pci->val, "ethdev-pci-id") == 0)
			devconf = conf->dev_confs + conf->device_config_index_by_pci_addr ++;

		ConfNode *ethdev;
		TAILQ_FOREACH(ethdev, &ethdev_pci->head, next) {
			if (ethdev->name != NULL && strcmp(ethdev->name, "ethdev-pci-id") == 0) {
				unformat_pci_addr(ethdev->val, &devconf->pci_addr);
				/** allocate a devconf and init default. */
				devconf->enable = 0;
				devconf->num_rx_desc = 1024;
				devconf->num_tx_desc = 1024;
				devconf->num_rx_queues = 1;
				devconf->num_tx_queues = 1;
				devconf->vlan_strip_offload = DPDK_DEVICE_VLAN_STRIP_OFF;
			}
			
			else if (ethdev->name != NULL && strcmp(ethdev->name, "enable") == 0) {
				if (!strcmp(ethdev->val, "yes")) {
					/** devconf already hold after ConfNodeLookupChildValue("ethdev-pci-id"). */
					devconf->enable = 1;
					ethdev_activity ++;
				}
			}

			else if (ethdev->name != NULL && strcmp(ethdev->name, "num-rx-desc") == 0) {
				if (oryx_str2_u64(ethdev->val, &devconf->num_rx_desc) < 0) {
					exit(EXIT_FAILURE);
				}
			}

			else if (ethdev->name != NULL && strcmp(ethdev->name, "num-tx-desc") == 0) {
				if (oryx_str2_u64(ethdev->val, &devconf->num_tx_desc) < 0) {
					exit(EXIT_FAILURE);
				}
			}
			
			else if (ethdev->name != NULL && strcmp(ethdev->name, "num-rx-queues") == 0) {
				if (oryx_str2_u64(ethdev->val, &devconf->num_rx_queues) < 0) {
					exit(EXIT_FAILURE);
				}
			}
			
			else if (ethdev->name != NULL && strcmp(ethdev->name, "num-tx-queues") == 0) {
				if (oryx_str2_u64(ethdev->val, &devconf->num_tx_queues) < 0) {
					exit(EXIT_FAILURE);
				}
			}

			else if (ethdev->name != NULL && strcmp(ethdev->name, "vlan-strip-offload") == 0) {
				if (!strcmp(ethdev->val, "on"))
					devconf->vlan_strip_offload = DPDK_DEVICE_VLAN_STRIP_ON;
				else
					devconf->vlan_strip_offload = DPDK_DEVICE_VLAN_STRIP_OFF;
			}
    	}
	}

	int i;
	char pci_addr[32] = {0};	/** debug */
	oryx_logn("===== Total %d/%d devices configured ======", ethdev_activity, conf->device_config_index_by_pci_addr);
	for (i = 0; i < conf->device_config_index_by_pci_addr; i ++) {
		devconf  = conf->dev_confs + i;
		format_pci_addr(pci_addr, &devconf->pci_addr);
		oryx_logn ("%18s%16s", pci_addr, devconf->enable ? "enabled" : "disabled");
		oryx_logn ("%18s%16lu", "rx-desc:",   devconf->num_rx_desc);
		oryx_logn ("%18s%16lu", "tx-desc:",   devconf->num_tx_desc);
		oryx_logn ("%18s%16lu", "rx-queues:", devconf->num_rx_queues);
		oryx_logn ("%18s%16lu", "tx-queues:", devconf->num_tx_queues);
	}
}


static void
dpdk_config(vlib_main_t *vm)
{
	dpdk_main_t *dm = &dpdk_main;
	dpdk_config_main_t *conf = dm->conf;
	dm->vm = vm;

	oryx_logn ("Entering %s", __func__);

	dpdk_device_config(conf);
	dpdk_mount_hugedir(conf);
	dpdk_bind_devices_to_uio(conf);
	dpdk_format_eal_args(dm);

	init_dpdk_env(dm);
}

void dpdk_init (vlib_main_t * vm)
{
	dpdk_main_t *dm = &dpdk_main;
	dpdk_config_main_t *conf = dm->conf;
	const char *conf_val;
	
	oryx_logn ("Entering %s", __func__);
	
	conf->num_mbufs = conf->num_mbufs ? conf->num_mbufs : DPDK_DEFAULT_NB_MBUF;
	conf->dev_confs = kmalloc(sizeof(dpdk_device_config_t) * MAX_PORTS,
									MPF_CLR, __oryx_unused_val__);
	conf->priv_size = vm->extra_priv_size;

	/** set config values for dpdk-config, prealloc and hash_size */
	if (ConfGet("dpdk-config.uio-driver", (const char **)&conf->uio_driver_name) == 1) {
		/** igb_uio uio_pci_generic (default), vfio-pci (aarch64 used) */
		if (strcmp (conf->uio_driver_name, "vfio-pci")) {
			oryx_loge(0,
				"uio-driver %s. But vfio-pci needed on this platform ",
	                   conf->uio_driver_name);
			exit(EXIT_FAILURE);
		}
	}
	if ((ConfGet("dpdk-config.num-mbufs", &conf_val)) == 1) {
		if (oryx_str2_u32(conf_val, &conf->num_mbufs) < 0) {
	        oryx_loge(0,
				"Error parsing ippair.memcap "
	                   "from conf file - %s.  Killing engine",
	                   conf_val);
	        exit(EXIT_FAILURE);
	    }

	}

	if (RTE_ALIGN(dm->conf->priv_size, RTE_MBUF_PRIV_ALIGN) != dm->conf->priv_size) {
		oryx_loge(0,
			"ERROR -->>>> mbuf priv_size=%u is not aligned\n",
			dm->conf->priv_size);
		exit (0);
	}
	
	oryx_logn ("================ DPDK INIT ARGS =================");
	oryx_logn ("%20s%15x", "core_mask", conf->coremask);
	oryx_logn ("%20s%15x", "port_mask", conf->portmask);
	oryx_logn ("%20s%15d", "num_mbufs", conf->num_mbufs);
	oryx_logn ("%20s%15d", "nxqslcore", conf->n_rx_q_per_lcore);
	oryx_logn ("%20s%15d", "cachesize", conf->cache_size);
	oryx_logn ("%20s%15d", "cacheline", RTE_CACHE_LINE_SIZE);
	oryx_logn ("%20s%15d", "priv_size", conf->priv_size);
	oryx_logn ("%20s%15d", "bhdrrouup", RTE_CACHE_LINE_ROUNDUP(conf->priv_size));
	oryx_logn ("%20s%15d", "datarmsiz", conf->data_room_size);
	oryx_logn ("%20s%15d", "socket_id", rte_socket_id());

	dpdk_config(vm);
}

