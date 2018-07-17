#include "oryx.h"
#include "vty.h"
#include "command.h"
#include "prefix.h"

#include "common_private.h"
#include "map_private.h"
#include "util_iface.h"
#include "cli_iface.h"

#include "dpdk.h"

#ifndef DIM
/** Number of elements in the array. */
#define	DIM(a)	(sizeof (a) / sizeof ((a)[0]))
#endif

vlib_iface_main_t vlib_iface_main = {
	.lock = INIT_MUTEX_VAL,
};

#define VTY_ERROR_PORT(prefix, alias)\
	vty_out (vty, "%s(Error)%s %s port \"%s\"%s", \
		draw_color(COLOR_RED), draw_color(COLOR_FIN), prefix, (const char *)alias, VTY_NEWLINE)

#define VTY_SUCCESS_PORT(prefix, v)\
	vty_out (vty, "%s(Success)%s %s port \"%s\"(%u)%s", \
		draw_color(COLOR_GREEN), draw_color(COLOR_FIN), prefix, iface_alias(v), v->ul_id, VTY_NEWLINE)

atomic_t nb_ifaces = ATOMIC_INIT(0);

static
void ht_iface_free (const ht_value_t v)
{
	/** Never free here! */
}

static
ht_key_t ht_iface_hval (struct oryx_htable_t *ht,
		const ht_value_t v, uint32_t s) 
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
int ht_iface_cmp (const ht_value_t v1, 
		uint32_t s1,
		const ht_value_t v2,
		uint32_t s2)
{
	int xret = 0;

	if (!v1 || !v2 ||s1 != s2 ||
		memcmp(v1, v2, s2))
		xret = 1;	/** Compare failed. */

	return xret;
}

/** if a null iface specified, map_entry_output display all */
static __oryx_always_inline__
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

static __oryx_always_inline__
void iface_entry_stat_clear (struct iface_t *iface, struct vty *vty)
{
	int id = 0;
	int lcore;
	
	if (unlikely (!iface)) 
		return;

	for (lcore = 0; lcore < MAX_LCORES; lcore ++) {
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
static __oryx_always_inline__
void iface_entry_stat_out (struct iface_t *iface, struct vty *vty)
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
	
	for (lcore = 0; lcore < MAX_LCORES; lcore ++) {
		nb_rx_pkts += oryx_counter_get(iface_perf(iface),
			iface->if_counter_ctx->lcore_counter_pkts[QUA_RX][lcore]);
		nb_rx_bytes += oryx_counter_get(iface_perf(iface),
			iface->if_counter_ctx->lcore_counter_bytes[QUA_RX][lcore]);

		nb_tx_pkts += oryx_counter_get(iface_perf(iface),
			iface->if_counter_ctx->lcore_counter_pkts[QUA_TX][lcore]);
		nb_tx_bytes += oryx_counter_get(iface_perf(iface),
			iface->if_counter_ctx->lcore_counter_bytes[QUA_TX][lcore]);
	}

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

static __oryx_always_inline__
void iface_entry_config (struct iface_t *iface,
			struct vty *vty, const struct prefix_t *var)
{
	uint32_t ul_flags = iface->ul_flags;
	vlib_iface_main_t *pm = &vlib_iface_main;
	
	switch (var->cmd)
	{
		case INTERFACE_SET_ALIAS:
			if (strlen ((char *)var->v) > sizeof (iface_alias(iface))) {
				VTY_ERROR_PORT("invalid alias", iface_alias(iface));
				break;
			}
			struct iface_t *exist;/** check same alias. */
			iface_lookup_alias(pm, (const char *)var->v, &exist);
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
			if (!strncmp ((char *)var->v, "d", 1))
				ul_flags &= ~NETDEV_LOOPBACK;
			iface->ul_flags = ul_flags;
			break;
			
		case INTERFACE_CLEAR_STATS:
			break;
			
		default:
			break;
	}
}

#define PRINT_SUMMARY	\
	vty_out (vty, "matched %d element(s), total %d element(s)%s", \
		atomic_read(&nb_ifaces), (int)vec_count(pm->entry_vec), VTY_NEWLINE);

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
		foreach_iface_split_func1_param1 (
			argv[0], iface_entry_out, vty);
	}

	PRINT_SUMMARY;

    return CMD_SUCCESS;
}

DEFUN(show_interfacce_stats,
      show_interfacce_stats_cmd,
      "show interface stats [WORD]",
      SHOW_STR SHOW_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR)
{
	vlib_iface_main_t *pm = &vlib_iface_main;
	
	vty_out(vty, "Trying to display %d elements ...%s", 
			vec_active(pm->entry_vec), VTY_NEWLINE);

	vty_out(vty, "%20s%20s%20s%s", "Port", "Rx(p/b)", "Tx(p/b)", VTY_NEWLINE);
	
	if (argc == 0) {
		foreach_iface_func1_param1 (
			argv[0], iface_entry_stat_out, vty);
	}
	else {
		foreach_iface_split_func1_param1 (
			argv[0], iface_entry_stat_out, vty);
	}

	PRINT_SUMMARY;

    return CMD_SUCCESS;
}

DEFUN(clear_interface_stats,
	clear_interface_stats_cmd,
	"clear interface stats [WORD]",
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
	vty_out(vty, "Trying to display %d elements ...%s", 
			vec_active(pm->entry_vec), VTY_NEWLINE);

	if (argc == 0) {
		foreach_iface_func1_param1 (
			argv[0], iface_entry_stat_clear, vty);
	}
	else {
		foreach_iface_split_func1_param1 (
			argv[0], iface_entry_stat_clear, vty);
	}
	
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
		.v = (const char *)argv[1],
		.s = __oryx_unused_val__,
	};
	
	foreach_iface_split_func1_param2 (
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
		.v = (const char *)argv[1],
		.s = __oryx_unused_val__,
	};
	
	foreach_iface_split_func1_param2 (
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
		.v = (const char *)argv[1],
		.s = __oryx_unused_val__,
	};
	
	foreach_iface_split_func1_param2 (
		argv[0], iface_entry_config, vty, (const struct prefix_t *)&var);

	PRINT_SUMMARY;
	
	return CMD_SUCCESS;
}

static int dpdk_iface_is_up(uint32_t portid, struct rte_eth_link *link)
{
	rte_eth_link_get_nowait(portid, link);
	
	if (link->link_status)
		return 1;
	else
		return 0;
}


int link_trasition_detected = 0;

static int iface_poll_linkstate(struct iface_t *this)
{
	int rv;
	struct ethtool_cmd ethtool;
	
	if (this->type == ETH_GE) {
		rv = netdev_is_running(this->sc_alias_fixed, &ethtool);
		/** up -> down */
		if ((this->ul_flags & NETDEV_ADMIN_UP) && rv == 0) {
			oryx_logn("%s is down", iface_alias(this));
			this->ul_up_down_times ++;
			this->ul_flags |= NETDEV_POLL_UP;
		}
		/** down -> up */
		if (!(this->ul_flags & NETDEV_ADMIN_UP) && rv == 1) {
			this->link_autoneg	= 1;
			this->link_duplex	= ethtool.duplex;
			this->link_speed	= ethtool.speed;
			oryx_logn("%s is Link Up - Speed %s Mbps - %s", iface_alias(this),
				ethtool_speed(ethtool.speed), ethtool_duplex(ethtool.duplex));
		}
		switch(rv) {
			case 0:
				this->ul_flags &= ~NETDEV_ADMIN_UP;
				break;
			case 1:
				this->ul_flags |= NETDEV_ADMIN_UP;
				break;
			default:
				oryx_logn("%s error", iface_alias(this));
				break;
		}
	} else {
		struct rte_eth_link link;
		rv = dpdk_iface_is_up(this->ul_id, &link);
		if ((this->ul_flags & NETDEV_ADMIN_UP) && rv == 0) { /** up -> down */
			this->ul_up_down_times ++;
			oryx_logn("%s is down", iface_alias(this));
		}
		if (!(this->ul_flags & NETDEV_ADMIN_UP) && rv == 1) { /** down -> up */
			this->link_autoneg	= 1;
			this->link_duplex	= link.link_duplex;
			this->link_speed	= link.link_speed;
			oryx_logn("%s is Link Up - Speed %u Mbps - %s", iface_alias(this), 
				(unsigned)link.link_speed, (link.link_duplex == ETH_LINK_FULL_DUPLEX) ? \
					("Full-Duplex") : ("Half-Duplex"));
		}
		switch(rv) {
			case 0:
				this->ul_flags &= ~NETDEV_ADMIN_UP;
				break;
			case 1:
				this->ul_flags |= NETDEV_ADMIN_UP;
				break;
			default:
				oryx_logn("%s error", iface_alias(this));
				break;
		}
	}

	return 0;
}

static int iface_poll_up(struct iface_t *this)
{
	if (this->type == ETH_GE)
		return netdev_up(this->sc_alias_fixed);
	else
		return rte_eth_dev_set_link_up(this->ul_id);
}

static struct iface_t iface_list[] = {

	{
		.sc_alias_fixed =	"enp5s0f1",
		.type			=	ETH_XE,
		.ul_flags		=	NETDEV_MARVELL_DSA,
		.if_poll_state	=	iface_poll_linkstate,
		.if_poll_up		=	iface_poll_up,
		.ul_id			=	-1,
		.sc_alias		=	" ",
		.eth_addr		=	{0,0,0,0,0,0},
		.link_speed		=	0,
		.link_duplex	=	0,
		.link_autoneg	=	0,
		.link_pad0		=	LINK_PAD0,
		.mtu			=	0,
		.ul_up_down_times	=	0,
		.perf_private_ctx	=	NULL,
		.if_counter_ctx		=	NULL
	},
	
	{
		.sc_alias_fixed	=	"enp5s0f2",
		.type			=	ETH_XE,
		.ul_flags		=	NETDEV_PANEL,
		.if_poll_state	=	iface_poll_linkstate,
		.if_poll_up 	=	iface_poll_up,
		.ul_id			=	-1,
		.sc_alias		=	" ",
		.eth_addr		=	{0,0,0,0,0,0},
		.link_speed 	=	0,
		.link_duplex	=	0,
		.link_autoneg	=	0,
		.link_pad0		=	LINK_PAD0,
		.mtu			=	0,
		.ul_up_down_times	=	0,
		.perf_private_ctx	=	NULL,
		.if_counter_ctx 	=	NULL

	},

	{
		.sc_alias_fixed =	"enp5s0f3",
		.type			=	ETH_XE,
		.ul_flags		=	NETDEV_PANEL,
		.if_poll_state	=	iface_poll_linkstate,
		.if_poll_up 	=	iface_poll_up,
		.ul_id			=	-1,
		.sc_alias		=	" ",
		.eth_addr		=	{0,0,0,0,0,0},
		.link_speed 	=	0,
		.link_duplex	=	0,
		.link_autoneg	=	0,
		.link_pad0		=	LINK_PAD0,
		.mtu			=	0,
		.ul_up_down_times	=	0,
		.perf_private_ctx	=	NULL,
		.if_counter_ctx 	=	NULL

	},
	
	{
		.sc_alias_fixed	=	"lan5",
		.type			=	ETH_GE,
		.ul_flags		=	NETDEV_PANEL,
		.if_poll_state	=	iface_poll_linkstate,
		.if_poll_up 	=	iface_poll_up,
		.ul_id			=	-1,
		.sc_alias		=	" ",
		.eth_addr		=	{0,0,0,0,0,0},
		.link_speed 	=	0,
		.link_duplex	=	0,
		.link_autoneg	=	0,
		.link_pad0		=	LINK_PAD0,
		.mtu			=	0,
		.ul_up_down_times	=	0,
		.perf_private_ctx	=	NULL,
		.if_counter_ctx 	=	NULL

	},

	{
		.sc_alias_fixed =	"lan6",
		.type			=	ETH_GE,
		.ul_flags		=	NETDEV_PANEL,
		.if_poll_state	=	iface_poll_linkstate,
		.if_poll_up 	=	iface_poll_up,
		.ul_id			=	-1,
		.sc_alias		=	" ",
		.eth_addr		=	{0,0,0,0,0,0},
		.link_speed 	=	0,
		.link_duplex	=	0,
		.link_autoneg	=	0,
		.link_pad0		=	LINK_PAD0,
		.mtu			=	0,
		.ul_up_down_times	=	0,
		.perf_private_ctx	=	NULL,
		.if_counter_ctx 	=	NULL

	},

	{
		.sc_alias_fixed =	"lan7",
		.type			=	ETH_GE,
		.ul_flags		=	NETDEV_PANEL,
		.if_poll_state	=	iface_poll_linkstate,
		.if_poll_up 	=	iface_poll_up,
		.ul_id			=	-1,
		.sc_alias		=	" ",
		.eth_addr		=	{0,0,0,0,0,0},
		.link_speed 	=	0,
		.link_duplex	=	0,
		.link_autoneg	=	0,
		.link_pad0		=	LINK_PAD0,
		.mtu			=	0,
		.ul_up_down_times	=	0,
		.perf_private_ctx	=	NULL,
		.if_counter_ctx 	=	NULL

	},

	{
		.sc_alias_fixed =	"lan8",
		.type			=	ETH_GE,
		.ul_flags		=	NETDEV_PANEL,
		.if_poll_state	=	iface_poll_linkstate,
		.if_poll_up 	=	iface_poll_up,
		.ul_id			=	-1,
		.sc_alias		=	" ",
		.eth_addr		=	{0,0,0,0,0,0},
		.link_speed 	=	0,
		.link_duplex	=	0,
		.link_autoneg	=	0,
		.link_pad0		=	LINK_PAD0,
		.mtu			=	0,
		.ul_up_down_times	=	0,
		.perf_private_ctx	=	NULL,
		.if_counter_ctx 	=	NULL
	},
	
	{
		.sc_alias_fixed =	"lan1",
		.type			=	ETH_GE,
		.ul_flags		=	NETDEV_PANEL,
		.if_poll_state	=	iface_poll_linkstate,
		.if_poll_up 	=	iface_poll_up,
		.ul_id			=	-1,
		.sc_alias		=	" ",
		.eth_addr		=	{0,0,0,0,0,0},
		.link_speed 	=	0,
		.link_duplex	=	0,
		.link_autoneg	=	0,
		.link_pad0		=	LINK_PAD0,
		.mtu			=	0,
		.ul_up_down_times	=	0,
		.perf_private_ctx	=	NULL,
		.if_counter_ctx 	=	NULL
	},
	
	{
		.sc_alias_fixed =	"lan2",
		.type			=	ETH_GE,
		.ul_flags		=	NETDEV_PANEL,
		.if_poll_state	=	iface_poll_linkstate,
		.if_poll_up 	=	iface_poll_up,
		.ul_id			=	-1,
		.sc_alias		=	" ",
		.eth_addr		=	{0,0,0,0,0,0},
		.link_speed 	=	0,
		.link_duplex	=	0,
		.link_autoneg	=	0,
		.link_pad0		=	LINK_PAD0,
		.mtu			=	0,
		.ul_up_down_times	=	0,
		.perf_private_ctx	=	NULL,
		.if_counter_ctx 	=	NULL
	},
	
	{
		.sc_alias_fixed =	"lan3",
		.type			=	ETH_GE,
		.ul_flags		=	NETDEV_PANEL,
		.if_poll_state	=	iface_poll_linkstate,
		.if_poll_up 	=	iface_poll_up,
		.ul_id			=	-1,
		.sc_alias		=	" ",
		.eth_addr		=	{0,0,0,0,0,0},
		.link_speed 	=	0,
		.link_duplex	=	0,
		.link_autoneg	=	0,
		.link_pad0		=	LINK_PAD0,
		.mtu			=	0,
		.ul_up_down_times	=	0,
		.perf_private_ctx	=	NULL,
		.if_counter_ctx 	=	NULL
	},
	
	{
		.sc_alias_fixed =	"lan4",
		.type			=	ETH_GE,
		.ul_flags		=	NETDEV_PANEL,
		.if_poll_state	=	iface_poll_linkstate,
		.if_poll_up 	=	iface_poll_up,
		.ul_id			=	-1,
		.sc_alias		=	" ",
		.eth_addr		=	{0,0,0,0,0,0},
		.link_speed 	=	0,
		.link_duplex	=	0,
		.link_autoneg	=	0,
		.link_pad0		=	LINK_PAD0,
		.mtu			=	0,
		.ul_up_down_times	=	0,
		.perf_private_ctx	=	NULL,
		.if_counter_ctx 	=	NULL
	}

};

static void register_ports(void)
{
	int i;
	struct iface_t *new;
	struct iface_t *this;
	vlib_iface_main_t *pm = &vlib_iface_main;
	int n_ports_now = vec_count(pm->entry_vec);

	/** SW<->CPU ports, only one. */
	for (i = 0; i < (int)DIM(iface_list); i ++) {

		this = &iface_list[i];
		
		/** lan1-lan8 are not vfio-pci drv iface. */
		if (this->type == ETH_GE && (this->ul_flags & NETDEV_PANEL)) {
			if (!netdev_exist(this->sc_alias_fixed)) {
				continue;
			}
		}
		
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
			if (!iface_add(pm, new))
				oryx_logn("registering interface %s ... success", iface_alias(new));
			else
				oryx_loge(-1, "registering interface %s ... error", iface_alias(new));
		}
	}
}


static __oryx_always_inline__
void iface_activity_prob_tmr_handler(struct oryx_timer_t __oryx_unused_param__*tmr,
			int __oryx_unused_param__ argc, char __oryx_unused_param__**argv)
{
	vlib_iface_main_t *pm = &vlib_iface_main;	
	int each;
	struct iface_t *this;

	vec_foreach_element(pm->entry_vec, each, this) {
		if (likely(this)) {
			/** check linkstate. */
			if (this->if_poll_state)
				this->if_poll_state(this);

			/** poll a iface up if need. */
			if(this->ul_flags & NETDEV_POLL_UP) {
				/** up this interface right now. */
				if(this->if_poll_up) {
					this->if_poll_up(this);
				} else {
					oryx_loge(-1,
						"ethdev up driver is not registered, this iface will down forever.");
				}
			}
		}
	}
}

void vlib_iface_init(vlib_main_t *vm)
{
	vlib_iface_main_t *pm = &vlib_iface_main;
	uint32_t ul_activity_tmr_setting_flags = TMR_OPTIONS_PERIODIC | TMR_OPTIONS_ADVANCED;
	
	pm->link_detect_tmr_interval = 3;
	pm->vm			= vm;
	pm->entry_vec	= vec_init (MAX_PORTS);
	pm->htable		= oryx_htable_init(DEFAULT_HASH_CHAIN_SIZE, 
							ht_iface_hval, ht_iface_cmp, ht_iface_free, 0);

	if (pm->htable == NULL || pm->entry_vec == NULL) {
		printf ("vlib iface main init error!\n");
		exit(0);
	}
	    
	install_element (CONFIG_NODE, &show_interface_cmd);
	install_element (CONFIG_NODE, &show_interfacce_stats_cmd);
	install_element (CONFIG_NODE, &clear_interface_stats_cmd);
	install_element (CONFIG_NODE, &interface_alias_cmd);
	install_element (CONFIG_NODE, &interface_mtu_cmd);
	install_element (CONFIG_NODE, &interface_looback_cmd);

	pm->link_detect_tmr = oryx_tmr_create(1, "iface activity monitoring tmr", 
							ul_activity_tmr_setting_flags,
							iface_activity_prob_tmr_handler,
							0, NULL, pm->link_detect_tmr_interval);
	register_ports();

	if(likely(pm->link_detect_tmr))
		oryx_tmr_start(pm->link_detect_tmr);

	vm->ul_flags |= VLIB_PORT_INITIALIZED;
}


