#include "oryx.h"
#include "vty.h"
#include "command.h"
#include "prefix.h"

#include "udp_private.h"
#include "iface_private.h"
#include "util_map.h"
#include "dpdk_classify.h"
#include "cli_iface.h"
#include "cli_udp.h"
#include "cli_appl.h"
#include "cli_map.h"


#if defined(HAVE_DPDK)
#include "dpdk.h"	/* struct eth_addr */
#endif

vlib_map_main_t vlib_map_main = {
	.lock = INIT_MUTEX_VAL,
};

atomic_t n_map_elements = ATOMIC_INIT(0);

volatile oryx_vector map_curr_table;

#define VTY_ERROR_MAP(prefix, alias)\
	vty_out (vty, "%s(Error)%s %s map \"%s\"%s", \
		draw_color(COLOR_RED), draw_color(COLOR_FIN), prefix, alias, VTY_NEWLINE)

#define VTY_SUCCESS_MAP(prefix, v)\
	vty_out (vty, "%s(Success)%s %s map \"%s\"(%u)%s", \
		draw_color(COLOR_GREEN), draw_color(COLOR_FIN), prefix, v->sc_alias, v->ul_id, VTY_NEWLINE)

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
	vlib_map_main_t *mm = &vlib_map_main;
	
	do_lock (&mm->lock);
	
	r = oryx_htable_add(mm->htable, 
		map_alias(map), strlen((const char *)map_alias(map)));

	/** Add alias to hash table for fast lookup by map alise. */
	if (r == 0 /** success */) {
		/** 
		  * Add map to oryx_vector. 
		  * Must be called before split and (un)mapping port to this map entry.
		  */
		map_slot(map) = vec_set (map_curr_table, map);

		u8 from_to;
		from_to = MAP_N_PORTS_FROM;
		if (map->port_list_str[from_to]) {
			map_entry_split_and_mapping_port (map, from_to);
		}
		/** MAP_N_PORTS_TO may should not be concerned. Discard it. */
		from_to = MAP_N_PORTS_TO;
		if (map->port_list_str[from_to]) {
			map_entry_split_and_mapping_port (map, from_to);
		}

		mm->highest_map = vec_first (map_curr_table);
		mm->lowest_map = vec_last (map_curr_table);

		/** Add this map to priority list */
		list_add_tail (&map->prio_node, &mm->map_priority_list);
		
	}

	do_unlock (&mm->lock);
	
	return r;
}

int map_table_entry_remove (struct map_t *map)
{
	vlib_map_main_t *mm = &vlib_map_main;
	int r = 0;
	
	do_lock (&mm->lock);
	
	/** Delete alias from hash table. */
	r = oryx_htable_del(mm->htable, map->sc_alias, strlen((const char *)map->sc_alias));

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
		
		/** Delete map from oryx_vector */
		vec_unset (map_curr_table, map->ul_id);
		mm->highest_map = vec_first (map_curr_table);
		mm->lowest_map = vec_last (map_curr_table);

		/** Delete this map from priority list */
		list_del (&map->prio_node);
	}

	do_unlock (&mm->lock);
	
	return r;
}

void map_table_entry_del (struct map_t *map)
{
	BUG_ON(map == NULL);
	map_table_entry_remove (map);
	map_entry_destroy(map);
}

static void map_entry_output (struct map_t *map,  struct vty *vty)
{

	oryx_vector v;
	
	ASSERT (map);

	{		
		char tmstr[100];
		tm_format (map->ull_create_time, "%Y-%m-%d,%H:%M:%S", (char *)&tmstr[0], 100);

		/** let us try to find the map which name is 'alias'. */
		vty_out (vty, "%16s\"%s\"(%u)		%s%s", "Map ", map->sc_alias, map->ul_id, tmstr, VTY_NEWLINE);

		vty_out (vty, "		%16s", "Ports: ");
		v = map->port_list[MAP_N_PORTS_FROM];
		int actives;

		actives = vec_active(v);
		if (!actives) {vty_out (vty, "N/A");}
		else {
			int i;
			struct iface_t *p;
			vec_foreach_element (v, i, p) {
				if (p) {
					vty_out (vty, "%s", p->sc_alias);
					-- actives;
					if (actives) vty_out (vty, ", ");
					else actives = actives;
				}
				else {p = p;}
			}
			//vty_out (vty, "%s", VTY_NEWLINE);
		}

		vty_out (vty, "%s", "  ---->  ");
		v = map->port_list[MAP_N_PORTS_TO];
		actives = vec_active(v);
		if (!actives) vty_out (vty, "N/A%s", VTY_NEWLINE);
		else {
			int i;
			struct iface_t *p;
			vec_foreach_element (v, i, p) {
				if (p) {
					if (p->ul_flags & NETDEV_LOOPBACK)
						vty_out (vty, "%s(%s%s%s)", p->sc_alias, 
							draw_color(COLOR_RED), 
							"loopback",
							draw_color(COLOR_FIN));
					else
						vty_out (vty, "%s", p->sc_alias);
					-- actives;
					if (actives) vty_out (vty, ", ");
					else actives = actives;
				}
				else {p = p;}
			}
			vty_out (vty, "%s", VTY_NEWLINE);
		}

		/** Caculate passed an dropped. */
		int passed = 0, dropped = 0;

		
		v = map->appl_set;
		actives = vec_active(v);
		if (actives) {
			int i;
			struct appl_t *a;
			vec_foreach_element (v, i, a) {
				if (a) {
					if (a->ul_flags & CRITERIA_FLAGS_PASS) passed ++;
					else dropped ++;
				}
				else { a = a;}
			}
		}

		vty_out (vty, "		%16s%s, %s %s", "States: ", 
			(!passed && !dropped) ? ((map->ul_flags & MAP_TRAFFIC_TRANSPARENT) ? "transparent" : "blocked") : "by Criteria(s)", 
			"hash(sdip+sdp+protocol+vlan)",
			VTY_NEWLINE);
		
		vty_out (vty, "		%16s%d appl(s)%s", "Pass: ", passed, VTY_NEWLINE);
		vty_out (vty, "		%16s%d appl(s)%s", "Drop: ", dropped, VTY_NEWLINE);

		/** 
		  *   Actually, we may have defined number of applications, 
		  *   but here for now, we are trying to display stream application only.
		  */
		v = map->appl_set;
		actives = vec_active(v);
		if (!actives) vty_out (vty, "		%16s%s%s", "Appl(s): ", "N/A", VTY_NEWLINE);
		else {
			int i;
			vty_out (vty, "		%16s%s", "Appl(s): ", VTY_NEWLINE);
			struct appl_t *a;
			vec_foreach_element (v, i, a) {
				if (a) {
					vty_out (vty, "		     %16s (%s)%s", a->sc_alias, 
						(a->ul_flags & CRITERIA_FLAGS_PASS) ? "pass" : "drop", VTY_NEWLINE);
				}
				else { a = a;}
			}
			vty_out (vty, "%s", VTY_NEWLINE);
		}

		v = map->udp_set;
		actives = vec_active(v);
		if (!actives) vty_out (vty, "		%16s%s%s", "UDP(s): ", "N/A", VTY_NEWLINE);
		else {
			int i;
			vty_out (vty, "		%16s%s", "Udp(s): ", VTY_NEWLINE);
			struct udp_t *a;
			vec_foreach_element (v, i, a) {
				if (a) {
					oryx_vector v0, v1;

					v0 = a->qua[QUA_DROP];
					v1 = a->qua[QUA_PASS];
					if (vec_lookup (v0, map->ul_id)) {
						vty_out (vty, "		     %16s (%s)%s", a->sc_alias, 
							"drop", VTY_NEWLINE);
					}
					if (vec_lookup (v1, map->ul_id)) {
						vty_out (vty, "		     %16s (%s)%s", a->sc_alias, 
							"pass", VTY_NEWLINE);
					}
				}
				else { a = a;}
			}
			vty_out (vty, "%s", VTY_NEWLINE);
		}
		
		vty_out (vty, "%16s%s", "-------------------------------------------", VTY_NEWLINE);
	}
}

#define PRINT_SUMMARY	\
	vty_out (vty, "matched %d element(s), %d element(s) actived.%s", \
		atomic_read(&n_map_elements), (int)vec_count(map_curr_table), VTY_NEWLINE);

DEFUN(show_map,
      show_map_cmd,
      "show map [WORD]",
      SHOW_STR SHOW_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR)
{
	vty_out (vty, "Trying to display %s%d%s elements ...%s", 
		draw_color(COLOR_RED), vec_active(map_curr_table), draw_color(COLOR_FIN), 
		VTY_NEWLINE);
	
	vty_out (vty, "%16s%s", "-------------------------------------------", VTY_NEWLINE);

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
	vlib_map_main_t *mm = &vlib_map_main;
	
	if (argc == 0) {
		foreach_map_func1_param0 (argv[0],
				map_table_entry_del);
	}
	else {
		split_foreach_map_func1_param0 (argv[0],
				map_table_entry_del);
	}

	/** reset lowest priority map and highest priority map. */
	mm->lowest_map = mm->highest_map = map_curr_table->index[0];

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
	vlib_map_main_t *mm = &vlib_map_main;
	struct map_t *map;

	map_table_entry_deep_lookup((const char *)argv[0], &map);
	if(likely(map)) {
		VTY_ERROR_MAP ("same", (char *)argv[0]);
		map_entry_output (map, vty);
		return CMD_SUCCESS;
	}

	map_entry_new (&map, (char *)argv[0], (char *)argv[1], (char *)argv[2]);
	if (unlikely (!map)) {
		VTY_ERROR_MAP ("new", (char *)argv[0]);
		return CMD_SUCCESS;
	}
	if (!map_table_entry_add (map)) {
		VTY_SUCCESS_MAP ("new", map);
	}
			
    return CMD_SUCCESS;
}

static void map_install_udp (char __oryx_unused__ *pass_drop, char *alias, struct map_t *map)
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
		/** vector_runtime and map_curr_table remapping must be protected by lock. */
		map->mpm_runtime_ctx  = &map->mpm_ctx[map->mpm_index%MPM_TABLES];
		/** Start dataplane string match rountine. */
		map->ul_mpm_has_been_setup = 1;
		do_unlock (map->ol_lock);

		/** After swapped, cleanup previous mpmctx. */
		MpmDestroyCtx (&map->mpm_ctx[(map->mpm_index + 1)%MPM_TABLES]);
	}
}

static void map_uninstall_udp (char *alias, struct map_t *map)
{
	void *backup_mpm_ptr;
	u8 do_swap = 0;
	vlib_map_main_t *mm = &vlib_map_main;
	
	split_foreach_udp_func2_param1 (alias,
			map_entry_remove_udp, map, 
			map_entry_unsetup_udp_quat, map);

	/** Make sure that there are some udps to install this time.  */
	if (vec_count(map->udp_set) <= 0) {
		map->ul_mpm_has_been_setup = 0;
		return;
	}

	backup_mpm_ptr = map->mpm_runtime_ctx;
	
	map_entry_install_and_compile_udp (map);

	do_swap = 1;
	
	if (do_swap) {
		
		/** Critical region started. */
		do_lock (&mm->lock);
		/** Stop dataplane string match routine. */
		map->ul_mpm_has_been_setup = 0;
		/** vector_runtime and map_curr_table remapping must be protected by lock. */
		map->mpm_runtime_ctx  = &map->mpm_ctx[map->mpm_index%MPM_TABLES];
		/** Start dataplane string match rountine. */
		map->ul_mpm_has_been_setup = 1;
		do_unlock (&mm->lock);

		/** After swapped, cleanup previous mpmctx. */
		MpmDestroyCtx (&map->mpm_ctx[(map->mpm_index + 1)%MPM_TABLES]);
	}
}

DEFUN(map_application,
      map_application_cmd,
      "map WORD (pass|drop) application WORD",
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR)
{

	struct map_t *map;
	
	map_table_entry_deep_lookup((const char *)argv[0], &map);
	if (unlikely(!map)) {
		VTY_ERROR_MAP ("non-existent", (char *)argv[0]);
		return CMD_SUCCESS;
	}

	split_foreach_application_func1_param3 (argv[2],
					map_entry_add_appl0, map, argv[1], em_download_appl);

	PRINT_SUMMARY;
	
    return CMD_SUCCESS;
}


DEFUN(no_map_applicatio,
      no_map_application_cmd,
      "no map WORD application WORD",
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR)
{

	struct map_t *map;
	
	map_table_entry_deep_lookup((const char *)argv[0], &map);
	if (unlikely (!map)) {
		VTY_ERROR_MAP ("non-existent", (char *)argv[0]);
		return CMD_SUCCESS;
	}

	split_foreach_application_func1_param1 (argv[1],
			map_entry_remove_appl, map);

	PRINT_SUMMARY;
	
     return CMD_SUCCESS;
}

static void map_priority_show (struct vty *vty)
{
	vlib_map_main_t *mm = &vlib_map_main;
	
	struct map_t *map = NULL, *p;
	list_for_each_entry_safe(map, p, &mm->map_priority_list, prio_node){
		vty_out (vty, "%s, ", map_alias(map));
	}

	vty_out (vty, "%s", VTY_NEWLINE);
	vty_out (vty, "%s", VTY_NEWLINE);

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
	/** All map in priority list can be reload to backup oryx_vector */
	int i;
	u8 do_swap = 0;
	oryx_vector vector_backup;
	struct map_t *map = NULL, *p;
	struct map_t *v1, *v2, *after, *before;
	vlib_map_main_t *mm = &vlib_map_main;
	
	map_table_entry_deep_lookup((const char *)argv[0], &v1);
	if (unlikely (!v1)) {
		VTY_ERROR_MAP ("non-existent", (char *)argv[0]);
		return CMD_SUCCESS;
	}

	map_table_entry_deep_lookup((const char *)argv[2], &v2);
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
	vty_out (vty, "before %s %s", map_alias(before), VTY_NEWLINE);
	vty_out (vty, "after %s %s", map_alias(after), VTY_NEWLINE);
	map_priority_show(vty);	
*/

	list_del (&after->prio_node);
	list_add (&after->prio_node, &before->prio_node);

	vector_backup = mm->entry_vec[++mm->vector_runtime%VECTOR_TABLES];
	
	/** Destroy all maps in backup oryx_vector's slots. */
	vec_foreach_element(vector_backup, i, map) {
		vec_unset (vector_backup, i);
	}

	/** Map to oryx_vector */
	list_for_each_entry_safe(map, p, &mm->map_priority_list, prio_node) {
		/** Update slot automatically. */
		map_slot(map) = vec_set (vector_backup, map);
	}

	/** do swapping ??? */
	do_swap = 1;

	if (do_swap){
		/** Critical region started. */
		do_lock (&mm->lock);
		/** vector_runtime and map_curr_table remapping must be protected by lock. */
		map_curr_table = mm->entry_vec[mm->vector_runtime%VECTOR_TABLES];
		do_unlock (&mm->lock);
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
	/** All map in priority list can be reload to backup oryx_vector */
	int i;
	u8 do_swap = 0;
	oryx_vector vector_backup;
	struct map_t *map = NULL, *p;
	struct map_t *v;
	vlib_map_main_t *mm = &vlib_map_main;

	struct prefix_t lp_al = {
		.cmd = LOOKUP_ALIAS,
		.s = strlen ((char *)argv[0]),
		.v = (char *)argv[0],
	};

	map_table_entry_lookup (&lp_al, &v);
	if (unlikely (!v)) {
		/** try id lookup if alldigit input. */
		if (isalldigit ((char *)argv[0])) {
			u32 id = atoi((char *)argv[0]);
			struct prefix_t lp_id = {
				.cmd = LOOKUP_ID,
				.v = (void *)&id,
				.s = strlen ((char *)argv[0]),
			};
			map_table_entry_lookup (&lp_al, &v);
			if(likely(v))
				goto lookup;
		}
		VTY_ERROR_MAP ("non-existent", (char *)argv[0]);
		return CMD_SUCCESS;
	}

lookup:

	/** */
	list_del (&v->prio_node);
	
	if (!strncmp (argv[1], "h", 1)) {
		/** Insert a new entry before the specified head */
		list_add (&v->prio_node, &mm->map_priority_list);
	} else {
		/** Insert a new entry after the specified head */
		list_add_tail (&v->prio_node, &mm->map_priority_list);
	}

	/** Critical region started. */
	vector_backup = mm->entry_vec[++mm->vector_runtime%VECTOR_TABLES];
	
	/** Destroy all maps in backup oryx_vector's slots. */
	vec_foreach_element(vector_backup, i, map) {
		vec_unset (vector_backup, i);
	}

	/** Map to oryx_vector */
	list_for_each_entry_safe(map, p, &mm->map_priority_list, prio_node){
		/** Update slot automatically. */
		map_slot(map) = vec_set (vector_backup, map);
	}

	/** do swapping ??? */
	do_swap = 1;

	if (do_swap){
		/** vector_runtime and map_curr_table remapping must be protected by lock. */
		map_curr_table = mm->entry_vec[mm->vector_runtime%VECTOR_TABLES];
	}

	return CMD_SUCCESS;
}

#ifdef HAVE_DEFAULT_MAP
static struct map_t default_map = {
	.ul_flags = MAP_DEFAULT;
};
#endif


void map_init(vlib_main_t *vm)
{
	vlib_map_main_t *mm = &vlib_map_main;
	
	mm->htable = oryx_htable_init(DEFAULT_HASH_CHAIN_SIZE, 
			map_hval, map_cmp, map_free, 0);
	
	mm->entry_vec[VECTOR_TABLE0] = vec_init (MAX_MAPS);
	mm->entry_vec[VECTOR_TABLE1] = vec_init (MAX_MAPS);
	if (mm->htable == NULL || mm->entry_vec[VECTOR_TABLE0] == NULL) {
		printf ("vlib map main init error!\n");
		exit(0);
	}

	INIT_LIST_HEAD(&mm->map_priority_list);

	map_curr_table = mm->entry_vec[VECTOR_TABLE0];

	mm->lowest_map = mm->highest_map = map_curr_table->index[0];
	install_element (CONFIG_NODE, &show_map_cmd);
	install_element (CONFIG_NODE, &new_map_cmd);
	install_element (CONFIG_NODE, &no_map_cmd);
	install_element (CONFIG_NODE, &no_map_application_cmd);
	install_element (CONFIG_NODE, &map_application_cmd);
	install_element (CONFIG_NODE, &map_adjusting_priority_highest_lowest_cmd);
	install_element (CONFIG_NODE, &map_adjusting_priority_cmd);

	vm->ul_flags |= VLIB_MAP_INITIALIZED;
}

