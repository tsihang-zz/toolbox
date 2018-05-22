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
    /** stats/counters */
    counter_id counter_pkts[QUA_COUNTERS];
    counter_id counter_bytes[QUA_COUNTERS];
	
    counter_id counter_eth[QUA_COUNTERS];
    counter_id counter_ipv4[QUA_COUNTERS];
    counter_id counter_ipv6[QUA_COUNTERS];
    counter_id counter_tcp[QUA_COUNTERS];
    counter_id counter_udp[QUA_COUNTERS];
    counter_id counter_icmpv4[QUA_COUNTERS];
    counter_id counter_icmpv6[QUA_COUNTERS];
    counter_id counter_sctp[QUA_COUNTERS];
	counter_id counter_arp[QUA_COUNTERS];
    counter_id counter_vlan[QUA_COUNTERS];
    counter_id counter_pppoe[QUA_COUNTERS];
    counter_id counter_mpls[QUA_COUNTERS];
};

struct iface_t {
	const char *sc_alias_fixed; /** fixed alias used to do linkstate poll,
								 *  and can not be overwrite by CLI. */
	int type;					/** ETH_GE, ETH_XE, ... */
	int is_a_panel_port;		/** panel port or inner port bt cpu <-> sw. */
	int (*if_poll_state)(struct iface_t *this);
	int (*if_poll_up)(struct iface_t *this);	
	u32 ul_id;					/** A panel interface ID. enp5s0f1, enp5s0f2, enp5s0f3, lan1 ... lan8
								 *  Can be translated by TO_INFACE_ID(Frame.vlan) */
	char  sc_alias[32];			/** Port name. 
								 *  for example, Gn_b etc... can be overwritten by CLI. */
	char eth_addr[6];			/** ethernet address for this port. */
#define NETDEV_ADMIN_UP           (1 << 0)	/** 0-down, 1-up */
#define NETDEV_PROMISC            (1 << 1)
#define	NETDEV_DUPLEX_FULL		  (1 << 2)	/** 0-half, 1-full */
#define NETDEV_PMD                (1 << 3)
#define	NETDEV_LOOPBACK	  		  (1 << 4)
#define NETDEV_POLL_UP			  (1 << 5)	/** poll this port up. */
	u32 ul_flags;
	u32 ul_up_down_times;		/** up->down counter. */
	u16 us_mtu;
	oryx_vector belong_maps;	/** Map for this interface belong to. */
	struct CounterCtx *perf_private_ctx;
	struct iface_counter_ctx *if_counter_ctx;
};

static inline void iface_counters_add(struct iface_t *p,
					counter_id id, u64 x){
	oryx_counter_add(p->perf_private_ctx, id, x);
}

static inline void iface_counters_inc(struct iface_t *p,
					counter_id id){
	oryx_counter_inc(p->perf_private_ctx, id);
}

static inline void iface_counters_set(struct iface_t *p,
					counter_id id, u64 x){
	oryx_counter_set(p->perf_private_ctx, id, x);
}

static inline void iface_counters_clear(struct iface_t *p,
					counter_id id){
	oryx_counter_set(p->perf_private_ctx, id, 0);
}

					
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

#define port_alias(p) ((p)->sc_alias)

void iface_alloc (struct iface_t **);
int iface_rename(vlib_port_main_t *vp, 
				struct iface_t *this, const char *new_name);
int iface_lookup_id(vlib_port_main_t *vp,
				u32 id, struct iface_t **this);
int iface_lookup_alias(vlib_port_main_t *vp,
				const char *alias, struct iface_t **this);

int iface_add(vlib_port_main_t *vp, struct iface_t *this);

int iface_del(vlib_port_main_t *vp, struct iface_t *this);


#endif

