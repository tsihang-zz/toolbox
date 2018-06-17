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
	oryx_logn("x-> %s", map->port_list_str[from_to]);
	if (map->port_list_str[from_to]) {
		split_foreach_port_func1_param2 (
			map->port_list_str[from_to], map_entry_add_port, map, from_to);
	}
}

static __oryx_always_inline__
void map_entry_split_and_unmapping_port (struct map_t *map, u8 from_to)
{
	if (map->port_list_str[from_to]) {
		split_foreach_port_func1_param2 (
			map->port_list_str[from_to], map_entry_remove_port, map, from_to);
	}
}

static __oryx_always_inline__
void map_inherit(struct map_t *son, struct map_t *father)
{
	uint32_t id = map_id(son);

	/** inherit all data from father node. */
	memcpy (son, father, sizeof(struct map_t));

	/** except THE ID. */
	map_id(son) = id;
	oryx_logn("1");
	map_entry_split_and_mapping_port (son, QUA_RX);
	map_entry_split_and_mapping_port (son, QUA_TX);
}

int map_table_entry_add (struct map_t *map)
{
	int r = 0;
	vlib_map_main_t *mm = &vlib_map_main;
	
	do_lock (&mm->lock);

	int each;
	struct map_t *son = NULL, *a = NULL;
	
	/** lookup for an empty slot for this map. */
	vec_foreach_element(mm->entry_vec, each, a) {
		if (unlikely(!a))
			continue;
		if (!(a->ul_flags & MAP_VALID)) {
			son = a;
			break;
		}
	}
	
	if (son) {			
		/** if there is an unused map, update its data with formatted map */		
		map_inherit(son, map);
		r = oryx_htable_add(mm->htable, 
					map_alias(son), strlen((const char *)map_alias(son)));
		/** Add alias to hash table for fast lookup by map alise. */
		if (r != 0)
			goto finish;

		son->ul_flags |= MAP_VALID;
		mm->nb_maps ++;
		kfree(map);
	
	} else {
		/** else, set this map to vector. */
		r = oryx_htable_add(mm->htable, 
					map_alias(map), strlen((const char *)map_alias(map)));
		/** Add alias to hash table for fast lookup by map alise. */
		if (r != 0)
			goto finish;

		map_id(map) = vec_set (mm->entry_vec, map);
		oryx_logn("2");
		map_entry_split_and_mapping_port (map, QUA_RX);
		map_entry_split_and_mapping_port (map, QUA_TX);
		map->ul_flags |= MAP_VALID;
		mm->nb_maps ++;
	}

finish:
	do_unlock (&mm->lock);
	return r;
}

int no_map_table_entry (struct map_t *map)
{
	vlib_map_main_t *mm = &vlib_map_main;
	int r = 0;
	
	do_lock (&mm->lock);
	
	/** Delete alias from hash table. */
	r = oryx_htable_del(mm->htable, (ht_value_t)map_alias(map),
								strlen((const char *)map_alias(map)));

	if (r == 0 /** success */) {
		map->ul_flags &= ~MAP_VALID;
		map_entry_split_and_unmapping_port (map, QUA_RX);
		map_entry_split_and_unmapping_port (map, QUA_TX);
		mm->nb_maps --;
	}

	do_unlock (&mm->lock);

	/** Should you free here ? */
	//vec_unset (mm->entry_vec, map_id(map));
	//kfree(map);
	
	return r;
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
	if(!(map->ul_flags & MAP_VALID))
		return;

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
		atomic_read(&n_map_elements), (int)vec_count(mm->entry_vec), VTY_NEWLINE);

DEFUN(show_map,
      show_map_cmd,
      "show map [WORD]",
      SHOW_STR SHOW_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR)
{
	vlib_map_main_t *mm = &vlib_map_main;
	
	vty_out (vty, "Trying to display %s%d%s elements ...%s", 
		draw_color(COLOR_RED), mm->nb_maps, draw_color(COLOR_FIN), 
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
				no_map_table_entry);
	}
	else {
		split_foreach_map_func1_param0 (argv[0],
				no_map_table_entry);
	}

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
	vlib_main_t *vm = mm->vm;

	map_table_entry_deep_lookup((const char *)argv[0], &map);
	if (unlikely(!map)) {
		VTY_ERROR_MAP ("non-existent", (char *)argv[0]);
		return CMD_SUCCESS;
	}
#if 0
	lock_lcores(vm);
	split_foreach_application_func1_param3 (argv[2],
					map_entry_add_appl, map, argv[1], acl_download_appl);
	unlock_lcores(vm);
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

DEFUN(sync_appl,
	sync_appl_cmd,
	"sync",
	KEEP_QUITE_STR KEEP_QUITE_CSTR)
{
	vlib_map_main_t *mm = &vlib_map_main;
	vlib_main_t *vm = mm->vm;
	
	vty_out(vty, "Syncing ... %s", VTY_NEWLINE);
	sync_acl(vm);
	vty_out(vty, "done %s", VTY_NEWLINE);

	return CMD_SUCCESS;
}

DEFUN(test_map,
	test_map_cmd,
	"test map",
	KEEP_QUITE_STR KEEP_QUITE_CSTR
	KEEP_QUITE_STR KEEP_QUITE_CSTR)
{
  const char *range = "150:180";
  const char *mask = "150/255";
  uint32_t val_start, val_end;
  uint32_t val, val_mask;
  int ret;
  vlib_map_main_t *mm = &vlib_map_main;
  struct appl_t *map = NULL;
  char alias[32] = {0};

  ret = format_range (range, UINT16_MAX, 0, ':', &val_start, &val_end);
  vty_out (vty, "%s ret = %d, start %d end %d%s", range,
	  ret, val_start, val_end, VTY_NEWLINE);

  ret = format_range (mask, UINT16_MAX, 0, '/', &val, &val_mask);
  vty_out (vty, "%s ret = %d, %d/%d%s", mask,
	  ret, val, val_mask, VTY_NEWLINE);

  int i = 0;

  for (i = 0; i < MAX_MAPS; i ++) {
	 memset(alias, 0, 31);
	 oryx_pattern_generate(alias, 16);
	  map_table_entry_deep_lookup((const char *)&alias[0], &map);
	  if (likely(map)) {
		  VTY_ERROR_MAP ("same", (char *)&alias[0]);
		  map_entry_output (map, vty);
		  return CMD_SUCCESS;
	  }
	  map_entry_new (&map, (char *)&alias[0], "lan*", "lan*");
	  if (unlikely (!map)) {
		  VTY_ERROR_MAP ("new", (char *)&alias[0]);
		  return CMD_SUCCESS;
	  }
	  if (!map_table_entry_add (map)) {
		  VTY_SUCCESS_MAP ("new", map);
	  }

  }
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
	uint32_t rx_panel_ports[MAX_PORTS] = {0};
	int i;
	uint32_t rx_index = 0, tx_index = 0;

	/** here need a signal from iface.link_detect timer. */
	vec_foreach_element(mm->entry_vec, each_map, map) {
		if (unlikely(!map))
			continue;
		if (!(map->ul_flags & MAP_VALID))
			continue;
		
		rx_index = tx_index = 0;
		for (i = 0; i < MAX_PORTS; i ++) {
			rx_panel_ports[i] = UINT32_MAX;	
			tx_panel_ports[i] = UINT32_MAX;
		}
		
		vec_foreach_element(pm->entry_vec, each_port, iface) {
			if(unlikely(!iface))
				continue;
			if(iface->ul_flags & NETDEV_ADMIN_UP) {
				if(map->rx_panel_port_mask & (1 << iface_id(iface)))
					rx_panel_ports[rx_index ++] = iface_id(iface);
		
				if(map->tx_panel_port_mask & (1 << iface_id(iface)))
					tx_panel_ports[tx_index ++] = iface_id(iface);	
			}
		}
		
		map->nb_online_rx_panel_ports = rx_index;
		map->nb_online_tx_panel_ports = tx_index;
		for (i = 0; i < MAX_PORTS; i ++) {
			map->online_rx_panel_ports[i] = rx_panel_ports[i];
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
	
	mm->entry_vec = vec_init (MAX_MAPS);
	if (mm->htable == NULL || 
		mm->entry_vec == NULL) {
		printf ("vlib map main init error!\n");
		exit(0);
	}

	install_element (CONFIG_NODE, &show_map_cmd);
	install_element (CONFIG_NODE, &new_map_cmd);
	install_element (CONFIG_NODE, &no_map_cmd);
	install_element (CONFIG_NODE, &no_map_application_cmd);
	install_element (CONFIG_NODE, &map_application_cmd);
	install_element (CONFIG_NODE, &sync_appl_cmd);
	install_element (CONFIG_NODE, &test_map_cmd);

	uint32_t ul_activity_tmr_setting_flags = TMR_OPTIONS_PERIODIC | TMR_OPTIONS_ADVANCED;

	mm->online_port_update_tmr = oryx_tmr_create(1, "map online port update tmr", 
							ul_activity_tmr_setting_flags,
							map_online_iface_update_tmr,
							0, NULL, 3000);
	if(likely(mm->online_port_update_tmr))
		oryx_tmr_start(mm->online_port_update_tmr);

	vm->ul_flags |= VLIB_MAP_INITIALIZED;
}

