#ifndef __NB_MAP_H__
#define __NB_MAP_H__


#define MAP_PREFIX	"map"
#define MAP_N_ELEMENTS (1 << 2)

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
	vector port_list[MAP_N_PORTS];

	/** 
	  * APPL set for current map. Attentions: all applications in one map have "OR" relationship.
	How Flow Mapping handles a case where a packet matches multiple applications in the same map?
	In cases like this, the packet is sent to all configured
destinations when the first pass application is matched (assuming there were no matching
drop rules ¨C drop rules have higher priority)
	For example, a confiigured map named "Pass_HTTP" within 3 applications below:
	appl1= "Source Address=192.168.1.1";
	appl2= "Source Port=80";
	appl3= "VLAN=100".
	In this case, when a packet whose "Source Address=192.168.1.1" && "Source Port=80" && "VLAN=100"
will be send once to destinations.
	If you only want to send packet to one destiation, 
just creating an application with combind criterias like below and added it to map.
	applx= "Source Address=192.168.1.1, Source Port=80, VLAN=100"
	*/
	vector appl_set;

	/**
	  * A copy of User-Defined Pattern from UDP Management.
	  */
	vector udp_set;
	
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


struct map_table_t {
	vector mt_entry;
	u32 entries;
	os_lock_t  ol_table_lock;
};

int
map_init (void);

int
map_entry_new (struct map_t **map, char *alias, char *from, char __oryx_unused__ *to);

int
map_table_entry_add (struct map_t *map);

int
map_table_entry_remove (struct map_t *map);

struct map_t *
map_table_entry_lookup_i (u32 id);

void
map_table_entry_exact_lookup (char *alias, struct map_t **map);

int
map_entry_add_udp (struct udp_t *udp, struct map_t *map);

int
map_entry_remove_udp (struct udp_t *udp, struct map_t *map);

void
map_table_entry_remove_and_destroy (struct map_t *map);

void map_entry_add_udp_to_drop_quat (struct udp_t *udp, struct map_t *map);
void map_entry_add_udp_to_pass_quat (struct udp_t *udp, struct map_t *map);

#if defined(HAVE_QUAGGA)

extern atomic_t n_map_elements;

#define split_foreach_map_func1_param0(argv_x, func) {\
	char *split = ",";/** split tokens */\
	char *token = NULL;\
	char *save = NULL;\
	char alias_list[128] = {0};\
	int foreach_element;\
	vector vec = map_vector_table;\
	u32 elements_before = vector_count(vec);\
	elements_before = elements_before;\
	atomic_set(&n_map_elements, 0);\
	struct map_t *v = NULL;\
	if (!strcmp (alias_list, "*")) {\
		/** lookup alias with Post-Fuzzy match */\
		vector_foreach_element(vec, foreach_element, v){\
			if (v){\
				func (v);\
				atomic_inc(&n_map_elements);\
			}\
		}\
	}  else {\
		memcpy (alias_list, (char *)argv_x, strlen ((char *)argv_x));\
		token = strtok_r (alias_list, split, &save);\
		while (token) {\
			if (token) {\
				v = NULL;\
				if (isalldigit (token)) {\
					/** Lookup by ID. */\
					v = map_table_entry_lookup_i (atoi(token));\
					if (!v) {\
						goto lookup_by_alias_exactly;\
					}\
				} else {\
					/** Lookup by ALIAS fuzzy. */\
					if (strchr (token, '*')){\
						goto lookup_by_alias_posted_fuzzy;\
					}\
					else {\
	lookup_by_alias_exactly:\
						/** Lookup by ALIAS exactly. */\
						map_table_entry_exact_lookup (token, &v);\
						if (unlikely(!v)) {\
							goto lookup_next;\
						}\
					}\
				}\
				if (v) {\
					func (v);\
					atomic_inc(&n_map_elements);\
					goto lookup_next;\
				}\
	lookup_by_alias_posted_fuzzy:\
				/** lookup alias with Post-Fuzzy match */\
				vector_foreach_element(vec, foreach_element, v){\
					if (v && !strncmp (v->sc_alias, token, (strlen(token) - 1/** ignore '*'  */))){\
						func (v);\
						atomic_inc(&n_map_elements);\
					}\
				}\
			}\
	lookup_next:\
			token = strtok_r (NULL, split, &save);\
		}\
	}\
}

#define split_foreach_map_func1_param1(argv_x, func, param0) {\
	char *split = ",";/** split tokens */\
	char *token = NULL;\
	char *save = NULL;\
	char alias_list[128] = {0};\
	int foreach_element;\
	vector vec = map_vector_table;\
	u32 elements_before = vector_count(vec);\
	elements_before = elements_before;\
	atomic_set(&n_map_elements, 0);\
	struct map_t *v = NULL;\
	if (!strcmp (alias_list, "*")) {\
		/** lookup alias with Post-Fuzzy match */\
		vector_foreach_element(vec, foreach_element, v){\
			if (v){\
				func (v, param0);\
				atomic_inc(&n_map_elements);\
			}\
		}\
	}  else {\
		memcpy (alias_list, (char *)argv_x, strlen ((char *)argv_x));\
		token = strtok_r (alias_list, split, &save);\
		while (token) {\
			if (token) {\
				v = NULL;\
				if (isalldigit (token)) {\
					/** Lookup by ID. */\
					v = map_table_entry_lookup_i (atoi(token));\
					if (!v) {\
						goto lookup_by_alias_exactly;\
					}\
				} else {\
					/** Lookup by ALIAS fuzzy. */\
					if (strchr (token, '*')){\
						goto lookup_by_alias_posted_fuzzy;\
					}\
					else {\
	lookup_by_alias_exactly:\
						/** Lookup by ALIAS exactly. */\
						map_table_entry_exact_lookup (token, &v);\
						if (unlikely(!v)) {\
							goto lookup_next;\
						}\
					}\
				}\
				if (v) {\
					func (v, param0);\
					atomic_inc(&n_map_elements);\
					goto lookup_next;\
				}\
	lookup_by_alias_posted_fuzzy:\
				/** lookup alias with Post-Fuzzy match */\
				vector_foreach_element(vec, foreach_element, v){\
					if (v && !strncmp (v->sc_alias, token, (strlen(token) - 1/** ignore '*'  */))){\
						func (v, param0);\
						atomic_inc(&n_map_elements);\
					}\
				}\
			}\
	lookup_next:\
			token = strtok_r (NULL, split, &save);\
		}\
	}\
}


#define split_foreach_map_func1_param2(argv_x, func, param0, param_1)\
	char *split = ",";/** split tokens */\
	char *token = NULL;\
	char *save = NULL;\
	char alias_list[128] = {0};\
	int foreach_element;\
	vector vec = map_vector_table;\
	u32 elements_before = vector_count(vec);\
	elements_before = elements_before;\
	atomic_set(&n_map_elements, 0);\
	struct map_t *v = NULL;\
	if (!strcmp (alias_list, "*")) {\
		/** lookup alias with Post-Fuzzy match */\
		vector_foreach_element(vec, foreach_element, v){\
			if (v){\
				func (v, param0, param_1);\
				atomic_inc(&n_map_elements);\
			}\
		}\
	}  else {\
		memcpy (alias_list, (char *)argv_x, strlen ((char *)argv_x));\
		token = strtok_r (alias_list, split, &save);\
		while (token) {\
			if (token) {\
				v = NULL;\
				if (isalldigit (token)) {\
					/** Lookup by ID. */\
					v = map_table_entry_lookup_i (atoi(token));\
					if (!v) {\
						goto lookup_by_alias_exactly;\
					}\
				} else {\
					/** Lookup by ALIAS fuzzy. */\
					if (strchr (token, '*')){\
						goto lookup_by_alias_posted_fuzzy;\
					}\
					else {\
		lookup_by_alias_exactly:\
						/** Lookup by ALIAS exactly. */\
						map_table_entry_exact_lookup (token, &v);\
						if (unlikely(!v)) {\
							goto lookup_next;\
						}\
					}\
				}\
				if (v) {\
					func (v, param0, param_1);\
					atomic_inc(&n_map_elements);\
					goto lookup_next;\
				}\
		lookup_by_alias_posted_fuzzy:\
				/** lookup alias with Post-Fuzzy match */\
				vector_foreach_element(vec, foreach_element, v){\
					if (v && !strncmp (v->sc_alias, token, (strlen(token) - 1/** ignore '*'  */))){\
						func (v, param0, param_1);\
						atomic_inc(&n_map_elements);\
					}\
				}\
			}\
		lookup_next:\
			token = strtok_r (NULL, split, &save);\
		}\
	}\
}

#define foreach_map_func1_param0(argv_x, func)\
	int foreach_element;\
	vector vec = map_vector_table;\
	u32 elements_before = vector_count(vec);\
	elements_before = elements_before;\
	atomic_set(&n_map_elements, 0);\
	struct map_t *v;\
	vector_foreach_element(vec, foreach_element, v){\
		if (v) {\
			func (v);\
			atomic_inc(&n_map_elements);\
		}\
	}

#define foreach_map_func1_param1(argv_x, func, param0)\
	int foreach_element;\
	vector vec = map_vector_table;\
	u32 elements_before = vector_count(vec);\
	elements_before = elements_before;\
	atomic_set(&n_map_elements, 0);\
	struct map_t *v;\
	vector_foreach_element(vec, foreach_element, v){\
		if (v) {\
			func (v, param0);\
			atomic_inc(&n_map_elements);\
		}\
	}

#endif
#endif
