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
		map_id(map) = vec_set (mm->map_curr_table, map);

		u8 from_to;
		from_to = QUA_RX;
		if (map->port_list_str[from_to]) {
			map_entry_split_and_mapping_port (map, from_to);
		}
		/** QUA_TX may should not be concerned. Discard it. */
		from_to = QUA_TX;
		if (map->port_list_str[from_to]) {
			map_entry_split_and_mapping_port (map, from_to);
		}

		mm->highest_map = vec_first (mm->map_curr_table);
		mm->lowest_map = vec_last (mm->map_curr_table);

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
	r = oryx_htable_del(mm->htable, (ht_value_t)map_alias(map),
								strlen((const char *)map_alias(map)));

	if (r == 0 /** success */) {

		/** dataplane may using the map, unmap port as soon as possible. */
		u8 from_to;
		
		from_to = QUA_RX;
		if (map->port_list_str[from_to]) {
			map_entry_split_and_unmapping_port (map, from_to);
		}

		/** QUA_TX may should not be concerned. Discard it. */
		from_to = QUA_TX;
		if (map->port_list_str[from_to]) {
			map_entry_split_and_unmapping_port (map, from_to);
		}
		
		/** Delete map from oryx_vector */
		vec_unset (mm->map_curr_table, map_id(map));
		mm->highest_map = vec_first (mm->map_curr_table);
		mm->lowest_map = vec_last (mm->map_curr_table);

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
	int i;
	char tmstr[100];
	vlib_port_main_t *pm = &vlib_port_main;
	vlib_appl_main_t *am = &vlib_appl_main;
	struct iface_t *iface;
	struct appl_t  *appl;
	int no_rx_panel_port = 0;
	int no_tx_panel_port = 0;
	
	BUG_ON(map == NULL);

	tm_format (map->ull_create_time, "%Y-%m-%d,%H:%M:%S", (char *)&tmstr[0], 100);

	/** let us try to find the map which name is 'alias'. */
	vty_out (vty, "%20s\"%s\"(%u)		%s%s", "Map ", map_alias(map), map_id(map), tmstr, VTY_NEWLINE);

	vty_out (vty, "		%20s", "Traffic mapping: ");
	if (!map->rx_panel_port_mask) {
		vty_out (vty, "N/A");
		no_rx_panel_port = 1;
	}
	else {
		vec_foreach_element(pm->entry_vec, i, iface) {
			if (iface) {
				if (map->rx_panel_port_mask & (1 << iface_id(iface)))
					vty_out (vty, "%s, ", iface_alias(iface));
			}
		}
	}

	vty_out (vty, "%s", "  ---->  ");

	if (!map->tx_panel_port_mask) {
		vty_out (vty, "N/A");
		no_tx_panel_port = 1;
	}
	else {
		vec_foreach_element(pm->entry_vec, i, iface) {
			if (iface) {
				if (map->tx_panel_port_mask & (1 << iface_id(iface)))
					vty_out (vty, "%s, ", iface_alias(iface));
			}
		}
	}
	vty_newline(vty);


	vty_out (vty, "		%20s", "Online Rx: ");
	if (no_rx_panel_port) vty_out (vty, "N/A");
	else {
		vec_foreach_element(pm->entry_vec, i, iface) {
			if (iface) {
				if (map->rx_panel_port_mask & (1 << iface_id(iface)) &&
					(iface->ul_flags & NETDEV_ADMIN_UP))
					vty_out (vty, "%s, ", iface_alias(iface));
			}
		}

	}
	vty_newline(vty);
	
	vty_out (vty, "		%20s", "Online Tx: ");
	if (no_tx_panel_port) vty_out (vty, "N/A");
	else {
		vec_foreach_element(pm->entry_vec, i, iface) {
			if (iface) {
				if (map->tx_panel_port_mask & (1 << iface_id(iface)) &&
					(iface->ul_flags & NETDEV_ADMIN_UP))
					vty_out (vty, "%s, ", iface_alias(iface));
			}
		}

	}
	vty_newline(vty);

	vty_out (vty, "		%20s", "Application: ");
	if (!map->ul_nb_appls) {	vty_out (vty, "N/A"); }
	else {
		vec_foreach_element(am->entry_vec, i, appl) {
			if (appl) {
				if (appl->ul_map_mask & (1 << map_id(map)))
					vty_out (vty, "%s, ", appl_alias(appl));
			}
		}
	}
	vty_newline(vty);

	vty_out (vty, "%20s%s", "-------------------------------------------", VTY_NEWLINE);
}

#define PRINT_SUMMARY	\
	vty_out (vty, "matched %d element(s), %d element(s) actived.%s", \
		atomic_read(&n_map_elements), (int)vec_count(mm->map_curr_table), VTY_NEWLINE);

DEFUN(show_map,
      show_map_cmd,
      "show map [WORD]",
      SHOW_STR SHOW_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR)
{
	vlib_map_main_t *mm = &vlib_map_main;
	
	vty_out (vty, "Trying to display %s%d%s elements ...%s", 
		draw_color(COLOR_RED), vec_active(mm->map_curr_table), draw_color(COLOR_FIN), 
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
	mm->lowest_map = mm->highest_map = mm->map_curr_table->index[0];

	PRINT_SUMMARY;
	
    return CMD_SUCCESS;
}

DEFUN(new_map,
      new_map_cmd,
      "map WORD from port WORD to port WORD",
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

DEFUN(map_application,
      map_application_cmd,
      "map WORD (pass|mirror) application WORD",
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR)
{
	struct map_t *map;
	vlib_map_main_t *mm = &vlib_map_main;

	map_table_entry_deep_lookup((const char *)argv[0], &map);
	if (unlikely(!map)) {
		VTY_ERROR_MAP ("non-existent", (char *)argv[0]);
		return CMD_SUCCESS;
	}
#if 1
	split_foreach_application_func1_param3 (argv[2],
					map_entry_add_appl, map, argv[1], acl_download_appl);
#else
	split_foreach_application_func1_param3 (argv[2],
					map_entry_add_appl, map, argv[1], NULL);
#endif
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
	vlib_map_main_t *mm = &vlib_map_main;	

	map_table_entry_deep_lookup((const char *)argv[0], &map);
	if (unlikely (!map)) {
		VTY_ERROR_MAP ("non-existent", (char *)argv[0]);
		return CMD_SUCCESS;
	}

	split_foreach_application_func1_param2 (argv[1],
			map_entry_remove_appl, map, acl_remove_appl);

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
	 * vty_out (vty, "before %s %s", map_alias(before), VTY_NEWLINE);
	 * vty_out (vty, "after %s %s", map_alias(after), VTY_NEWLINE);
	 * map_priority_show(vty); 
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
		map_id(map) = vec_set (vector_backup, map);
	}

	/** do swapping ??? */
	do_swap = 1;

	if (do_swap){
		/** Critical region started. */
		do_lock (&mm->lock);
		/** vector_runtime and mm->map_curr_table remapping must be protected by lock. */
		mm->map_curr_table = mm->entry_vec[mm->vector_runtime%VECTOR_TABLES];
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
			map_table_entry_lookup (&lp_id, &v);
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
		map_id(map) = vec_set (vector_backup, map);
	}

	/** do swapping ??? */
	do_swap = 1;

	if (do_swap){
		/** vector_runtime and mm->map_curr_table remapping must be protected by lock. */
		mm->map_curr_table = mm->entry_vec[mm->vector_runtime%VECTOR_TABLES];
	}

	return CMD_SUCCESS;
}

DEFUN(sync_appl,
	sync_appl_cmd,
	"sync",
	KEEP_QUITE_STR KEEP_QUITE_CSTR)
{
	vlib_map_main_t *mm = &vlib_map_main;
	vlib_main_t *vm = mm->vm;

	vm->ul_flags |= VLIB_DP_SYNC;
	sync_acl();
	vm->ul_flags &= ~VLIB_DP_SYNC;
	
	return CMD_SUCCESS;
}

static __oryx_always_inline__
void map_online_iface_update_tmr(struct oryx_timer_t __oryx_unused__*tmr,
			int __oryx_unused__ argc, char __oryx_unused__**argv)
{
	int each_map;
	int each_port;
	vlib_map_main_t *mm = &vlib_map_main;
	vlib_port_main_t *pm = &vlib_port_main;
	struct map_t *map;
	struct iface_t *iface;
	uint32_t tx_panel_ports[MAX_PORTS] = {0};
	int i;
	uint32_t index = 0;

	/** here need a signal from iface.link_detect timer. */
	vec_foreach_element(mm->map_curr_table, each_map, map) {
		if (unlikely(!map))
			continue;

		index = 0;
		for (i = 0; i < MAX_PORTS; i ++) {
			tx_panel_ports[i] = UINT32_MAX;
		}
		
		vec_foreach_element(pm->entry_vec, each_port, iface) {
			if(unlikely(!iface))
				continue;
			if(map->tx_panel_port_mask & (1 << iface_id(iface))) {
				if(iface->ul_flags & NETDEV_ADMIN_UP) {
					tx_panel_ports[index ++] = iface_id(iface);					
				}
			}
		}
		map->nb_online_tx_panel_ports = index;
		for (i = 0; i < MAX_PORTS; i ++) {
			map->online_tx_panel_ports[i] = tx_panel_ports[i];
		}
	}
}


void map_init(vlib_main_t *vm)
{
	vlib_map_main_t *mm = &vlib_map_main;

	mm->vm = vm;
	
	mm->htable = oryx_htable_init(DEFAULT_HASH_CHAIN_SIZE, 
			map_hval, map_cmp, map_free, 0);
	
	mm->entry_vec[VECTOR_TABLE0] = vec_init (MAX_MAPS);
	mm->entry_vec[VECTOR_TABLE1] = vec_init (MAX_MAPS);
	if (mm->htable == NULL || 
		mm->entry_vec[VECTOR_TABLE0] == NULL ||
		mm->entry_vec[VECTOR_TABLE1] == NULL) {
		printf ("vlib map main init error!\n");
		exit(0);
	}

	INIT_LIST_HEAD(&mm->map_priority_list);

	mm->map_curr_table = mm->entry_vec[VECTOR_TABLE0];

	mm->lowest_map = mm->highest_map = mm->map_curr_table->index[0];
	install_element (CONFIG_NODE, &show_map_cmd);
	install_element (CONFIG_NODE, &new_map_cmd);
	install_element (CONFIG_NODE, &no_map_cmd);
	install_element (CONFIG_NODE, &no_map_application_cmd);
	install_element (CONFIG_NODE, &map_application_cmd);
	install_element (CONFIG_NODE, &map_adjusting_priority_highest_lowest_cmd);
	install_element (CONFIG_NODE, &map_adjusting_priority_cmd);
	install_element (CONFIG_NODE, &sync_appl_cmd);

	uint32_t ul_activity_tmr_setting_flags = TMR_OPTIONS_PERIODIC | TMR_OPTIONS_ADVANCED;

	mm->online_port_update_tmr = oryx_tmr_create(1, "map online port update tmr", 
							ul_activity_tmr_setting_flags,
							map_online_iface_update_tmr,
							0, NULL, 3000);
	if(likely(mm->online_port_update_tmr))
		oryx_tmr_start(mm->online_port_update_tmr);

	vm->ul_flags |= VLIB_MAP_INITIALIZED;
}

