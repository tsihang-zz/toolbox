#include "oryx.h"
#include "et1500.h"
#include "dpdk.h"

/* These args appear by themselves */
#define foreach_eal_double_hyphen_predicate_arg \
_(no-shconf)                                    \
_(no-hpet)                                      \
_(no-huge)                                      \
_(vmware-tsc-map)

#define foreach_eal_single_hyphen_mandatory_arg \
_(coremask, c)                                  \
_(nchannels, n)                                 \

#define foreach_eal_single_hyphen_arg           \
_(blacklist, b)                                 \
_(mem-alloc-request, m)                         \
_(force-ranks, r)

/* These args are preceeded by "--" and followed by a single string */
#define foreach_eal_double_hyphen_arg           \
_(huge-dir)                                     \
_(proc-type)                                    \
_(file-prefix)                                  \
_(vdev)

char *eal_init_argv[1024] = {0};
int eal_init_args = 0;
int eal_args_offset = 0;
dpdk_main_t dpdk_main;
dpdk_config_main_t dpdk_config_main;
vlib_pci_main_t pci_main;

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
	SCLogDebug("read hugepages %s, free_hugepages = %d", p, r);
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

static int
foreach_directory_file (char *dir_name,
			int (*f) (void *arg, char * path_name,
			char * file_name), void *arg,
			int scan_dirs)
{
	DIR *d;
	struct dirent *e;
	int error = 0;
	char s[1024], t[1024];

  d = opendir (dir_name);
  if (!d)
	{
	  if (errno == ENOENT)
			return 0;
	  else {
	  	SCLogNotice ("open %s, %d", dir_name, errno);
		return 0;
	  }
	}

  while (1)
	{
	  e = readdir (d);
	  if (!e)
	break;
	  if (scan_dirs)
	{
	  if (e->d_type == DT_DIR
		  && (!strcmp (e->d_name, ".") || !strcmp (e->d_name, "..")))
		continue;
	}
	  else
	{
	  if (e->d_type == DT_DIR)
		continue;
	}

	  sprintf (s, "%s/%s", dir_name, e->d_name);
	  sprintf (t, "%s", e->d_name);
	
	  error = f (arg, s, t);
	  if (error)
		break;
	}

  closedir (d);

  return error;
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
	

	umount (ET1500_DPDK_DEFAULT_HUGE_DIR);
	
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
	
	SCLogNotice("free-hugepages availiable:");
	SCLogNotice(" @ 1024MB, %2d", pages_avail_1g);
	SCLogNotice(" @ -512MB, %2d", pages_avail_512m);
	SCLogNotice(" @ ---2MB, %2d", pages_avail_2m);
	
	rv = mkdir (ET1500_DPDK_DEFAULT_RUN_DIR, 0755);
	if (rv && errno != EEXIST)
		goto done;
	
	rv = mkdir (ET1500_DPDK_DEFAULT_HUGE_DIR, 0755);
	if (rv && errno != EEXIST)
		goto done;

	if (use_1g && !(less_than_1g && use_512m && use_2m)) {
		SCLogNotice ("Mounting ... 1 GB (%s)", ET1500_DPDK_DEFAULT_HUGE_DIR);
		rv = mount ("none", ET1500_DPDK_DEFAULT_HUGE_DIR, "hugetlbfs", 0, "pagesize=1G");
	} 
	else if (use_512m) {
		SCLogNotice ("Mounting ... 512 MB (%s)", ET1500_DPDK_DEFAULT_HUGE_DIR);
		rv = mount ("none", ET1500_DPDK_DEFAULT_HUGE_DIR, "hugetlbfs", 0, NULL);
	}
	else if (use_2m) {
		SCLogNotice ("Mounting ... 2 MB (%s)", ET1500_DPDK_DEFAULT_HUGE_DIR);
		rv = mount ("none", ET1500_DPDK_DEFAULT_HUGE_DIR, "hugetlbfs", 0, NULL);
	}
	else {
		SCLogNotice ("no enough free hugepages");
		goto done;
	}
	
	if (rv){
		SCLogNotice ("mount failed %d", errno);
		goto done;
	}

done:
	return;
#endif
}

static void
dpdk_eal_args_2string(dpdk_main_t *dm, char *format_buffer)
{
	dm = dm;
	int i;
	int l = 0;
	
	for (i = 0; i < eal_init_args; i ++) {
		l += sprintf (format_buffer + l, "%s ", eal_init_argv[i]);
	}
}

static void
dpdk_eal_args_format(const char *argv)
{
	char *t = kmalloc(strlen(argv) + 1, MPF_CLR, __oryx_unused_val__);
	sprintf (t, "%s%c", argv, 0);
	eal_init_argv[eal_init_args ++] = t;
}

/**
 * DPDK EAL init args: -c 0xf -n 4 --huge-dir /run/et1500/hugepages --file-prefix et1500 -w 0000:05:00.2 -w 0000:05:00.3 --master-lcore 0 --socket-mem 256
 */
static void
dpdk_format_eal_args (dpdk_main_t *dm)
{
	int i;
	char argv_buf[128] = {0};
	const char *socket_mem = "256";
	const char *file_prefix = "et1500";
	dpdk_config_main_t *conf = &dpdk_config_main;

	/** app it's self */
	memset(argv_buf, 0, 128);
	sprintf (argv_buf, "%s", "et1500");
	dpdk_eal_args_format(argv_buf);

	/** -c $COREMASK */
	memset(argv_buf, 0, 128);
	sprintf (argv_buf, "-c");
	dpdk_eal_args_format(argv_buf);
	memset(argv_buf, 0, 128);
	sprintf (argv_buf, "0x%x", conf->coremask);
	dpdk_eal_args_format(argv_buf);

	/** -n $NCHANNELS */
	memset(argv_buf, 0, 128);
	sprintf (argv_buf, "-n");
	dpdk_eal_args_format(argv_buf);
	memset(argv_buf, 0, 128);
	sprintf (argv_buf, "%d", conf->nchannels);
	dpdk_eal_args_format(argv_buf);

	/** --huge-dir $HUGEDIR */
	memset(argv_buf, 0, 128);
	sprintf (argv_buf, "--huge-dir");
	dpdk_eal_args_format(argv_buf);
	memset(argv_buf, 0, 128);
	sprintf (argv_buf, "%s", ET1500_DPDK_DEFAULT_HUGE_DIR);
	dpdk_eal_args_format(argv_buf);


	/** --file-prefix $FILEPREFIX */
	memset(argv_buf, 0, 128);
	sprintf (argv_buf, "--file-prefix");
	dpdk_eal_args_format(argv_buf);
	memset(argv_buf, 0, 128);
	sprintf (argv_buf, "%s", file_prefix);
	dpdk_eal_args_format(argv_buf);

	/** PCI device */
	for (i = 0; i < (int)conf->device_config_index_by_pci_addr; i ++) {
		dpdk_device_config_t *devconf = conf->dev_confs + i;
		devconf = conf->dev_confs + i;

		if(!devconf->enable)
			continue;
		
		/** -w $PCIADDR */
		memset(argv_buf, 0, 128);
		sprintf (argv_buf, "-w");
		dpdk_eal_args_format(argv_buf);
		memset(argv_buf, 0, 128);
		format_pci_addr(argv_buf, &devconf->pci_addr);
		dpdk_eal_args_format(argv_buf);
	}
	
	/** --master-lcore $MASTERLCORE */
	memset(argv_buf, 0, 128);
	sprintf (argv_buf, "--master-lcore");
	dpdk_eal_args_format(argv_buf);

	memset(argv_buf, 0, 128);
	dpdk_eal_args_format(argv_buf);


	/** --socket-mem $SOCKETMEM */
	memset(argv_buf, 0, 128);
	sprintf (argv_buf, "--socket-mem");
	dpdk_eal_args_format(argv_buf);

	memset(argv_buf, 0, 128);
	sprintf (argv_buf, "%s", socket_mem);
	dpdk_eal_args_format(argv_buf);

	char eal_args_format_buffer[1024] = {0};
	dpdk_eal_args_2string(dm, eal_args_format_buffer);
	SCLogNotice("eal args[%d]= %s", eal_init_args, eal_args_format_buffer);
#if 0
	/** never free eal args. */
	for (i = 0; i < eal_init_args; i ++) {
		kfree(eal_init_argv[i]);
	}
#endif
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
        SCLogNotice ("Unable to find dpdk config device using default value");
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
				if (ParseSizeStringU64(ethdev->val, &devconf->num_rx_desc) < 0) {
					exit(EXIT_FAILURE);
				}
			}

			else if (ethdev->name != NULL && strcmp(ethdev->name, "num-tx-desc") == 0) {
				if (ParseSizeStringU64(ethdev->val, &devconf->num_tx_desc) < 0) {
					exit(EXIT_FAILURE);
				}
			}
			
			else if (ethdev->name != NULL && strcmp(ethdev->name, "num-rx-queues") == 0) {
				if (ParseSizeStringU64(ethdev->val, &devconf->num_rx_queues) < 0) {
					exit(EXIT_FAILURE);
				}
			}
			
			else if (ethdev->name != NULL && strcmp(ethdev->name, "num-tx-queues") == 0) {
				if (ParseSizeStringU64(ethdev->val, &devconf->num_tx_queues) < 0) {
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
	SCLogNotice("===== Total %d/%d devices configured ======", ethdev_activity, conf->device_config_index_by_pci_addr);
	for (i = 0; i < conf->device_config_index_by_pci_addr; i ++) {
		devconf  = conf->dev_confs + i;
		format_pci_addr(pci_addr, &devconf->pci_addr);
		SCLogNotice ("%18s%16s", pci_addr, devconf->enable ? "enabled" : "disabled");
		SCLogNotice ("%18s%16lu", "rx-desc:",   devconf->num_rx_desc);
		SCLogNotice ("%18s%16lu", "tx-desc:",   devconf->num_tx_desc);
		SCLogNotice ("%18s%16lu", "rx-queues:", devconf->num_rx_queues);
		SCLogNotice ("%18s%16lu", "tx-queues:", devconf->num_tx_queues);
	}
}

static void
dpdk_config(vlib_main_t *vm)
{
	dpdk_main_t *dm = &dpdk_main;
	dpdk_config_main_t *conf = &dpdk_config_main;
	/* Check if we have memcap and hash_size defined at config */
	const char *conf_val;

	dm->vlib_main = vm;

	SCLogNotice ("Entering %s", __func__);

	/** set config values for dpdk-config, prealloc and hash_size */
	if (ConfGet("dpdk-config.uio-driver", (char **)&conf->uio_driver_name) == 1) {
		/** igb_uio uio_pci_generic (default), vfio-pci (aarch64 used) */
		if (strcmp (conf->uio_driver_name, "vfio-pci")) {
			SCLogError(SC_ERR_SIZE_PARSE, "uio-driver %s. But vfio-pci needed on this platform ",
	                   conf->uio_driver_name);
			exit(EXIT_FAILURE);
		}
	}
	if ((ConfGet("dpdk-config.num-mbufs", &conf_val)) == 1){
		if (ParseSizeStringU32(conf_val, &conf->num_mbufs) < 0) {
	        SCLogError(SC_ERR_SIZE_PARSE, "Error parsing ippair.memcap "
	                   "from conf file - %s.  Killing engine",
	                   conf_val);
	        exit(EXIT_FAILURE);
	    }
	}
    
	dpdk_device_config(conf);
	dpdk_mount_hugedir(conf);
	dpdk_bind_devices_to_uio(conf);
	dpdk_format_eal_args(dm);
  
  	/* Init runtime enviornment */
	if (rte_eal_init(vm->argc, vm->argv) < 0) {
		rte_exit(EXIT_FAILURE, "rte_eal_init(): Failed");
	}
	vm->ul_flags |= VLIB_DPDK_EAL_INITIALIZED;
	
	/* Dump the physical memory layout prior to creating the mbuf_pool */
	fprintf (stdout, "DPDK physical memory layout:\n");
	rte_dump_physmem_layout (stdout);

	/* create the mbuf pool */
	dm->pktmbuf_pools = rte_pktmbuf_pool_create((const char *)"et1500_mbuf_pool", /* pool name */
									conf->num_mbufs,	/* number of mbufs */
									conf->cache_size, 	/* cache size */
									conf->priv_size, 	/* priv size */
									conf->data_room_size, /* dataroom size */
									rte_socket_id());	/* cpu socket */
	if (dm->pktmbuf_pools == NULL) {
		rte_exit(EXIT_FAILURE, "Cannot init mbuf pool\n");
	}
	vm->ul_flags |= VLIB_DPDK_MEMPOOL_INITIALIZED;
	
}

void dpdk_init (vlib_main_t * vm)
{
	dpdk_main_t *dm = &dpdk_main;
	
	dm->conf = &dpdk_config_main;
	dm->conf->nchannels = 4;
	dm->conf->coremask = 0xf;
	dm->conf->num_mbufs = dm->conf->num_mbufs ? dm->conf->num_mbufs : ET1500_DPDK_DEFAULT_NB_MBUF;
	dm->conf->dev_confs = kmalloc(sizeof(dpdk_device_config_t) * PANEL_N_PORTS, MPF_CLR, __oryx_unused_val__);
	dm->conf->uio_driver_name = (u8 *)"uio_pci_generic";
	dm->conf->cache_size = ET1500_DPDK_DEFAULT_CACHE_SIZE;
	dm->conf->data_room_size = ET1500_DPDK_DEFAULT_BUFFER_SIZE;
	dm->conf->priv_size = 0;//vm->extra_priv_size;
	dm->stat_poll_interval = ET1500_DPDK_STATS_POLL_INTERVAL;
	dm->link_state_poll_interval = ET1500_DPDK_LINK_POLL_INTERVAL;
		
	dpdk_config(vm);
}

