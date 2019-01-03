#ifndef __BOX_IFACE_PRIVATE_H__
#define __BOX_IFACE_PRIVATE_H__

#define foreach_iface_config_prefix_cmd				\
		  _(SET_ALIAS, "Set interface alias.")				\
		  _(SET_MTU, "10,000 Gbps.")						\
		  _(SET_LINKUP, "40,000 Gbps.") 					\
		  _(CLEAR_STATS, "Clear interface statistics.") 	\
		  _(SET_LOOPBACK, "Interface loopback.")			\
		  _(SET_VID, "100,000 Gbps.")
		
		
enum interface_conf_cmd {
#define _(f,s) INTERFACE_##f,
	foreach_iface_config_prefix_cmd
#undef _
	INTERFACE,
};


struct iface_counter_ctx {
	counter_id lcore_counter_pkts[QUA_RXTX][VLIB_MAX_LCORES];
	counter_id lcore_counter_bytes[QUA_RXTX][VLIB_MAX_LCORES];
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

#if defined(BUILD_DEBUG)
#define iface_lookup_id(pm,id,iface)\
			iface_lookup_id0((pm),(id),(iface));
#else
#define iface_lookup_id(pm,id,iface)\
			(*(iface)) = NULL;\
			(*(iface)) = (struct iface_t *) vec_lookup ((pm)->entry_vec, (id));
#endif

typedef struct vlib_iface_main_t {
	int						ul_n_ports;	/* dpdk_ports + sw_ports */
	uint32_t				ul_flags;
	struct oryx_timer_t		*link_detect_tmr;
	struct oryx_timer_t		*healthy_tmr;
	uint32_t				link_detect_tmr_interval;
	uint32_t				poll_interval;
	sys_mutex_t				lock;
	oryx_vector				entry_vec;
	struct oryx_htable_t	*htable;
	void					*vm;
} vlib_iface_main_t;

extern vlib_iface_main_t vlib_iface_main;


static __oryx_always_inline__
int iface_lookup_alias(vlib_iface_main_t *pm,
				char *alias, struct iface_t **this)
{
	(*this) = NULL;
	void *s = oryx_htable_lookup(pm->htable, (ht_value_t)&alias[0],
									strlen((const char *)alias));			
	if (likely (s)) {
		(*this) = (struct iface_t *) container_of (s, struct iface_t, sc_alias);
		return 0;
	}
	return -1;
}

static __oryx_always_inline__
int iface_lookup_id0
(
	IN vlib_iface_main_t *pm,
	IN uint32_t id,
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
			iface_lookup_alias(pm, (char*)lp->v, p);
			break;
		default:
			break;
	}
}

static __oryx_always_inline__
void iface_alloc
(
	OUT struct iface_t **this
)
{
	int 						id;
	int 						lcore;
	char						lcore_stats_name[1024];
	struct oryx_counter_ctx_t			*per_private_ctx0;
	struct iface_counter_ctx	*if_counter_ctx0;

	(*this) = NULL;
	
	/** create an port */
	(*this) = kmalloc (sizeof (struct iface_t), MPF_CLR, __oryx_unused_val__);
	BUG_ON ((*this) == NULL);

	memcpy(&(*this)->sc_alias[0], "--", strlen("--"));
	(*this)->mtu = 1500;
	(*this)->perf_private_ctx = kmalloc(sizeof(struct oryx_counter_ctx_t), MPF_CLR, __oryx_unused_val__);
	(*this)->if_counter_ctx = kmalloc(sizeof(struct iface_counter_ctx), MPF_CLR, __oryx_unused_val__);

	per_private_ctx0 = (*this)->perf_private_ctx;
	if_counter_ctx0 = (*this)->if_counter_ctx;
	
	for (id = QUA_RX; id < QUA_RXTX; id ++) {
		for (lcore = 0; lcore < VLIB_MAX_LCORES; lcore ++) { 	
			sprintf(lcore_stats_name, "port.%s.bytes.lcore%d", id == QUA_RX ? "rx" : "tx", lcore);
			if_counter_ctx0->lcore_counter_bytes[id][lcore] = oryx_register_counter(strdup(lcore_stats_name), 
					"NULL", per_private_ctx0);
		
			sprintf(lcore_stats_name, "port.%s.pkts.lcore%d", id == QUA_RX ? "rx" : "tx", lcore);
			if_counter_ctx0->lcore_counter_pkts[id][lcore] = oryx_register_counter(strdup(lcore_stats_name), 
					"NULL", per_private_ctx0);
		}
	}


	/**
	 * More Counters here.
	 * TODO ...
	 */
	
	/** last step */
	oryx_counter_get_array_range(COUNTER_RANGE_START(per_private_ctx0), 
			COUNTER_RANGE_END(per_private_ctx0), 
			per_private_ctx0);
}

static __oryx_unused__
int iface_rename
(
	IN vlib_iface_main_t *pm,
	IN struct iface_t *this,
	IN const char *new_name
)
{
	/** Delete old alias from hash table. */
	oryx_htable_del (pm->htable, iface_alias(this), strlen (iface_alias(this)));
	memset (iface_alias(this), 0, strlen (iface_alias(this)));
	memcpy (iface_alias(this), new_name, strlen (new_name));
	/** New alias should be rewrite to hash table. */
	oryx_htable_add (pm->htable, iface_alias(this), strlen (iface_alias(this)));	
	return 0;
}

static __oryx_unused__
int iface_add
(
	IN vlib_iface_main_t *pm,
	IN struct iface_t *this
)
{
	oryx_sys_mutex_lock (&pm->lock);
	int r = oryx_htable_add(pm->htable, iface_alias(this),
						strlen((const char *)iface_alias(this)));
	if (r == 0) {
		vec_set_index (pm->entry_vec, this->ul_id, this);
		pm->ul_n_ports ++;
	}
	oryx_sys_mutex_unlock (&pm->lock);
	return r;
}

static __oryx_unused__
int iface_del
(
	IN vlib_iface_main_t *pm,
	IN struct iface_t *this
)
{
	oryx_sys_mutex_lock (&pm->lock);
	int r = oryx_htable_del(pm->htable, iface_alias(this),
						strlen((const char *)iface_alias(this)));
	if (r == 0) {
		vec_unset (pm->entry_vec, this->ul_id);
		pm->ul_n_ports --;
	}
	oryx_sys_mutex_unlock (&pm->lock);
	return r;
}

#endif

