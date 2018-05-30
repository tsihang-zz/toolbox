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
	QUA_COUNTER_RX,
	QUA_COUNTER_TX,
	QUA_COUNTERS
};

enum {
	ETH_GE,
	ETH_XE
};

struct iface_counter_ctx {
	counter_id lcore_counter_pkts[QUA_COUNTERS][MAX_LCORES];
	counter_id lcore_counter_bytes[QUA_COUNTERS][MAX_LCORES];
};

struct iface_t {
	const char *sc_alias_fixed; /** fixed alias used to do linkstate poll,
								 *  and can not be overwrite by CLI. */
	int type;					/** ETH_GE, ETH_XE, ... */

#define NETDEV_ADMIN_UP           (1 << 0)	/** 0-down, 1-up */
#define NETDEV_PROMISC            (1 << 1)
#define	NETDEV_DUPLEX_FULL		  (1 << 2)	/** 0-half, 1-full */
#define NETDEV_PMD                (1 << 3)
#define	NETDEV_LOOPBACK	  		  (1 << 4)
#define NETDEV_POLL_UP			  (1 << 5)	/** poll this port up. */
#define NETDEV_MARVELL_DSA		  (1 << 6)	/** marvell dsa frame. */
#define NETDEV_PANEL			  (1 << 7)	/** is a panel port or not. 
											  cpu <-> sw */
	u32 ul_flags;

	int (*if_poll_state)(struct iface_t *this);
	int (*if_poll_up)(struct iface_t *this);	
	u32 ul_id;					/** A panel interface ID. enp5s0f1, enp5s0f2, enp5s0f3, lan1 ... lan8
								 *  Can be translated by TO_INFACE_ID(Frame.vlan) */
	char  sc_alias[32];			/** Port name. 
								 *  for example, Gn_b etc... can be overwritten by CLI. */
	char eth_addr[6];			/** ethernet address for this port. */
	u32 ul_up_down_times;		/** up->down counter. */
	u16 us_mtu;
	oryx_vector belong_maps;	/** Map for this interface belong to. */
	struct CounterCtx *perf_private_ctx;
	struct iface_counter_ctx *if_counter_ctx;
};
#define iface_alias(p) (p)->sc_alias
#define iface_id(p)	   (p)->ul_id
#define iface_perf(p)  (p)->perf_private_ctx
#define iface_support_marvell_dsa(p) ((p)->ul_flags & NETDEV_MARVELL_DSA)

#define iface_counters_add(p,id,x)\
	oryx_counter_add(iface_perf((p)),(id),(x));
#define iface_counters_inc(p,id,x)\
	oryx_counter_inc(iface_perf((p)),(id));
#define iface_counters_set(p,id,x)\
	oryx_counter_set(iface_perf((p)),(id),(x));

typedef struct vlib_port_main {
	/* dpdk_ports + sw_ports */
	int ul_n_ports;
	u32 ul_flags;
	struct oryx_timer_t *link_detect_tmr;
	u32 link_detect_tmr_interval;
	u32 poll_interval;

	os_lock_t lock;
	oryx_vector entry_vec;
	struct oryx_htable_t *htable;

	/** enp5s0f1 must be up first before lan1-lan8 up. */
	int enp5s0f1_is_up;
	
}vlib_port_main_t;

extern vlib_port_main_t vlib_port_main;

static __oryx_always_inline__ int iface_lookup_id(vlib_port_main_t *vp,
				u32 id, struct iface_t **this)
{
	(*this) = NULL;
	(*this) = (struct iface_t *) vec_lookup (vp->entry_vec, id);

#if defined(BUILD_DEBUG)
	BUG_ON((*this) == NULL || (*this->ul_id) != id);
#endif	
	return 0;
}

void iface_alloc (struct iface_t **);
int iface_rename(vlib_port_main_t *vp, 
				struct iface_t *this, const char *new_name);
int iface_lookup_alias(vlib_port_main_t *vp,
				const char *alias, struct iface_t **this);

int iface_add(vlib_port_main_t *vp, struct iface_t *this);

int iface_del(vlib_port_main_t *vp, struct iface_t *this);


#endif

