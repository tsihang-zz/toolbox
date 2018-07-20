#ifndef IFACE_ENUM_SF1500_H
#define IFACE_ENUM_SF1500_H

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

