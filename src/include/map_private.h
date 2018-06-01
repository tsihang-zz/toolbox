#ifndef MAP_PRIVATE_H
#define MAP_PRIVATE_H


#define MAP_PREFIX	"map"
#define MAX_MAPS (1 << 2)

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

/** 
we can create map rules that direct traffic on any network port(s)
to any tool ports. If a shared-ports destination for a set of network ports is not defined,
non-matching traffic will be silently discarded.*/
enum {
	/**
	Network port(s). 
	where you connect data sources to Teraspek-OS nodes. For
example, you could connect a switch's SPAN port, connect an external TAP, or
simply connect an open port on a hub to an open port on a line card. Regardless,
the idea is the same - network ports are where data arrives at the node.
	In their standard configuration, network ports both accept data input and data output are allowed
	*/
	MAP_N_PORTS_FROM,

	/** 
	Tool port(s) 
	Tool ports only allow data output to a connected tool. Any data arriving at
the tool port from an external source will be discarded. In addition, a tool port's Link
Status must be up for packets to be sent out of the port. You can check a port's link
status with the show port command.*/
	MAP_N_PORTS_TO,

	/** 
	  * Collector port(s)
	  */
	MAP_N_PORTS_SHARED,

	/** 
	  * Unused. 
	  */
	MAP_N_PORTS,
};

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

/** Dataplane traffic output. */
struct traffic_dpo_t {
	u8 uc_mode;
	void (*dpo_fn)(void *packet, void *map);
};

typedef struct _action_t {
	uint8_t act;
}action_t;

/**
Packets matching multiple maps in a configuration are sent to the map with the highest
priority when the network ports are shared among multiple maps with pass-by map
rules. By default, the first map configured has the highest priority; however, you can
adjust this.
	Defaultly, priority for each map is ruled by ul_id. 
*/
struct map_t {

	/** 
	  * Unique, and also can be well human-readable.
	  */
	char sc_alias[32];

	/**
	  * Unique, allocated automatically.
	  */
	u32 ul_id;

	/**
	  * lock 
	  */
	os_lock_t *ol_lock;

	/**
	  * A temporary variable holding argvs from CLI, and will be freee after split.
	  */
	char *port_list_str[MAP_N_PORTS];

	/** 
	  * Attention: MAP_N_PORTS_FROM, Where frame comes from. More than one map can be configured 
with the same source ports in the port list.The source port list of one map must be exactly the same as 
the source port list of another map (have the same ports as well as the same number of ports) and it must
not overlap with the source port list of any other map. 
	Maps sharing the same source port list are grouped together for the purpose of
prioritizing their rules.
	*/
	oryx_vector port_list[MAP_N_PORTS];

	action_t	appl_set_action[1024];	/** action = appl_set_action[appl_id] */

	oryx_vector appl_set;		/** APPL set for current map. Attentions: all applications in one map have "OR" relationship.
	 							 * How Flow Mapping handles a case where a packet matches multiple applications in the same map?
	 							 * In cases like this, the packet is sent to all configured destinations 
	 							 * when the first pass application is matched (assuming there were no matching 
	 							 * drop rules �C drop rules have higher priority)
	 							 * For example, a confiigured map named "Pass_HTTP" within 3 applications below:
	 							 * appl1= "Source Address=192.168.1.1";
	 							 * appl2= "Source Port=80";
	 						 	 * appl3= "VLAN=100".
	 							 * In this case, when a packet whose "Source Address=192.168.1.1" && "Source Port=80" && "VLAN=100"
     							 * will be send once to destinations.
	 							 * If you only want to send packet to one destiation, just creating an application 
	 							 * with combind criterias like below and added it to map. applx= "Source Address=192.168.1.1, Source Port=80, VLAN=100" */

	/**
	  * A copy of User-Defined Pattern from UDP Management.
	  */
	oryx_vector udp_set;
	
	/**
	  * Mpm type, default is MPM_AC.
	  */
	u8 uc_mpm_type;

	u32 ul_mpm_has_been_setup;

	/** 
	  * Muiltiple Pattern Match.
	  */
	u8 mpm_index;
	MpmCtx *mpm_runtime_ctx;
	MpmCtx mpm_ctx[MPM_TABLES];
	MpmThreadCtx mpm_thread_ctx;
	PrefilterRuleStore pmq;

	struct list_head prio_node;

/**
 * Default map can not be removed.
 */
#define MAP_DEFAULT				(1 << 0)
/** 
 * Transparent, default setting of map.
 * When a map created, traffic will be transparent bettwen its "from" and "to" port,
 * no matter whether there are passed applications, udps or not.
 */
#define MAP_TRAFFIC_TRANSPARENT	(1 << 1)
	u32 ul_flags;

	struct traffic_dpo_t dpo;
	
	/**
	  * Create time.
	  */
	u64 ull_create_time;
	
};
#define map_slot(map) ((map)->ul_id)
#define map_alias(map) ((map)->sc_alias)

typedef struct {
	int ul_maps;
	u32 ul_flags;

	struct list_head map_priority_list;
	
	os_lock_t lock;

	volatile u32 vector_runtime;
	
	oryx_vector entry_vec[VECTOR_TABLES];
	struct oryx_htable_t *htable;
	
	/** fast tracinging. unused actually. */
	struct map_t *lowest_map;
	struct map_t *highest_map;	
}vlib_map_main_t;

extern vlib_map_main_t vlib_map_main;

#if 0
struct map_table_t {
	oryx_vector mt_entry;
	u32 entries;
	os_lock_t  ol_table_lock;
};
#endif

#endif

