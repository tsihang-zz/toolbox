#ifndef IFACE_PRIVATE_H
#define IFACE_PRIVATE_H

#include "common_private.h"


#define foreach_iface_speed			\
  _(1G, "1,000 Gbps.")				\
  _(10G, "10,000 Gbps.")			\
  _(40G, "40,000 Gbps.")			\
  _(100G, "100,000 Gbps.")			\

enum speed_t {
#define _(f,s) ETH_SPEED_##f,
	foreach_iface_speed
#undef _
	ETH_SPEED,
};

#define foreach_iface_config_prefix_cmd				\
  _(SET_ALIAS, "Set interface alias.")				\
  _(SET_MTU, "10,000 Gbps.")						\
  _(SET_LINKUP, "40,000 Gbps.")						\
  _(CLEAR_STATS, "Clear interface statistics.")		\
  _(SET_LOOPBACK, "Interface loopback.")			\
  _(SET_VID, "100,000 Gbps.")


enum interface_conf_cmd {
#define _(f,s) INTERFACE_##f,
	foreach_iface_config_prefix_cmd
#undef _
	INTERFACE,
};

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

	struct CounterCtx 		*perf_private_ctx;
	struct iface_counter_ctx *if_counter_ctx;
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

#define VLIB_PM_LINK_TRANSITION_DETECTED			(1 << 0)
typedef struct vlib_iface_main {
	int						ul_n_ports;	/* dpdk_ports + sw_ports */
	uint32_t				ul_flags;
	struct oryx_timer_t		*link_detect_tmr;
	uint32_t						link_detect_tmr_interval;
	uint32_t						poll_interval;
	os_mutex_t				lock;
	oryx_vector				entry_vec;
	struct oryx_htable_t	*htable;
	void					*vm;
}vlib_iface_main_t;

extern vlib_iface_main_t vlib_iface_main;

static __oryx_always_inline__
int iface_lookup_id0(vlib_iface_main_t *pm,
				uint32_t id, struct iface_t **this)
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

static __oryx_always_inline__
int iface_lookup_alias(vlib_iface_main_t *pm,
				const char *alias, struct iface_t **this)
{
	(*this) = NULL;
	void *s = oryx_htable_lookup(pm->htable, (ht_value_t)alias,
									strlen((const char *)alias));			
	if (likely (s)) {
		(*this) = (struct iface_t *) container_of (s, struct iface_t, sc_alias);
		return 0;
	}
	return -1;
}

static __oryx_always_inline__
void iface_table_entry_lookup (struct prefix_t *lp, 
				struct iface_t **p)
{
	vlib_iface_main_t *pm = &vlib_iface_main;

	ASSERT (lp);
	ASSERT (p);
	
	switch (lp->cmd) {
		case LOOKUP_ID:
			iface_lookup_id0(pm, (*(uint32_t*)lp->v), p);
			break;
		case LOOKUP_ALIAS:
			iface_lookup_alias(pm, (const char*)lp->v, p);
			break;
		default:
			break;
	}
}

void iface_alloc (struct iface_t **);
int iface_rename(vlib_iface_main_t *pm, 
				struct iface_t *this, const char *new_name);
int iface_add(vlib_iface_main_t *pm, struct iface_t *this);

int iface_del(vlib_iface_main_t *pm, struct iface_t *this);


#endif

