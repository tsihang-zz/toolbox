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

void sync_acl(vlib_main_t *vm);


vlib_map_main_t vlib_map_main = {
	.lock = INIT_MUTEX_VAL,
};

atomic_t n_map_elements = ATOMIC_INIT(0);


#define VTY_ERROR_MAP(prefix, alias)\
	vty_out (vty, "%s(Error)%s %s map \"%s\"%s", \
		draw_color(COLOR_RED), draw_color(COLOR_FIN), prefix, (const char *)alias, VTY_NEWLINE)

#define VTY_SUCCESS_MAP(prefix, v)\
	vty_out (vty, "%s(Success)%s %s map \"%s\"(%u)%s", \
		draw_color(COLOR_GREEN), draw_color(COLOR_FIN), prefix, (char *)&v->sc_alias[0], v->ul_id, VTY_NEWLINE)

static void
ht_map_free (const ht_value_t __oryx_unused_param__ v)
{
	/** Never free here! */
}

static ht_key_t
ht_map_hval (struct oryx_htable_t *ht,
		const ht_value_t v, uint32_t s) 
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
ht_map_cmp (const ht_value_t v1, 
		uint32_t s1,
		const ht_value_t v2,
		uint32_t s2)
{
	int xret = 0;

	if (!v1 || !v2 ||s1 != s2 ||
		memcmp(v1, v2, s2))
		xret = 1;

	return xret;
}

static __oryx_always_inline__
void map_ports (struct map_t *map, const char *iface_str, uint8_t qua)
{
#if defined(BUILD_DEBUG)
	BUG_ON(map == NULL);
	BUG_ON(iface_str == NULL);
#endif
	foreach_iface_split_func1_param2 (
		iface_str, map_entry_add_port, map, qua);
}

static __oryx_always_inline__
void unmap_ports (struct map_t *map, const char *iface_str, uint8_t qua)
{
#if defined(BUILD_DEBUG)
	BUG_ON(map == NULL);
	BUG_ON(iface_str == NULL);
#endif
	foreach_iface_split_func1_param2 (
		iface_str, map_entry_remove_port, map, qua);
}

static __oryx_always_inline__
void unmap_all_ports (struct map_t *map, const uint8_t qua)
{
#if defined(BUILD_DEBUG)
	BUG_ON(map == NULL);
#endif
	foreach_iface_func1_param2 (
		NULL, map_entry_remove_port, map, qua);

	/* Hold this BUG. */
	if (qua == QUA_RX)
		BUG_ON(map->rx_panel_port_mask != 0);
	if (qua == QUA_TX)
		BUG_ON(map->tx_panel_port_mask != 0);
}


static __oryx_always_inline__
void map_appls (struct map_t *map, char *appl_str)
{
#if defined(BUILD_DEBUG)
	BUG_ON(map == NULL);
	BUG_ON(appl_str == NULL);
#endif
	split_foreach_application_func1_param1 (
		appl_str, map_entry_add_appl, map);
}

static __oryx_always_inline__
void unmap_appls (struct map_t *map, char *appl_str)
{
#if defined(BUILD_DEBUG)
	BUG_ON(map == NULL);
	BUG_ON(appl_str == NULL);
#endif

	split_foreach_application_func1_param1 (
		appl_str, map_entry_remove_appl, map);
}

static __oryx_always_inline__
void unmap_all_appls (struct map_t *map)
{
#if defined(BUILD_DEBUG)
	BUG_ON(map == NULL);
#endif

	foreach_application_func1_param1 (
		NULL, map_entry_remove_appl, map);

	/* Hold this BUG. */
	BUG_ON(map->ul_nb_appls != 0);
}

static __oryx_always_inline__
void map_inherit(struct map_t *son, struct map_t *father)
{
	uint32_t id = map_id(son);

	/** inherit all data from father node. */
	memcpy (son, father, sizeof(struct map_t));

	/** except THE ID. */
	map_id(son) = id;
	map_ports (son, son->port_list_str[QUA_RX], QUA_RX);
	map_ports (son, son->port_list_str[QUA_TX], QUA_TX);
}

static int map_table_entry_add (vlib_map_main_t *mm, struct map_t *map)
{
	int r = 0;
	int each;
	struct map_t *son = NULL, *a = NULL;
	
	do_mutex_lock (&mm->lock);
	
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
		map = son;
	
	} else {
		/** else, set this map to vector. */
		r = oryx_htable_add(mm->htable, 
					map_alias(map), strlen((const char *)map_alias(map)));
		/** Add alias to hash table for fast lookup by map alise. */
		if (r != 0)
			goto finish;

		map_id(map) = vec_set (mm->entry_vec, map);
		map_ports (map, map->port_list_str[QUA_RX], QUA_RX);
		map_ports (map, map->port_list_str[QUA_TX], QUA_TX);
		map->ul_flags |= MAP_VALID;
		mm->nb_maps ++;
	}

finish:
	do_mutex_unlock (&mm->lock);
	return r;
}

static int no_map_table_entry (struct map_t *map)
{
	vlib_map_main_t *mm = &vlib_map_main;
	vlib_appl_main_t *am = &vlib_appl_main;
	vlib_main_t *vm = mm->vm;
	int r = 0;
	
	do_mutex_lock (&mm->lock);
	
	/** Delete alias from hash table. */
	r = oryx_htable_del(mm->htable, (ht_value_t)map_alias(map),
								strlen((const char *)map_alias(map)));

	if (r == 0 /** success */) {

		/** disable this map. */
		map->ul_flags &= ~MAP_VALID;
		
		/** unmap all rx & tx ports */
		unmap_all_ports (map, QUA_RX);
		unmap_all_ports (map, QUA_TX);

		/** unmap all applications */
		if(map->ul_nb_appls) {
			unmap_all_appls(map);
		}
		mm->nb_maps --;
	}

	do_mutex_unlock (&mm->lock);

	/** Should you free here ? */
	
	return r;
}

static void map_entry_output (struct map_t *map,  struct vty *vty)
{
	int i;
	char tmstr[100];
	vlib_iface_main_t *pm = &vlib_iface_main;
	vlib_appl_main_t *am = &vlib_appl_main;
	struct iface_t *iface;
	struct appl_t  *appl;
	int no_rx_panel_port = 0;
	int no_tx_panel_port = 0;
	
	BUG_ON(map == NULL);
	if(!(map->ul_flags & MAP_VALID))
		return;

	fmt_time (map->ull_create_time, "%Y-%m-%d,%H:%M:%S", (char *)&tmstr[0], 100);

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
		atomic_read(&n_map_elements), mm->nb_maps, VTY_NEWLINE);

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

	if (argc == 0) {
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

	map_entry_find_same((const char *)argv[0], &map);
	if (likely(map)) {
		VTY_ERROR_MAP ("same", argv[0]);
		map_entry_output (map, vty);
		return CMD_SUCCESS;
	}

	map_entry_new (&map, (const char *)argv[0], (const char *)argv[1], (const char *)argv[2]);

	if (unlikely (!map)) {
		VTY_ERROR_MAP ("new", argv[0]);
		return CMD_SUCCESS;
	}
	
	if (map_table_entry_add (mm, map)) {
		VTY_ERROR_MAP ("new", argv[0]);
	}
			
    return CMD_SUCCESS;
}

DEFUN(map_add_rm_port,
	map_add_rm_port_cmd,
	"map WORD (add|rm) (rx|tx) port WORD",
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
	uint8_t qua = QUA_TX;
	uint8_t add = 1;

	map_entry_find_same((const char *)argv[0], &map);
	if (unlikely(!map)) {
		VTY_ERROR_MAP ("non-existent", argv[0]);
		return CMD_SUCCESS;
	}

	if(!strncmp(argv[1], "r", 1))
		add = 0;
	if(!strncmp(argv[2], "r", 1))
		qua = QUA_RX;

	add ? map_ports  (map, (const char *)argv[3], qua):\
		   unmap_ports(map, (const char *)argv[3], qua);

	return CMD_SUCCESS;
}


DEFUN(map_application,
      map_application_cmd,
      "map WORD application WORD (pass|timestamp)",
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR)
{
	struct map_t *map;
	vlib_map_main_t *mm = &vlib_map_main;
	uint32_t action = 0;

	map_entry_find_same((const char *)argv[0], &map);
	if(unlikely(!map)) {
		VTY_ERROR_MAP ("non-existent", argv[0]);
		return CMD_SUCCESS;
	}

	if(!strncmp(argv[2], "t", 1))
		action |= ACT_TIMESTAMP;
	if(!strncmp(argv[2], "p", 1))
		action |= ACT_FWD;


	split_foreach_application_func1_param1 (argv[1],
				map_entry_add_appl, map);

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

	map_entry_find_same((const char *)argv[0], &map);
	if (unlikely (!map)) {
		VTY_ERROR_MAP ("non-existent", argv[0]);
		return CMD_SUCCESS;
	}

	split_foreach_application_func1_param1 (argv[1],
			map_entry_remove_appl, map);

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
	int				i;
	char			alias[32] = {0};
	struct map_t	*map = NULL;
	vlib_map_main_t	*mm = &vlib_map_main;

	for (i = 0; i < MAX_MAPS; i ++) {
		memset(alias, 0, 31);
		oryx_pattern_generate(alias, 16);
		map_entry_find_same((const char *)&alias[0], &map);
		if (likely(map)) {
			VTY_ERROR_MAP ("same", (char *)&alias[0]);
			map_entry_output (map, vty);
			return CMD_SUCCESS;
		}

		map_entry_new (&map, (const char *)&alias[0], (const char *)"lan*", (const char *)"lan*");

		if (unlikely (!map)) {
			VTY_ERROR_MAP ("new", (char *)&alias[0]);
			return CMD_SUCCESS;
		}

		if (map_table_entry_add (mm, map)) {
			VTY_ERROR_MAP ("new", argv[0]);
		}
	}
	return CMD_SUCCESS;
}

static __oryx_always_inline__
void map_online_iface_update_tmr(struct oryx_timer_t __oryx_unused_param__*tmr,
			int __oryx_unused_param__ argc, char __oryx_unused_param__**argv)
{
	int 				i;
	int					each_map;
	int					each_port;
	uint32_t			tx_panel_ports[MAX_PORTS] = {0};
	uint32_t			rx_panel_ports[MAX_PORTS] = {0};
	uint32_t			rx_index = 0;
	uint32_t			tx_index = 0;
	struct map_t		*map;
	struct iface_t		*iface;
	vlib_map_main_t		*mm = &vlib_map_main;
	vlib_iface_main_t	*pm = &vlib_iface_main;

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

void vlib_map_init(vlib_main_t *vm)
{
	vlib_map_main_t *mm = &vlib_map_main;

	mm->vm			=	vm;
	mm->entry_vec	=	vec_init (MAX_MAPS);
	mm->htable		=	oryx_htable_init(DEFAULT_HASH_CHAIN_SIZE, 
									ht_map_hval, ht_map_cmp, ht_map_free, 0);	
	if (mm->htable == NULL || 
		mm->entry_vec == NULL)
		oryx_panic(-1, 
			"vlib map main init error!");


	install_element (CONFIG_NODE, &show_map_cmd);
	install_element (CONFIG_NODE, &new_map_cmd);
	install_element (CONFIG_NODE, &no_map_cmd);
	install_element (CONFIG_NODE, &no_map_application_cmd);
	install_element (CONFIG_NODE, &map_application_cmd);
	install_element (CONFIG_NODE, &sync_appl_cmd);
	install_element (CONFIG_NODE, &test_map_cmd);
	install_element (CONFIG_NODE, &map_add_rm_port_cmd);

	mm->online_port_update_tmr = oryx_tmr_create(1, "map online port update tmr", 
										(TMR_OPTIONS_PERIODIC | TMR_OPTIONS_ADVANCED),
										map_online_iface_update_tmr, 0, NULL, 3000);

	if(likely(mm->online_port_update_tmr))
		oryx_tmr_start(mm->online_port_update_tmr);

	vm->ul_flags |= VLIB_MAP_INITIALIZED;
}

