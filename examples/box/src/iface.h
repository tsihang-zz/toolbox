#ifndef IFACE_H
#define IFACE_H

enum {
	ETH_GE,
	ETH_XE
};

struct iface_counter_ctx {
	counter_id lcore_counter_pkts[QUA_RXTX][MAX_LCORES];
	counter_id lcore_counter_bytes[QUA_RXTX][MAX_LCORES];
};

#define NETDEV_ADMIN_UP							(1 << 0)	/** 0-down, 1-up */
#define NETDEV_PROMISC							(1 << 1)
#define	NETDEV_LOOPBACK							(1 << 3)
#define NETDEV_POLL_UP							(1 << 4)	/** poll this port up. */
#define NETDEV_MARVELL_DSA						(1 << 5)	/** marvell dsa frame. */
#define NETDEV_PANEL							(1 << 6)	/** is a panel port or not.
															 *  cpu <-> sw */
#define LINK_PAD0	(0)
struct iface_t {
	const char				*sc_alias_fixed; 	/** fixed alias used to do linkstate poll,
								 			  	 *  and can not be overwrite by CLI. */
	int						type;				/** ETH_GE, ETH_XE, ... */
	uint32_t				ul_flags;

	int						(*if_poll_state)(struct iface_t *this);
	int						(*if_poll_up)(struct iface_t *this);	
	uint32_t				ul_id;				/* A panel interface ID. 
											 	 * enp5s0f1, enp5s0f2, enp5s0f3, lan1 ... lan8
											 	 * Can be translated by TO_INFACE_ID(Frame.vlan) */
	char					sc_alias[32];		/* Port name.
												 * for example, Gn_b etc... 
												 * can be overwritten by CLI. */
	char					eth_addr[6];		/** ethernet address for this port. */
	uint32_t				ul_up_down_times;	/** up->down counter. */

		uint32_t link_speed;		/**< ETH_SPEED_NUM_ */
		uint32_t link_duplex  : 8;	/**< ETH_LINK_[HALF/FULL]_DUPLEX */
		uint32_t link_autoneg : 1;	/**< ETH_LINK_SPEED_[AUTONEG/FIXED] */
		uint32_t link_pad0    : 7;
		uint32_t mtu       		: 16;	/**< MTU */

	struct oryx_counter_ctx_t 		*perf_private_ctx;
	struct iface_counter_ctx *if_counter_ctx;

	uint32_t			ul_map_mask;		/** map for this iface belong to. */
};

#define iface_alias(p) (p)->sc_alias
#define iface_id(p)	   (p)->ul_id
#define iface_perf(p)  (p)->perf_private_ctx
#define iface_support_marvell_dsa(p) ((p)->ul_flags & NETDEV_MARVELL_DSA)

#define iface_counters_add(p,id,x)\
	oryx_counter_add(iface_perf((p)),(id),(x));
#define iface_counters_inc(p,id)\
	oryx_counter_inc(iface_perf((p)),(id));
#define iface_counters_set(p,id,x)\
	oryx_counter_set(iface_perf((p)),(id),(x));

#define iface_counter_update(iface,nb_pkts,nb_bytes,rx_tx,lcore)\
	iface_counters_add((iface),\
		(iface)->if_counter_ctx->lcore_counter_pkts[rx_tx][(lcore)], nb_pkts);\
	iface_counters_add((iface),\
		(iface)->if_counter_ctx->lcore_counter_bytes[rx_tx][(lcore)], nb_bytes);

static __oryx_always_inline__
int iface_lookup_id0
(
	IN vlib_iface_main_t *pm,
	IN const uint32_t id,
	OUT struct iface_t **this
)
{
	BUG_ON(pm->entry_vec == NULL);
	
	(*this) = NULL;
	if (!vec_active(pm->entry_vec) ||
		id > vec_active(pm->entry_vec))
		return 0;
	(*this) = (struct iface_t *) vec_lookup (pm->entry_vec, id);

	return 0;
}

#if defined(BUILD_DEBUG)
#define iface_lookup_id(pm,id,iface)\
	iface_lookup_id0((pm),(id),(iface));
#else
#define iface_lookup_id(pm,id,iface)\
	(*(iface)) = NULL;\
	(*(iface)) = (struct iface_t *) vec_lookup ((pm)->entry_vec, (id));
#endif

ORYX_DECLARE (
	void vlib_iface_init (
		IN vlib_main_t *vm
	)
);

#endif
