#include "oryx.h"
#include "common_private.h"
#include "iface_private.h"
#include "util_map.h"
#include "dpdk_classify.h"

static __oryx_always_inline__
int map_has_this_appl (struct map_t *map, struct appl_t *appl)
{
	return (appl->ul_map_mask & (1 << map_id(map)));
}


__oryx_always_extern__
void map_entry_add_port (struct iface_t *port, struct map_t *map, u8 from_to)
{
#if 0
	/* skip SW <-> CPU interface. */
	if (iface_id(port) == SW_CPU_XAUI_PORT_ID)
		return;
#endif

	oryx_logn("add = %s", from_to == QUA_RX ? "rx" : "tx");
	/** Map Rx Port */
	if(from_to == QUA_RX) {
		map->rx_panel_port_mask |= (1 << iface_id(port));
	}

	/** Map Tx Port */
	if(from_to == QUA_TX) {
		map->tx_panel_port_mask |= (1 << iface_id(port));
	}
}

__oryx_always_extern__
void map_entry_remove_port (struct iface_t *port, struct map_t *map, u8 from_to)
{
	oryx_logn("rm = %s", from_to == QUA_RX ? "rx" : "tx");
	/** Unmap Rx Port */
	if(from_to == QUA_RX) {
		map->rx_panel_port_mask &= ~(1 << iface_id(port));
	}

	/** Unmap Tx Port */
	if(from_to == QUA_TX) {
		map->tx_panel_port_mask &= ~(1 << iface_id(port));
	}
}

__oryx_always_extern__
int map_entry_add_appl (struct appl_t *appl, struct map_t *map , 
	uint32_t action)
{
	if (map_has_this_appl (map, appl)) {
		oryx_logn("map %s trying to add appl %s, exsited.", map_alias(map), appl_alias(appl));
		return 0;
	}

	struct appl_priv_t *ap;
	egress_options eo = {
		.act.v = ACT_DEFAULT,
	};

	/** parse application action when hit on dataplane. */
	eo.act.v = action;
	
	ap					=	&appl->priv[appl->nb_maps ++];
	ap->ul_flags		=	eo.data32;
	ap->ul_map_id		=	map_id(map);
	appl->ul_map_mask	|=	(1 << map_id(map));
	map->ul_nb_appls ++;

	return 0;
}

__oryx_always_extern__
int map_entry_remove_appl (struct appl_t *appl, struct map_t *map)
{
	if (!map_has_this_appl (map, appl)) {		
		oryx_logn("map %s trying to rm appl %s, non-exsited.", map_alias(map), appl_alias(appl));
		return 0;
	}
	
	appl->ul_map_mask &=	~(1 << map_id(map));
	map->ul_nb_appls --;

	return -1;
}

__oryx_always_extern__
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

	(*map)->ul_flags = MAP_TRAFFIC_TRANSPARENT;
	
	/** make alias. */
	sprintf ((char *)&(*map)->sc_alias[0], "%s", ((alias != NULL) ? alias: MAP_PREFIX));

	(*map)->port_list_str[QUA_RX] = strdup (from);
	(*map)->port_list_str[QUA_TX] = strdup(to);
	/** Need Map's ul_id, so have to remove map_entry_split_and_mapping_port to map_table_entry_add. */

	oryx_thread_mutex_create(&(*map)->ol_lock);

	return 0;
}

__oryx_always_extern__
int map_table_entry_deep_lookup(const char *argv, struct map_t **map)
{	
	struct map_t *v;
	
	struct prefix_t lp_al = {
		.cmd = LOOKUP_ALIAS,
		.s = strlen ((char *)argv),
		.v = (char *)argv,
	};

	map_table_entry_lookup (&lp_al, &v);
	if (unlikely (!v)) {
		/** try id lookup if alldigit input. */
		if (isalldigit ((char *)argv)) {
			u32 id = atoi((char *)argv);
			struct prefix_t lp_id = {
				.cmd = LOOKUP_ID,
				.v = (void *)&id,
				.s = sizeof(id),
			};
			map_table_entry_lookup (&lp_id, &v);
		}
	}
	(*map) = v;

	return 0;
}

int acl_remove_appl(struct map_t *map, struct appl_t *appl) {
	vlib_map_main_t *mm = &vlib_map_main;
	return 0;
}


