#include "oryx.h"

#include "et1500.h"
#if defined(HAVE_DPDK)
#include "dpdk.h"
#endif


#define MAP_DEBUG

/** fast tracinging. unused actually. */
struct map_t *lowest_map;
struct map_t *highest_map;

DECLARE_TABLE(traffic_map);
LIST_HEAD_DECLARE(map_priority_list);
INIT_MUTEX(map_priority_lock);

#define MAP_LOCK
#ifdef MAP_LOCK
INIT_MUTEX(map_lock);
#else
#undef do_lock(lock)
#undef do_unlock(lock)
#define do_lock(lock)
#define do_unlock(lock)
#endif

/** Hash table is only used for CONTROLPLANE. */
struct oryx_htable_t *map_hash_table;

u32 vector_runtime;
vector map_vector_table;
vector map_vector_tables[VECTOR_TABLES];


#define VTY_ERROR_MAP(prefix, alias)\
	vty_out (vty, "%s(Error)%s %s map \"%s\"%s", \
		draw_color(COLOR_RED), draw_color(COLOR_FIN), prefix, alias, VTY_NEWLINE)

#define VTY_SUCCESS_MAP(prefix, v)\
	vty_out (vty, "%s(Success)%s %s map \"%s\"(%u)%s", \
		draw_color(COLOR_GREEN), draw_color(COLOR_FIN), prefix, v->sc_alias, v->ul_id, VTY_NEWLINE)

static void mpm_cleanup (struct map_t *map)
{

	if (!map) return;

#if 0
	/** MPM_TABLEx is destroyed after mpm_runtime_ctx switched. 
		So, here, we just Destroy mpm_runtime_ctx. */
	u8 table;
	
	table = MPM_TABLE0;
	MpmDestroyCtx (&map->mpm_ctx[table]);

	
	table = MPM_TABLE1;
	MpmDestroyCtx (&map->mpm_ctx[table]);
#endif

	MpmDestroyThreadCtx (map->mpm_runtime_ctx, &map->mpm_thread_ctx);
	PmqFree(&map->pmq);
}

static void map_entry_add_port (struct port_t *port, struct map_t *map, u8 from_to)
{
	vector v;

	/** Map.Port */
	v = map->port_list[from_to % MAP_N_PORTS];
	vector_set_index (v, port->ul_id, (void *)port);
	
	/** Port.Map */
	v = port->belong_maps;
	vector_set_index (v, map->ul_id, (void *)map);

	printf ("%s -> %s, and now %d maps ->>> ", port->sc_alias, map->sc_alias, vector_active (port->belong_maps));
	int i = 0;
	struct map_t *m;
	v = port->belong_maps;
	vector_foreach_element (v, i, m) {
		printf ("%s ", m->sc_alias);
	}

	printf ("\n");
	
}

static void map_entry_remove_port (struct port_t *port, struct map_t *map, u8 from_to)
{
	vector v;
	
	v = map->port_list[from_to % MAP_N_PORTS];
	/** Map.Port. */
	vector_unset (v, port->ul_id);
	/** Port.Map */
	vector_unset (port->belong_maps, map->ul_id);
}

static int map_entry_split_application (struct map_t *map,
				   char *text, size_t s)
{

	/** Split Tokens */
	char *split = ",";
	char *token = NULL;
	char *save = NULL;
	vector v = map->appl_set;
	char src[64] = {0};
	
	memcpy (src, text, s);
	
	token = strtok_r (src, split, &save);
	while (token) {
		if (token) {
			struct appl_t *appl;
			appl_table_entry_lookup (token, &appl);
			if (likely(appl)) {
				if (v) vector_set_index (v, appl->ul_id, (void *)appl);
			}
		}
		token = strtok_r (NULL, split, &save);
	};

	return 0;

}

static __oryx_always_inline__
void base_action_iterator (char *action_str, u8 *action)
{
	if (action_str) {
		if (!strcmp (action_str, "pass")) *action |= CRITERIA_FLAGS_PASS;
		if (!strcmp (action_str, "drop")) *action &= ~CRITERIA_FLAGS_PASS;
	}
}

static __oryx_always_inline__
void advanced_action_iterator (char *action_str, u8 *action)
{
	if (action_str) {
		if (!strcmp (action_str, "timestamp")) *action |= CRITERIA_FLAGS_TIMESTAMP;
		if (!strcmp (action_str, "slicing")) *action |= CRITERIA_FLAGS_SLICING;
	}
}

static __oryx_always_inline__
int vector_has_this_udp (vector v, struct udp_t *udp)
{

	int foreach;
	struct udp_t *u;
	
	vector_foreach_element (v, foreach, u) {
		if (!u) continue;
		else {
			if (u->ul_id  == udp->ul_id) {
				return 1;
			}
			else continue;
		}
	}

	return 0;
}

static __oryx_always_inline__
int vector_has_this_appl (vector v, struct appl_t *appl)
{

	int foreach;
	struct appl_t *u;

	vector_foreach_element (v, foreach, u) {
		if (!u) continue;
		else {
			if (u->ul_id  == appl->ul_id) {
				return 1;
			}
			else continue;
		}
	}

	return 0;
}


int map_entry_new (struct map_t **map, char *alias, char *from, char *to)
{
	u8 table = MPM_TABLE0;

	ASSERT (alias);
	ASSERT (from);
	ASSERT (to);

	/** create an appl */
	(*map) = kmalloc (sizeof (struct map_t), MPF_CLR, __oryx_unused_val__);
	ASSERT ((*map));

	(*map)->ull_create_time = time(NULL);
	(*map)->ul_mpm_has_been_setup = 0;
	(*map)->uc_mpm_type = mpm_default_matcher;

	INIT_LIST_HEAD(&(*map)->prio_node);

	/** Init mpm feature. **/	
	MpmInitCtx(&(*map)->mpm_ctx[table], (*map)->uc_mpm_type);

	/** called after adding patterns. */
	/** MpmInitThreadCtx (&(*map)->mpm_thread_ctx, (*map)->uc_mpm_type); */
	PmqSetup(&(*map)->pmq);
	
	(*map)->mpm_runtime_ctx = &(*map)->mpm_ctx[table];
	printf ("mpmtable0 ->> %p, runtime ->> %p%s", 
		&(*map)->mpm_ctx[table], (*map)->mpm_runtime_ctx, "\n");
	
	(*map)->udp_set = vector_init (1);
	(*map)->appl_set = vector_init (1);
	(*map)->ul_flags = MAP_TRAFFIC_TRANSPARENT;
	
	/** make alias. */
	sprintf ((char *)&(*map)->sc_alias[0], "%s", ((alias != NULL) ? alias: MAP_PREFIX));

	(*map)->port_list[MAP_N_PORTS_FROM] = vector_init (PANEL_N_PORTS);
	(*map)->port_list[MAP_N_PORTS_TO] = vector_init (PANEL_N_PORTS);
	
	(*map)->port_list_str[MAP_N_PORTS_FROM] = strdup (from);
	(*map)->port_list_str[MAP_N_PORTS_TO] = strdup(to);
	/** Need Map's ul_id, so have to remove map_entry_split_and_mapping_port to map_table_entry_add. */

	oryx_thread_mutex_create(&(*map)->ol_lock);

	return 0;
}

__oryx_always_extern__
void map_entry_destroy (struct map_t *map)
{
	vector_free (map->port_list[MAP_N_PORTS_FROM]);
	vector_free (map->port_list[MAP_N_PORTS_TO]);
	vector_free (map->appl_set);
	vector_free (map->udp_set);
	mpm_cleanup (map);
	kfree (map);
}

__oryx_always_extern__
int map_entry_add_appl (struct appl_t __oryx_unused__*appl, struct map_t __oryx_unused__*map)
{

	int xret = 0;
	vector v = map->appl_set;
	
	xret = vector_has_this_appl (v, appl);
	
	if (xret < 0 /** critical error */)
		xret  = -1;
	else {
		if (xret == 0 /** no such element. */)
			if (likely(v)) {
				printf ("installing ... %s  ", appl_alias(appl));
				vector_set_index (v, appl->ul_id, appl);
				printf ("done \n");
			}
	}

	return xret;
}

__oryx_always_extern__
int map_entry_remove_appl (struct appl_t *appl, struct map_t *map)
{

	int xret = 0;
	vector v = map->appl_set;
	
	xret = vector_has_this_appl (v, appl);
	
	if (xret < 0 /** critical error */)
		xret  = -1;
	else {
		if (xret == 1 /** find same element. */)
			if (likely(v)) {
				printf ("uninstalling ... %s  ", appl_alias(appl));
				vector_unset (v, appl->ul_id);
				printf ("done \n");
			}
	}

	return xret;
}


/** add a user defined pattern to existent Map */
__oryx_always_extern__
int map_entry_add_udp (struct udp_t *udp, struct map_t *map)
{

	int xret = 0;
	vector v = map->udp_set;
	
	xret = vector_has_this_udp (v, udp);
	
	if (xret < 0 /** critical error */)
		xret  = -1;
	else {
		if (xret == 0 /** no such element. */)
			if (likely(v)) {
				printf ("installing ... %s  ", udp_alias(udp));
				vector_set_index (v, udp->ul_id, udp);
				printf ("done(%d)\n", vector_count(v));
			}
	}
	return xret;
}


__oryx_always_extern__
int map_entry_remove_udp (struct udp_t *udp, struct map_t *map)
{

	int xret = 0;
	vector v = map->udp_set;
	
	xret = vector_has_this_udp (v, udp);
	
	if (xret < 0 /** critical error */)
		xret  = -1;
	else {
		if (xret == 1 /** find same element. */)
			if (likely(v)) {
				printf ("uninstalling ... %s  ", udp_alias(udp));
				vector_unset (v, udp->ul_id);
				printf ("done(%d)\n", vector_count(v));
			}
	}

	return xret;
}

/** Must called after map_entry_add_udp or map_entry_remove_udp. */
__oryx_always_extern__
int map_entry_install_and_compile_udp (struct map_t *map)
{
	int i, j;
	u32 ul_pid = -1;
	u32 ul_sid;	
	MpmCtx *mpm_ctx;
	MpmThreadCtx *mpm_threadctx;
	
	/** Swap. */
	mpm_ctx = &map->mpm_ctx[++map->mpm_index%MPM_TABLES];
	mpm_threadctx = &map->mpm_thread_ctx;

	/** Prepare mpmctx before add pattern or compile it. */
	memset(mpm_ctx, 0x00, sizeof(MpmCtx));
	MpmInitCtx (mpm_ctx, map->uc_mpm_type /** default is MPM_AC. */);

#if 0
	/** Make sure that there are some udps to install this time.  */
	if (vector_count(map->udp) <= 0) {
		return 0;
	}
#endif

	struct udp_t *u;
	
	vector_foreach_element(map->udp_set, i, u) {
		if (!u) continue;
		
		struct pattern_t *p;
		vector_foreach_element(u->patterns, j, p) {
			if (!p) continue;

			ul_pid = mkpid (map->ul_id, u->ul_id, p->ul_id);
			/** overwrite. */
			ul_sid = mksid (map->ul_id, u->ul_id, p->ul_id);
			printf ("controlpalne ->  u->ul_id =%u, p->ul_id =%u, mksid =%u\n", u->ul_id, p->ul_id, ul_sid);
			
			if (p->ul_flags & PATTERN_FLAGS_NOCASE)
				MpmAddPatternCI(mpm_ctx, (uint8_t *)&p->sc_val[0], p->ul_size, 0, 0, ul_pid, ul_sid, 0);
			else
				MpmAddPatternCS(mpm_ctx, (uint8_t *)&p->sc_val[0], p->ul_size, 0, 0, ul_pid, ul_sid, 0);	
		}
	}
	
	/** Recompile MPM table by calling MpmPreparePatterns. */
	MpmPreparePatterns (mpm_ctx);
	MpmInitThreadCtx(mpm_threadctx, map->uc_mpm_type);
	
	return 0;
}


__oryx_always_extern__
void map_entry_add_udp_to_pass_quat (struct udp_t *udp, struct map_t *map)
{
	u8 quat;
	vector quat_vec;
	
	quat = QUA_PASS;
	quat_vec = udp->qua[quat];

	vector_set_index (quat_vec, map_slot(map), map);
}

__oryx_always_extern__
void map_entry_add_udp_to_drop_quat (struct udp_t *udp, struct map_t *map)
{
	u8 quat;
	vector quat_vec;
	
	quat = QUA_DROP;
	quat_vec = udp->qua[quat];

	vector_set_index (quat_vec, map_slot(map), map);
}

__oryx_always_extern__
void map_entry_setup_udp_quat (struct udp_t *udp, u8 quat, struct map_t *map)
{
	vector quat_vec = NULL;

	switch (quat) {
		case QUA_PASS:
			quat_vec = udp->qua[QUA_PASS];
			break;
		case QUA_DROP:
			quat_vec = udp->qua[QUA_DROP];
		default:
			ASSERT(0);
	}

	vector_set_index (quat_vec, map_slot(map), map);
}

static __oryx_always_inline__
void map_entry_unsetup_udp_quat (struct udp_t *udp, struct map_t *map)
{

	u8 quat;
	vector quat_vec;
	
	quat = QUA_DROP;
	quat_vec = udp->qua[quat];
	vector_unset (quat_vec, map_slot(map));


	quat = QUA_PASS;
	quat_vec = udp->qua[quat];
	vector_unset (quat_vec, map_slot(map));

}

static __oryx_always_inline__
void map_entry_split_and_mapping_port (struct map_t *map, u8 from_to)
{
	split_foreach_port_func1_param2 (
		map->port_list_str[from_to], 
		map_entry_add_port, map, from_to);
}

static __oryx_always_inline__
void map_entry_split_and_unmapping_port (struct map_t *map, u8 from_to)
{
	split_foreach_port_func1_param2 (
		map->port_list_str[from_to], 
		map_entry_remove_port, map, from_to);
}

int map_table_entry_add (struct map_t *map)
{

	int r = 0;

	do_lock (&map_lock);
	
	r = oryx_htable_add(map_hash_table, 
		map_alias(map), strlen((const char *)map_alias(map)));

	/** Add alias to hash table for fast lookup by map alise. */
	if (r == 0 /** success */) {
		u8 from_to;

		/** 
		  * Add map to vector. 
		  * Must be called before split and (un)mapping port to this map entry.
		  */
		map_slot(map) = vector_set (map_vector_table, map);
		
		from_to = MAP_N_PORTS_FROM;
		if (map->port_list_str[from_to]) {
			map_entry_split_and_mapping_port (map, from_to);
		}

		/** MAP_N_PORTS_TO may should not be concerned. Discard it. */
		from_to = MAP_N_PORTS_TO;
		if (map->port_list_str[from_to]) {
			map_entry_split_and_mapping_port (map, from_to);
		}

		highest_map = vector_first (map_vector_table);
		lowest_map = vector_last (map_vector_table);

		/** Add this map to priority list */
		list_add_tail (&map->prio_node, &map_priority_list);
		
	}

	do_unlock (&map_lock);
	
	return r;
}

int map_table_entry_remove (struct map_t *map)
{

	 int r = 0;

	do_lock (&map_lock);
	
	/** Delete alias from hash table. */
	r = oryx_htable_del(map_hash_table, map->sc_alias, strlen((const char *)map->sc_alias));

	if (r == 0 /** success */) {

		/** dataplane may using the map, unmap port as soon as possible. */
		u8 from_to;

		from_to = MAP_N_PORTS_FROM;
		if (map->port_list_str[from_to]) {
			map_entry_split_and_unmapping_port (map, from_to);
		}

		/** MAP_N_PORTS_TO may should not be concerned. Discard it. */
		from_to = MAP_N_PORTS_TO;
		if (map->port_list_str[from_to]) {
			map_entry_split_and_unmapping_port (map, from_to);
		}
		
		/** Delete map from vector */
		vector_unset (map_vector_table, map->ul_id);
		highest_map = vector_first (map_vector_table);
		lowest_map = vector_last (map_vector_table);

		/** Delete this map from priority list */
		list_del (&map->prio_node);
	}

	do_unlock (&map_lock);
	
	return r;
}

void map_table_entry_remove_and_destroy (struct map_t *map)
{

	if (!map) return;

	map_table_entry_remove (map);
	map_entry_destroy(map);
}


void map_table_entry_exact_lookup (char *alias, struct map_t **map)
{
	(*map) = NULL;
	
	if (!alias) return;

	void *s = oryx_htable_lookup (map_hash_table,
		alias, strlen((const char *)alias));

	if (s) {
		(*map) = (struct map_t *) container_of (s, struct map_t, sc_alias);
	}
}

struct map_t *map_table_entry_lookup_i (u32 id)
{
	return (struct map_t *) vector_lookup (map_vector_table, id);
}


struct map_table_t map_table = {
	.ol_table_lock = INIT_MUTEX_VAL,
	.mt_entry = NULL,
	.entries = 0,
};

#if defined(HAVE_QUAGGA)
#include "vty.h"
#include "command.h"
#include "prefix.h"
#endif

static __oryx_always_inline__
void map_free (void __oryx_unused__ *v)
{
	/** Never free here! */
	v = v;
}

static uint32_t
map_hval (struct oryx_htable_t *ht,
		void *v, uint32_t s) 
{
     uint8_t *d = (uint8_t *)v;
     uint32_t i;
     uint32_t hv = 0;

     for (i = 0; i < s; i++) {
         if (i == 0)      hv += (((uint32_t)*d++));
         else if (i == 1) hv += (((uint32_t)*d++) * s);
         else             hv *= (((uint32_t)*d++) * i) + s + i;
     }

     hv *= s;
     hv %= ht->array_size;
     
     return hv;
}

static int
map_cmp (void *v1, 
		uint32_t s1,
		void *v2,
		uint32_t s2)
{
	int xret = 0;

	if (!v1 || !v2 ||s1 != s2 ||
		memcmp(v1, v2, s2))
		xret = 1;

	return xret;
}

#if defined(HAVE_QUAGGA)

static void map_entry_output (struct map_t *map,  struct vty *vty)
{

	vector v;
	
	ASSERT (map);

	{		
		char tmstr[100];
		tm_format (map->ull_create_time, "%Y-%m-%d,%H:%M:%S", (char *)&tmstr[0], 100);

		/** let us try to find the map which name is 'alias'. */
		printout (vty, "%16s\"%s\"(%u)		%s%s", "Map ", map->sc_alias, map->ul_id, tmstr, VTY_NEWLINE);

		printout (vty, "		%16s", "Ports: ");
		v = map->port_list[MAP_N_PORTS_FROM];
		int actives;

		actives = vector_active(v);
		if (!actives) {printout (vty, "N/A");}
		else {
			int i;
			struct port_t *p;
			vector_foreach_element (v, i, p) {
				if (p) {
					printout (vty, "%s", p->sc_alias);
					-- actives;
					if (actives) printout (vty, ", ");
					else actives = actives;
				}
				else {p = p;}
			}
			//printout (vty, "%s", VTY_NEWLINE);
		}

		printout (vty, "%s", "  ---->  ");
		v = map->port_list[MAP_N_PORTS_TO];
		actives = vector_active(v);
		if (!actives) printout (vty, "N/A%s", VTY_NEWLINE);
		else {
			int i;
			struct port_t *p;
			vector_foreach_element (v, i, p) {
				if (p) {
					if (p->ul_flags & NB_INTF_FLAGS_LOOPBACK)
						printout (vty, "%s(%s%s%s)", p->sc_alias, 
							draw_color(COLOR_RED), 
							"loopback",
							draw_color(COLOR_FIN));
					else
						printout (vty, "%s", p->sc_alias);
					-- actives;
					if (actives) printout (vty, ", ");
					else actives = actives;
				}
				else {p = p;}
			}
			printout (vty, "%s", VTY_NEWLINE);
		}

		/** Caculate passed an dropped. */
		int passed = 0, dropped = 0;

		
		v = map->appl_set;
		actives = vector_active(v);
		if (actives) {
			int i;
			struct appl_t *a;
			vector_foreach_element (v, i, a) {
				if (a) {
					if (a->ul_flags & CRITERIA_FLAGS_PASS) passed ++;
					else dropped ++;
				}
				else { a = a;}
			}
		}

		printout (vty, "		%16s%s, %s %s", "States: ", 
			(!passed && !dropped) ? ((map->ul_flags & MAP_TRAFFIC_TRANSPARENT) ? "transparent" : "blocked") : "by Criteria(s)", 
			"hash(sdip+sdp+protocol+vlan)",
			VTY_NEWLINE);
		
		printout (vty, "		%16s%d appl(s)%s", "Pass: ", passed, VTY_NEWLINE);
		printout (vty, "		%16s%d appl(s)%s", "Drop: ", dropped, VTY_NEWLINE);

		/** 
		  *   Actually, we may have defined number of applications, 
		  *   but here for now, we are trying to display stream application only.
		  */
		v = map->appl_set;
		actives = vector_active(v);
		if (!actives) printout (vty, "		%16s%s%s", "Appl(s): ", "N/A", VTY_NEWLINE);
		else {
			int i;
			printout (vty, "		%16s%s", "Appl(s): ", VTY_NEWLINE);
			struct appl_t *a;
			vector_foreach_element (v, i, a) {
				if (a) {
					printout (vty, "		     %16s (%s)%s", a->sc_alias, 
						(a->ul_flags & CRITERIA_FLAGS_PASS) ? "pass" : "drop", VTY_NEWLINE);
				}
				else { a = a;}
			}
			printout (vty, "%s", VTY_NEWLINE);
		}

		v = map->udp_set;
		actives = vector_active(v);
		if (!actives) printout (vty, "		%16s%s%s", "UDP(s): ", "N/A", VTY_NEWLINE);
		else {
			int i;
			printout (vty, "		%16s%s", "Udp(s): ", VTY_NEWLINE);
			struct udp_t *a;
			vector_foreach_element (v, i, a) {
				if (a) {
					vector v0, v1;

					v0 = a->qua[QUA_DROP];
					v1 = a->qua[QUA_PASS];
					if (vector_lookup (v0, map->ul_id)) {
						printout (vty, "		     %16s (%s)%s", a->sc_alias, 
							"drop", VTY_NEWLINE);
					}
					if (vector_lookup (v1, map->ul_id)) {
						printout (vty, "		     %16s (%s)%s", a->sc_alias, 
							"pass", VTY_NEWLINE);
					}
				}
				else { a = a;}
			}
			printout (vty, "%s", VTY_NEWLINE);
		}
		
		printout (vty, "%16s%s", "-------------------------------------------", VTY_NEWLINE);
		
	}
}

atomic_t n_map_elements = ATOMIC_INIT(0);
#define PRINT_SUMMARY	\
	printout (vty, "matched %d element(s), %d element(s) actived.%s", \
		atomic_read(&n_map_elements), (int)vector_count(map_vector_table), VTY_NEWLINE);

DEFUN(show_map,
      show_map_cmd,
      "show map [WORD]",
      SHOW_STR SHOW_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR)
{

	printout (vty, "Trying to display %s%d%s elements ...%s", 
		draw_color(COLOR_RED), vector_active(map_vector_table), 	draw_color(COLOR_FIN), 
		VTY_NEWLINE);
	
	printout (vty, "%16s%s", "-------------------------------------------", VTY_NEWLINE);

	if (argc == 0){
		foreach_map_func1_param1 (argv[0],
				map_entry_output, vty)
	}
	else {
		split_foreach_map_func1_param1 (argv[0],
				map_entry_output, vty);
	}

	PRINT_SUMMARY;

    return CMD_SUCCESS;
}

DEFUN(no_map,
      no_map_cmd,
      "no map [WORD]",
      NO_STR
      NO_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR)
{

	if (argc == 0) {
		foreach_map_func1_param0 (argv[0],
				map_table_entry_remove_and_destroy);
	}
	else {
		split_foreach_map_func1_param0 (argv[0],
				map_table_entry_remove_and_destroy);
	}

	/** reset lowest priority map and highest priority map. */
	lowest_map = highest_map = map_vector_table->index[0];

	PRINT_SUMMARY;
	
    return CMD_SUCCESS;
}

DEFUN(new_map,
      new_map_cmd,
      "map WORD from port PORTS to port PORTS",
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR)
{

	struct map_t *map;

	map_table_entry_exact_lookup ((char *)argv[0], &map);
	if (likely(map)) {
		VTY_SUCCESS_MAP ("existent", map);
		map_entry_output (map, vty);
	} else {
		map_entry_new (&map, (char *)argv[0], (char *)argv[1], (char *)argv[2]);
		if (unlikely (!map)) {
			VTY_ERROR_MAP ("new", (char *)argv[0]);
			return CMD_SUCCESS;
		}
		if (!map_table_entry_add (map)) {
			VTY_SUCCESS_MAP ("new", map);
		}
	}
			
    return CMD_SUCCESS;
}

static void map_install_udp (struct vty *vty, 
	char __oryx_unused__ *pass_drop, char *alias, struct map_t *map)
{

	void *backup_mpm_ptr;
	u8 do_swap = 0;

	void (*fn)(struct udp_t *udp, struct map_t *map) = \
					&map_entry_add_udp_to_drop_quat;

	if (!strncmp (pass_drop, "p", 1)) {
		fn = &map_entry_add_udp_to_pass_quat;	
	}
	
	split_foreach_udp_func2_param1 (alias,
			map_entry_add_udp, map, fn, map);

	backup_mpm_ptr = map->mpm_runtime_ctx;
	
	map_entry_install_and_compile_udp (map);

	do_swap = 1;
	
	if (do_swap) {
		
		/** Critical region started. */
		do_lock (map->ol_lock);
		/** Stop dataplane string match routine. */
		map->ul_mpm_has_been_setup = 0;
		/** vector_runtime and map_vector_table remapping must be protected by lock. */
		map->mpm_runtime_ctx  = &map->mpm_ctx[map->mpm_index%MPM_TABLES];
		/** Start dataplane string match rountine. */
		map->ul_mpm_has_been_setup = 1;
		do_unlock (map->ol_lock);

		/** After swapped, cleanup previous mpmctx. */
		MpmDestroyCtx (&map->mpm_ctx[(map->mpm_index + 1)%MPM_TABLES]);
	}

	vty_out (vty, "%d, %p -->  %p%s", vector_count(map->udp_set), backup_mpm_ptr, map->mpm_runtime_ctx, VTY_NEWLINE);

}

static void map_uninstall_udp (struct vty *vty, char *alias, struct map_t *map)
{
	void *backup_mpm_ptr;
	u8 do_swap = 0;

	split_foreach_udp_func2_param1 (alias,
			map_entry_remove_udp, map, 
			map_entry_unsetup_udp_quat, map);

	/** Make sure that there are some udps to install this time.  */
	if (vector_count(map->udp_set) <= 0) {
		map->ul_mpm_has_been_setup = 0;
		return;
	}

	backup_mpm_ptr = map->mpm_runtime_ctx;
	
	map_entry_install_and_compile_udp (map);

	do_swap = 1;
	
	if (do_swap) {
		
		/** Critical region started. */
		do_lock (&map_lock);
		/** Stop dataplane string match routine. */
		map->ul_mpm_has_been_setup = 0;
		/** vector_runtime and map_vector_table remapping must be protected by lock. */
		map->mpm_runtime_ctx  = &map->mpm_ctx[map->mpm_index%MPM_TABLES];
		/** Start dataplane string match rountine. */
		map->ul_mpm_has_been_setup = 1;
		do_unlock (&map_lock);

		/** After swapped, cleanup previous mpmctx. */
		MpmDestroyCtx (&map->mpm_ctx[(map->mpm_index + 1)%MPM_TABLES]);
	}

	vty_out (vty, "%d, %p -->  %p%s", vector_count(map->udp_set), backup_mpm_ptr, map->mpm_runtime_ctx, VTY_NEWLINE);
}


DEFUN(map_application_udp,
      map_application_udp_cmd,
      "map WORD add (pass|drop) (application|udp) WORD",
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR)
{

	struct map_t *map;
	
	map_table_entry_exact_lookup ((char *)argv[0], &map);
	if (unlikely(!map)) {
		VTY_ERROR_MAP ("non-existent", (char *)argv[0]);
		return CMD_SUCCESS;
	}

	if (!strncmp (argv[2], "u", 1)) {
		map_install_udp (vty, (char *)argv[1], (char *)argv[3], map);
	}else {

		split_foreach_application_func1_param1 (argv[3],
						map_entry_add_appl, map);
	}

	PRINT_SUMMARY;
	
    return CMD_SUCCESS;
}


DEFUN(no_map_application_udp,
      no_map_application_udp_cmd,
      "no map WORD (application|udp) WORD",
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR)
{

	struct map_t *map;
	
	map_table_entry_exact_lookup ((char *)argv[0], &map);
	if (unlikely(!map)) {
		VTY_ERROR_MAP ("non-existent", (char *)argv[0]);
		return CMD_SUCCESS;
	}

	if (!strncmp (argv[1], "u", 1)) {
		map_uninstall_udp (vty, (char *)argv[2], map);
	}else {
		split_foreach_application_func1_param1 (argv[2],
				map_entry_remove_appl, map);
	}

	PRINT_SUMMARY;
	
     return CMD_SUCCESS;
}


static void map_priority_show (struct vty *vty)
{

	struct map_t *map = NULL, *p;
	list_for_each_entry_safe(map, p, &map_priority_list, prio_node){
		printout (vty, "%s, ", map_alias(map));
	}

	printout (vty, "%s", VTY_NEWLINE);
	printout (vty, "%s", VTY_NEWLINE);

}

DEFUN(map_adjusting_priority,
      map_adjusting_priority_cmd,
      "map WORD priority (after|before) map WORD",
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR)
{
	/** All map in priority list can be reload to backup vector */
	int i;
	u8 do_swap = 0;
	vector vector_backup;
	struct map_t *map = NULL, *p;
	struct map_t *v1, *v2, *after, *before;
	
	map_table_entry_exact_lookup ((char *)argv[0], &v1);
	if (unlikely (!v1)) {
		VTY_ERROR_MAP ("non-existent", (char *)argv[0]);
		return CMD_SUCCESS;
	}

	map_table_entry_exact_lookup ((char *)argv[2], &v2);
	if (unlikely (!v2)) {
		VTY_ERROR_MAP ("non-existent", (char *)argv[2]);
		return CMD_SUCCESS;
	}

	after = before = v1;

	if (!strncmp (argv[1], "b", 1)) {
		after = v2;
	}else {
		before = v2;
	}

/**
	printout (vty, "before %s %s", map_alias(before), VTY_NEWLINE);
	printout (vty, "after %s %s", map_alias(after), VTY_NEWLINE);
	map_priority_show(vty);	
*/

	list_del (&after->prio_node);
	list_add (&after->prio_node, &before->prio_node);

	vector_backup = map_vector_tables[++vector_runtime%VECTOR_TABLES];
	
	/** Destroy all maps in backup vector's slots. */
	vector_foreach_element(vector_backup, i, map) {
		vector_unset (vector_backup, i);
	}

	/** Map to vector */
	list_for_each_entry_safe(map, p, &map_priority_list, prio_node) {
		/** Update slot automatically. */
		map_slot(map) = vector_set (vector_backup, map);
	}

	/** do swapping ??? */
	do_swap = 1;

	if (do_swap){
		/** Critical region started. */
		do_lock (&map_lock);
		/** vector_runtime and map_vector_table remapping must be protected by lock. */
		map_vector_table = map_vector_tables[vector_runtime%VECTOR_TABLES];
		do_unlock (&map_lock);
	}

	/** map_priority_show (vty); */

	return CMD_SUCCESS;
}

DEFUN(map_adjusting_priority_highest_lowest,
      map_adjusting_priority_highest_lowest_cmd,
      "map WORD priority (highest|lowest)",
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR)
{
	/** All map in priority list can be reload to backup vector */
	int i;
	u8 do_swap = 0;
	vector vector_backup;
	struct map_t *map = NULL, *p;
	struct map_t *v;
	
	map_table_entry_exact_lookup ((char *)argv[0], &v);
	if (unlikely (!v)) {
		VTY_ERROR_MAP ("non-existent", (char *)argv[0]);
		return CMD_SUCCESS;
	}

	/** */
	list_del (&v->prio_node);
	
	if (!strncmp (argv[1], "h", 1)) {
		/** Insert a new entry before the specified head */
		list_add (&v->prio_node, &map_priority_list);
	} else {
		/** Insert a new entry after the specified head */
		list_add_tail (&v->prio_node, &map_priority_list);
	}

	/** Critical region started. */
	vector_backup = map_vector_tables[++vector_runtime%VECTOR_TABLES];
	
	/** Destroy all maps in backup vector's slots. */
	vector_foreach_element(vector_backup, i, map) {
		vector_unset (vector_backup, i);
	}

	/** Map to vector */
	list_for_each_entry_safe(map, p, &map_priority_list, prio_node){
		/** Update slot automatically. */
		map_slot(map) = vector_set (vector_backup, map);
	}

	/** do swapping ??? */
	do_swap = 1;

	if (do_swap){
		/** vector_runtime and map_vector_table remapping must be protected by lock. */
		map_vector_table = map_vector_tables[vector_runtime%VECTOR_TABLES];
	}

	return CMD_SUCCESS;
}

#endif

#ifdef HAVE_DEFAULT_MAP
static struct map_t default_map = {
	.ul_flags = MAP_DEFAULT;
};
#endif

int map_init (void)
{
	/**
	 * Init MAP hash table, NAME -> MAP_ID.
	 */
	map_hash_table = oryx_htable_init(DEFAULT_HASH_CHAIN_SIZE, 
							map_hval, map_cmp, map_free, 0);
	printf ("[map_hash_table] htable init ");
	
	if (map_hash_table == NULL) {
		printf ("error!\n");
		goto finish;
	}
	printf ("okay\n");
	
	/**
	 * Init MAP table, MAP ID -> MAP INSTANCE.
	 */
	map_vector_tables[VECTOR_TABLE0] = vector_init (MAP_N_ELEMENTS);
	map_vector_tables[VECTOR_TABLE1] = vector_init (MAP_N_ELEMENTS);
	map_vector_table = map_vector_tables[VECTOR_TABLE0];
	printf ("map_vector_table = %p, %p, %p\n", map_vector_table, map_vector_tables[VECTOR_TABLE0], map_vector_tables[VECTOR_TABLE1]);

	u8 action = 0;
	base_action_iterator ("p", &action); printf ("action %x\n", action);
	base_action_iterator ("d", &action); printf ("action %x\n", action);

	INIT_TABLE(traffic_map, map_vector_table, map_hash_table);

	//map_table.mt_entry = map_vector_tables;

	
#if defined(HAVE_QUAGGA)
	lowest_map = highest_map = map_vector_table->index[0];
	install_element (CONFIG_NODE, &show_map_cmd);
	install_element (CONFIG_NODE, &new_map_cmd);
	install_element (CONFIG_NODE, &no_map_cmd);
	install_element (CONFIG_NODE, &no_map_application_udp_cmd);
	install_element (CONFIG_NODE, &map_application_udp_cmd);
	install_element (CONFIG_NODE, &map_adjusting_priority_highest_lowest_cmd);
	install_element (CONFIG_NODE, &map_adjusting_priority_cmd);
#endif

#ifndef HAVE_SELF_TEST
#define HAVE_SELF_TEST
#endif

#if defined(HAVE_SELF_TEST)
	struct map_t *map, *map1, *map2;
	char alias[32];
	int i;
	/** fully init map table */
	for (i = 0; i < MAP_N_ELEMENTS; i ++) {

		memset (alias, 0, 32);
		sprintf (alias, "map%d", i);
		map_table_entry_exact_lookup (alias, &map);
		if (likely(map)) {
			printf ("%s exist\n", alias);
			continue;
		}
		map_entry_new (&map, alias, "1,2", "4");
		map_table_entry_add (map);
	}

	map_table_entry_exact_lookup ("map1", &map1);
	if (!map1) {printf ("ERROR!!!! lookup\n"); exit(0);}

	map_table_entry_exact_lookup ("map2", &map2);
	if (!map2) {printf ("ERROR!!!! lookup\n");exit(0);}
#endif


finish:

#if defined(HAVE_SELF_TEST)
	/** fully destroy map table */
	for (i = 0; i < MAP_N_ELEMENTS; i ++) {

		memset (alias, 0, 32);
		sprintf (alias, "map%d", i);
		map_table_entry_exact_lookup (alias, &map);
		if (likely(map)) {
			printf ("%s exist\n", alias);
			continue;
		}
		map_table_entry_remove_and_destroy(map);
	}
#endif

	return 0;
}

