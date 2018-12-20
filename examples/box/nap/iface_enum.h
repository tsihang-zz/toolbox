#ifndef IFACE_ENUM_SF1500_H
#define IFACE_ENUM_SF1500_H

#include "netdev.h"

enum {
	ETH_GE,
	ETH_XE
};

static int dpdk_iface_is_up(uint32_t portid, struct rte_eth_link *link)
{
	rte_eth_link_get_nowait(portid, link);
	
	if (link->link_status)
		return 1;
	else
		return 0;
}

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

#endif

