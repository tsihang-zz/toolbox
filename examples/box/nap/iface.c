#include "oryx.h"
#include "config.h"
#include "dpdk.h"
#include "iface.h"
#include "iface_enum.h"
#include "cli.h"

#ifndef DIM
/** Number of elements in the array. */
#define	DIM(a)	(sizeof (a) / sizeof ((a)[0]))
#endif

#define PRINT_SUMMARY	\
	vty_out (vty, "matched %d element(s), total %d element(s)%s", \
		atomic_read(nr_ifaces), (int)vec_count(pm->entry_vec), VTY_NEWLINE);
#define VTY_ERROR_PORT(prefix, alias)\
	vty_out (vty, "%s(Error)%s %s port \"%s\"%s", \
		draw_color(COLOR_RED), draw_color(COLOR_FIN), prefix, (const char *)alias, VTY_NEWLINE)

#define VTY_SUCCESS_PORT(prefix, v)\
	vty_out (vty, "%s(Success)%s %s port \"%s\"(%u)%s", \
		draw_color(COLOR_GREEN), draw_color(COLOR_FIN), prefix, iface_alias(v), v->ul_id, VTY_NEWLINE)

vlib_iface_main_t vlib_iface_main;
int link_trasition_detected = 0;

ATOMIC_DECL_AND_INIT(uint32_t, nr_ifaces);

static
void ht_iface_free
(
	IN const ht_value_t __oryx_unused__ v
)
{
	/** Never free here! */
}

static
ht_key_t ht_iface_hval 
(
	IN struct oryx_htable_t *ht,
	IN const ht_value_t v,
	IN uint32_t s
) 
{
     uint8_t *d = (uint8_t *)v;
     uint32_t i;
     uint32_t hv = 0;

     for (i = 0; i < s; i++) {
         if (i == 0)      hv += (((uint32_t)*d++));
         else if (i == 1) hv += (((uint32_t)*d++) * s);
         else             hv *= (((uint32_t)*d++) * i) + s + i;
     }

     hv *= s;
     hv %= ht->array_size;
     
     return hv;
}

static
int ht_iface_cmp
(
	IN const ht_value_t v1,
	IN uint32_t s1,
	IN const ht_value_t v2,
	IN uint32_t s2
)
{
	int xret = 0;

	if (!v1 || !v2 ||s1 != s2 ||
		memcmp(v1, v2, s2))
		xret = 1;	/** Compare failed. */

	return xret;
}

/** if a null iface specified, map_entry_output display all */
static
void iface_entry_out (struct iface_t *iface, struct vty *vty)
{
	BUG_ON(iface == NULL);
	
	vty_out (vty, "%s", VTY_NEWLINE);
	/** find this iface named 'alias'. */
	vty_out (vty, "%16s\"%s\"(%u)%s", "Port ", iface_alias(iface), iface->ul_id, VTY_NEWLINE);

	vty_out (vty, "	%16s%02X:%02X:%02X:%02X:%02X:%02X%s", 
		"Mac: ", 
		iface->eth_addr[0], iface->eth_addr[1], iface->eth_addr[2],
		iface->eth_addr[3], iface->eth_addr[4], iface->eth_addr[5], VTY_NEWLINE);

	if (iface->ul_flags & NETDEV_ADMIN_UP) {
		struct oryx_fmt_buff_t fmt = FMT_BUFF_INITIALIZATION;
		if (iface->type == ETH_XE)
			oryx_format(&fmt, "%d Mbps - %s", iface->link_speed, ethtool_duplex(iface->link_duplex));
		else
			oryx_format(&fmt, "%s Mbps - %s", ethtool_speed(iface->link_speed), ethtool_duplex(iface->link_duplex));
		vty_out (vty, " %16s%s, %s%s", 
				"State: ", (iface->ul_flags & NETDEV_ADMIN_UP) ? "Up" : "Down", FMT_DATA(fmt), VTY_NEWLINE);
		oryx_format_free(&fmt);
	} else {
		vty_out (vty, " %16s%s%s", 
				"State: ", (iface->ul_flags & NETDEV_ADMIN_UP) ? "Up" : "Down", VTY_NEWLINE);
	}
}

static
void iface_entry_stat_clear (struct iface_t *iface, struct vty *vty)
{
	int id = 0;
	int lcore;
	
	if (unlikely (!iface)) 
		return;

	for (lcore = 0; lcore < VLIB_MAX_LCORES; lcore ++) {
		oryx_counter_set(iface_perf(iface),
			iface->if_counter_ctx->lcore_counter_pkts[QUA_RX][lcore], 0);
		oryx_counter_set(iface_perf(iface),
			iface->if_counter_ctx->lcore_counter_bytes[QUA_RX][lcore], 0);

		oryx_counter_set(iface_perf(iface),
			iface->if_counter_ctx->lcore_counter_pkts[QUA_TX][lcore], 0);
		oryx_counter_set(iface_perf(iface),
			iface->if_counter_ctx->lcore_counter_bytes[QUA_TX][lcore], 0);
	}

}

/** if a null iface specified, map_entry_output display all */
static 
void iface_entry_stat_out (struct iface_t *iface, struct vty *vty, bool clear)
{
	int lcore;
	uint64_t nb_rx_pkts;
	uint64_t nb_rx_bytes;
	uint64_t nb_tx_pkts;
	uint64_t nb_tx_bytes;

	char format[256] = {0};
	
	if (unlikely (!iface)) 
		return;

	nb_rx_bytes = nb_rx_pkts = nb_tx_bytes = nb_tx_pkts = 0;
	
	for (lcore = 0; lcore < VLIB_MAX_LCORES; lcore ++) {
		nb_rx_pkts += oryx_counter_get(iface_perf(iface),
			iface->if_counter_ctx->lcore_counter_pkts[QUA_RX][lcore]);
		nb_rx_bytes += oryx_counter_get(iface_perf(iface),
			iface->if_counter_ctx->lcore_counter_bytes[QUA_RX][lcore]);

		nb_tx_pkts += oryx_counter_get(iface_perf(iface),
			iface->if_counter_ctx->lcore_counter_pkts[QUA_TX][lcore]);
		nb_tx_bytes += oryx_counter_get(iface_perf(iface),
			iface->if_counter_ctx->lcore_counter_bytes[QUA_TX][lcore]);
	}

	if (clear)
		iface_entry_stat_clear(iface, vty);
	
	{
		/** find this iface named 'alias'. */
	
		sprintf (format, "%s(%02u)", iface_alias(iface), iface_id(iface));
		vty_out (vty, "%20s", format);

		sprintf (format, "%lu%s", nb_rx_pkts, nb_rx_pkts ? "/" : "");
		vty_out (vty, "%20s", format);

		sprintf (format, "%lu%s", nb_tx_pkts, nb_tx_pkts ? "/" : "");
		vty_out (vty, "%20s", format);

		vty_newline(vty);

		if (nb_rx_bytes) {
			sprintf (format, "%lu", nb_rx_bytes);
			vty_out (vty, "%40s", format);
		}
		
		if (nb_tx_bytes) {
			sprintf (format, "%lu", nb_tx_bytes);
			vty_out (vty, "%20s", format);
		}

		if (nb_rx_bytes || nb_tx_bytes)
			vty_newline(vty);
	} 
}

static
void iface_entry_config (struct iface_t *iface,
			struct vty *vty, const struct prefix_t *var)
{
	uint32_t ul_flags = iface->ul_flags;
	vlib_iface_main_t *pm = &vlib_iface_main;
	
	switch (var->cmd)
	{
		case INTERFACE_SET_ALIAS:
			if (strlen ((const char *)var->v) > sizeof (iface_alias(iface))) {
				VTY_ERROR_PORT("invalid alias", iface_alias(iface));
				break;
			}
			struct iface_t *exist;/** check same alias. */
			iface_lookup_alias(pm, (char *)var->v, &exist);
			if (exist) {
				VTY_ERROR_PORT("alias been named by a iface", iface_alias(exist));
				break;
			}	
			iface_rename(pm, iface, (const char *)var->v);
			break;
			
		case INTERFACE_SET_MTU:
			break;

		case INTERFACE_SET_LOOPBACK:
			ul_flags |= NETDEV_LOOPBACK;
			if (!strncmp ((const char *)var->v, "d", 1))
				ul_flags &= ~NETDEV_LOOPBACK;
			iface->ul_flags = ul_flags;
			break;
			
		case INTERFACE_CLEAR_STATS:
			break;
			
		default:
			break;
	}
}
			
static void iface_config_write(struct vty *vty)
{
	vty = vty;
	vty_out (vty, "! skip%s", VTY_NEWLINE);
}


static __oryx_always_inline__
void iface_stats_tmr_handler
(
	IN struct oryx_timer_t __oryx_unused__*tmr,
	IN int __oryx_unused__ argc,
	IN char __oryx_unused__**argv
)
{
	uint64_t nr_rx_pkts;
	uint64_t nr_rx_bytes;
	uint64_t nr_tx_pkts;
	uint64_t nr_tx_bytes;

	int lcore,
		each;
	char format[256] = {0};
	oryx_vector vec = vlib_iface_main.entry_vec;
	struct iface_t *iface;	

	FILE *fp = stdout;
	const char *box_iface_stats = "/tmp/box_iface_stats.log";
	char emptycmd[128] = "cat /dev/null > ";

	strcat(emptycmd, box_iface_stats);
	do_system(emptycmd);

	fp = fopen(box_iface_stats, "a+");
	if(!fp) {
		return;
	}

	fprintf(fp, "Trying to display %d elements ...%s", 
			vec_active(vec), "\n");

	fprintf(fp, "%20s%20s%20s%s", "Port", "Rx(P/B)", "Tx(P/B)", "\n");

	vec_foreach_element(vec, each, iface) {
		if (unlikely(iface == NULL))
			continue;

		{
			/* reset counter. */
			nr_rx_bytes = nr_rx_pkts = nr_tx_bytes = nr_tx_pkts = 0;
			
			for (lcore = 0; lcore < VLIB_MAX_LCORES; lcore ++) {

				nr_rx_pkts += oryx_counter_get(iface_perf(iface),
					iface->if_counter_ctx->lcore_counter_pkts[QUA_RX][lcore]);
				nr_rx_bytes += oryx_counter_get(iface_perf(iface),
					iface->if_counter_ctx->lcore_counter_bytes[QUA_RX][lcore]);
			
				nr_tx_pkts += oryx_counter_get(iface_perf(iface),
					iface->if_counter_ctx->lcore_counter_pkts[QUA_TX][lcore]);
				nr_tx_bytes += oryx_counter_get(iface_perf(iface),
					iface->if_counter_ctx->lcore_counter_bytes[QUA_TX][lcore]);
			}
			/** find this iface named 'alias'. */
		
			sprintf (format, "%s(%02u)", iface_alias(iface), iface_id(iface));
			fprintf(fp, "%20s", format);
	
			sprintf (format, "%lu%s", nr_rx_pkts, nr_rx_pkts ? "/" : "");
			fprintf(fp, "%20s", format);
	
			sprintf (format, "%lu%s", nr_tx_pkts, nr_tx_pkts ? "/" : "");
			fprintf(fp, "%20s", format);
	
			fprintf(fp, "%s", "\n");
	
			if (nr_rx_bytes) {
				sprintf (format, "%lu", nr_rx_bytes);
				fprintf(fp, "%40s", format);
			}
			
			if (nr_tx_bytes) {
				sprintf (format, "%lu", nr_tx_bytes);
				fprintf(fp, "%20s", format);
			}
	
			if (nr_rx_bytes || nr_tx_bytes)
				fprintf(fp, "%s", "\n");
		}
	}
	
	fflush(fp);
	fclose(fp);
}

static __oryx_always_inline__
void iface_activity_prob_tmr_handler
(
	IN struct oryx_timer_t __oryx_unused__*tmr,
	IN int __oryx_unused__ argc,
	IN char __oryx_unused__**argv
)
{
	vlib_iface_main_t *pm = &vlib_iface_main;	
	int each;
	struct iface_t *this;

	vec_foreach_element(pm->entry_vec, each, this) {
		if (likely(this != NULL)) {
			/** check linkstate. */
			if (this->if_poll_state)
				this->if_poll_state(this);

			/** poll a iface up if need. */
			if(this->ul_flags & NETDEV_POLL_UP) {
				/** up this interface right now. */
				if(this->if_poll_up) {
					this->if_poll_up(this);
				} else {
					oryx_loge(errno,
						"ethdev up driver is not registered, this iface will down forever.");
				}
			}
		}
	}
}

static void
register_ports(void)
{
	int i;
	struct iface_t *new;
	struct iface_t *this;
	vlib_iface_main_t *pm = &vlib_iface_main;
	int n_ports_now = vec_count(pm->entry_vec);

	/** SW<->CPU ports, only one. */
	for (i = 0; i < (int)DIM(iface_list); i ++) {

		this = &iface_list[i];
	#if 0	
		/** lan1-lan8 are not vfio-pci drv iface. */
		if (this->type == ETH_GE && (this->ul_flags & NETDEV_PANEL)) {
			if (!netdev_exist(this->sc_alias_fixed)) {
				continue;
			}
		}
	#endif
		
		iface_alloc(&new);
		if (!new) {
			oryx_panic(-1, 
				"Can not alloc memory for a iface");
		} else {
			memcpy(&new->sc_alias[0], this->sc_alias_fixed, strlen(this->sc_alias_fixed));
			new->sc_alias_fixed		= this->sc_alias_fixed;
			new->type				= this->type;
			new->if_poll_state		= this->if_poll_state;
			new->if_poll_up			= this->if_poll_up;
			new->ul_id				= n_ports_now + i;
			new->ul_flags			= this->ul_flags;
			if (iface_add(pm, new))
				oryx_panic(-1, 
					"registering interface %s ... error", iface_alias(new));
		}
	}
}

DEFUN(show_interfacce,
      show_interface_cmd,
      "show interface [WORD]",
      SHOW_STR SHOW_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR)
{
	vlib_iface_main_t *pm = &vlib_iface_main;
	
	vty_out (vty, "Trying to display %s%d%s elements ...%s", 
		draw_color(COLOR_RED), vec_active(pm->entry_vec), draw_color(COLOR_FIN), 
		VTY_NEWLINE);
	
	vty_out (vty, "%16s%s", "_______________________________________________", VTY_NEWLINE);

	if (argc == 0) {
		foreach_iface_func1_param1 (
			argv[0], iface_entry_out, vty);
	}
	else {
		split_foreach_iface_func1_param1 (
			argv[0], iface_entry_out, vty);
	}

	PRINT_SUMMARY;

    return CMD_SUCCESS;
}

DEFUN(show_interfacce_stats,
      show_interfacce_stats_cmd,
      "show interface stats [clear]",
      SHOW_STR SHOW_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR)
{
	vlib_iface_main_t *pm = &vlib_iface_main;
	bool clear = 0;
	
	vty_out(vty, "Trying to display %d elements ...%s", 
			vec_active(pm->entry_vec), VTY_NEWLINE);

	vty_out(vty, "%20s%20s%20s%s", "Port", "Rx(p/b)", "Tx(p/b)", VTY_NEWLINE);

	if (argc && !strncmp (argv[0], "c", 1))
		clear = 1;
	
	foreach_iface_func1_param2 (
		argv[0], iface_entry_stat_out, vty, clear);

	PRINT_SUMMARY;

    return CMD_SUCCESS;
}

DEFUN(interface_alias,
      interface_alias_cmd,
      "interface WORD alias WORD",
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
	  KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR)
{
	vlib_iface_main_t *pm = &vlib_iface_main;
	struct prefix_t var = {
		.cmd = INTERFACE_SET_ALIAS,
		.v = (void *)argv[1],
		.s = __oryx_unused_val__,
	};
	
	split_foreach_iface_func1_param2 (
		argv[0], iface_entry_config, vty, (const struct prefix_t *)&var);

	PRINT_SUMMARY;
	
	return CMD_SUCCESS;
}

DEFUN(interface_mtu,
      interface_mtu_cmd,
      "interface WORD mtu <64-65535>",
      KEEP_QUITE_STR
      KEEP_QUITE_CSTR
      KEEP_QUITE_STR
      KEEP_QUITE_CSTR
      KEEP_QUITE_STR
      KEEP_QUITE_CSTR
      KEEP_QUITE_STR
      KEEP_QUITE_CSTR)
{
	vlib_iface_main_t *pm = &vlib_iface_main;
	struct prefix_t var = {
		.cmd = INTERFACE_SET_MTU,
		.v = (void *)argv[1],
		.s = __oryx_unused_val__,
	};
	
	split_foreach_iface_func1_param2 (
		argv[0], iface_entry_config, vty, (const struct prefix_t *)&var);

	PRINT_SUMMARY;
	
	return CMD_SUCCESS;
}

DEFUN(interface_looback,
      interface_looback_cmd,
      "interface WORD loopback (enable|disable)",
      KEEP_QUITE_STR
      KEEP_QUITE_CSTR
      KEEP_QUITE_STR
      KEEP_QUITE_CSTR
      KEEP_QUITE_STR
      KEEP_QUITE_CSTR
      KEEP_QUITE_STR
      KEEP_QUITE_CSTR
      KEEP_QUITE_STR
      KEEP_QUITE_CSTR)
{
	vlib_iface_main_t *pm = &vlib_iface_main;
	struct prefix_t var = {
		.cmd = INTERFACE_SET_LOOPBACK,
		.v = (void *)argv[1],
		.s = __oryx_unused_val__,
	};
	
	split_foreach_iface_func1_param2 (
		argv[0], iface_entry_config, vty, (const struct prefix_t *)&var);

	PRINT_SUMMARY;
	
	return CMD_SUCCESS;
}

__oryx_always_extern__
void vlib_iface_init
(
	IN vlib_main_t *vm
)
{	
	vlib_iface_main_t *pm = &vlib_iface_main;
	
	pm->link_detect_tmr_interval = 3;
	pm->entry_vec	= vec_init (MAX_PORTS);
	pm->htable		= oryx_htable_init(DEFAULT_HASH_CHAIN_SIZE, 
							ht_iface_hval, ht_iface_cmp, ht_iface_free, 0);

	if (pm->htable == NULL || pm->entry_vec == NULL)
		oryx_panic(-1, 
			"vlib iface main init error!");

	install_element (CONFIG_NODE, &show_interface_cmd);
	install_element (CONFIG_NODE, &show_interfacce_stats_cmd);
	install_element (CONFIG_NODE, &interface_alias_cmd);
	install_element (CONFIG_NODE, &interface_mtu_cmd);
	install_element (CONFIG_NODE, &interface_looback_cmd);

	register_ports();

	pm->link_detect_tmr = oryx_tmr_create(1, "iface activity monitoring tmr", 
							(TMR_OPTIONS_PERIODIC | TMR_OPTIONS_ADVANCED),
							iface_activity_prob_tmr_handler,
							0, NULL, pm->link_detect_tmr_interval);
	if(likely(pm->link_detect_tmr != NULL))
		oryx_tmr_start(pm->link_detect_tmr);

	pm->healthy_tmr = oryx_tmr_create(1, "iface healthy monitoring tmr", 
							(TMR_OPTIONS_PERIODIC | TMR_OPTIONS_ADVANCED),
							iface_stats_tmr_handler,
							0, NULL, 3);
	if(likely(pm->healthy_tmr != NULL))
		oryx_tmr_start(pm->healthy_tmr);

	vm->ul_flags |= VLIB_PORT_INITIALIZED;

}

