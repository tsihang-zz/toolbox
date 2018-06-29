#ifndef MAP_PRIVATE_H
#define MAP_PRIVATE_H

#define MAP_PREFIX	"map"
#define MAX_MAPS 	(32)	/** a uint32_t per bit value. */

/** 
	Make signature id (outer). 
	UDP_N_PATTERNS define the maximum patterns per udp. 
	so 8bits for pattern can support maxmum 255 patterns. 
*/
#define UDP_ID_OFFSET	(8)
#define PATTERN_ID_OFFSET (0)
#define MAP_ID_OFFSET	(0)

#define mkpid(map_id, udp_id, pattern_id)\
	(((udp_id) << UDP_ID_OFFSET) | (((pattern_id) << PATTERN_ID_OFFSET) & 0xFF))
#define mksid(map_id, udp_id, pattern_id)\
	(((udp_id) << UDP_ID_OFFSET) | (((pattern_id) << PATTERN_ID_OFFSET) & 0xFF))

#define split_udp_id(sid)\
	((sid >> UDP_ID_OFFSET))
#define split_pattern_id(sid)\
	((sid >> PATTERN_ID_OFFSET) & 0xFF)
#define split_map_id(sid)\
	0
#define split_id(sid, map_id, udp_id, pattern_id)\
	map_id=split_map_id(sid);\
	udp_id=split_udp_id(sid);\
	pattern_id=split_pattern_id(sid);

typedef enum {
	MAP_CMD_ADD,
	MAP_CMD_DEL,
	MAP_CMD_ADD_APPL,
	MAP_CMD_DEL_APPL,
} map_cmd;

/** Vector table is used for DATAPLANE and CONTROLPLANE. */
enum {
	VECTOR_TABLE0,
	VECTOR_TABLE1,
	VECTOR_TABLES,
};

enum {
	MPM_TABLE0,
	MPM_TABLE1,
	MPM_TABLES,
};


/**
 * Packets matching multiple maps in a configuration are sent to the map with the highest
 * priority when the network ports are shared among multiple maps with pass-by map
 * rules. By default, the first map configured has the highest priority; however, you can
 * adjust this.
 * Defaultly, priority for each map is ruled by ul_id. 
 */
struct map_t {

	char			sc_alias[32];				/** Unique, and also can be well human-readable. */
	uint32_t		ul_id;						/** Unique, allocated automatically. */
	os_mutex_t 		*ol_lock;
	char 			*port_list_str[QUA_RXTX];	/** A temporary variable holding argvs from CLI,
	     											and will be freee after split. */
	uint32_t		rx_panel_port_mask;			/** Attention: QUA_RX, Where frame comes from.
													More than one map can be configured with 
													the same source ports in the port list.
													The source port list of one map must be 
													exactly the same as the source port list 
													of another map (have the same ports 
													as well as the same number of ports) and 
													it must not overlap with the source port list
													of any other map. Maps sharing the same source 
													port list are grouped together for the purpose
													of prioritizing their rules.*/
	uint32_t		tx_panel_port_mask;			/** The same with this.rx_panel_port_mask. */

	uint32_t		online_tx_panel_ports[MAX_PORTS];
	uint32_t		nb_online_tx_panel_ports;

	uint32_t		online_rx_panel_ports[MAX_PORTS];
	uint32_t		nb_online_rx_panel_ports;

	uint32_t		ul_nb_appls;
	
#define MAP_DEFAULT				(1 << 0)		/** Default map can not be removed. */
#define MAP_TRAFFIC_TRANSPARENT	(1 << 1)		/** Transparent, default setting for a map.
												 * When a map created, traffic will be transparent bettwen its "from" and "to" port,
												 * no matter whether there are passed applications, udps or not. */
#define MAP_INTELLIGENT_LB		(1 << 2)
#define MAP_ENABLED				(1 << 3)
#define MAP_VALID				(1 << 4)
	uint32_t		ul_flags;
	uint64_t		ull_create_time;			/** Create time. */
	
};

#define map_id(map) ((map)->ul_id)
#define map_alias(map) ((map)->sc_alias)

#define map_rx_has_iface(map,iface)\
	((map)->rx_panel_port_mask & (1 << iface_id((iface))))
#define map_tx_has_iface(map,iface)\
	((map)->tx_panel_port_mask & (1 << iface_id((iface))))

#define map_is_invalid(map)\
	(unlikely(!((map))) || !(((map))->ul_flags & MAP_VALID))

#define VLIB_MM_XXXXXXXXXX		(1 << 0)
typedef struct {
	uint32_t				ul_flags;
	volatile uint32_t		nb_maps;
	os_mutex_t 				lock;
	volatile uint32_t		vector_runtime;
	volatile oryx_vector	entry_vec;
	struct oryx_timer_t		*online_port_update_tmr;
	struct oryx_htable_t	*htable;	
	void					*vm;
}vlib_map_main_t;

extern vlib_map_main_t vlib_map_main;

static __oryx_always_inline__
void map_entry_lookup_alias (vlib_map_main_t *mm, char *alias, struct map_t **map)
{
	(*map) = NULL;
	
	if (!alias) return;

	void *s = oryx_htable_lookup (mm->htable,
		(ht_value_t)alias, strlen((const char *)alias));

	if (s) {
		(*map) = (struct map_t *) container_of (s, struct map_t, sc_alias);
	}
}

static __oryx_always_inline__
void map_entry_lookup_id0 (vlib_map_main_t *mm, u32 id, struct map_t **m)
{
	BUG_ON(mm->entry_vec == NULL);

	(*m) = NULL;
	if (!vec_active(mm->entry_vec) || 
		(id > vec_active(mm->entry_vec)))
		return;	
	(*m) = (struct map_t *)vec_lookup (mm->entry_vec, id);
}

#if defined(BUILD_DEBUG)
#define map_entry_lookup_id(mm,id,m)\
	map_entry_lookup_id0((mm),(id),(m));
#else
#define map_entry_lookup_id(mm,id,m)\
	(*(m)) = NULL;\
	(*(m)) = (struct map_t *)vec_lookup((mm)->entry_vec, (id));
#endif

static __oryx_always_inline__
void map_table_entry_lookup (struct prefix_t *lp, 
	struct map_t **m)
{
	vlib_map_main_t *mm = &vlib_map_main;

	ASSERT (lp);
	ASSERT (m);
	(*m) = NULL;
	
	switch (lp->cmd) {
		case LOOKUP_ID:
			map_entry_lookup_id0(mm, (*(u32*)lp->v), m);
			break;
		case LOOKUP_ALIAS:
			map_entry_lookup_alias(mm, (char*)lp->v, m);
			break;
		default:
			break;
	}
}

#endif

