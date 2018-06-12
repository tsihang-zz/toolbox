#include "oryx.h"
#include "common_private.h"
#include "iface_private.h"
#include "util_map.h"
#include "dpdk_classify.h"

void map_entry_add_port (struct iface_t *port, struct map_t *map, u8 from_to)
{
	/** Map Rx Port */
	if(from_to == QUA_RX) {
		map->rx_panel_port_mask |= (1 << iface_id(port));
	}

	/** Map Tx Port */
	if(from_to == QUA_TX) {
		map->tx_panel_port_mask |= (1 << iface_id(port));
	}
}

void map_entry_remove_port (struct iface_t *port, struct map_t *map, u8 from_to)
{
	/** Unmap Rx Port */
	if(from_to == QUA_RX) {
		map->rx_panel_port_mask &= ~(1 << iface_id(port));
	}

	/** Unmap Tx Port */
	if(from_to == QUA_TX) {
		map->tx_panel_port_mask &= ~(1 << iface_id(port));
	}
}

static __oryx_always_inline__
int map_has_this_appl (struct map_t *map, struct appl_t *appl)
{
	return (appl->ul_map_mask & (1 << map_id(map)));
}

__oryx_always_extern__
int map_entry_add_appl (struct appl_t *appl, struct map_t *map , 
	const char *hit_action, int (*rule_download_engine)(struct map_t *, struct appl_t *))
{
	struct appl_signature_t *sig = appl->instance;	
	action_t action = {
		.act = 0,
	};
	
	if (map_has_this_appl (map, appl))
		return 0;
	else {
		if(!strncmp(hit_action, "p", 1)) {
			action.act |= CRITERIA_FLAGS_PASS;
		}
		if(!strncmp(hit_action, "d", 1)) {
			action.act &= ~CRITERIA_FLAGS_PASS;
			action.act |= CRITERIA_FLAGS_DROP;
		}
		
		if(rule_download_engine) {
			if(!rule_download_engine(map, appl)) {
				oryx_logn ("installing ... %s  done!", appl_alias(appl));		
				appl->ul_map_mask |= (1 << map_id(map));
				return 0;
			}
		}
	}

	return -1;
}

__oryx_always_extern__
int map_entry_remove_appl (struct appl_t *appl, struct map_t *map,
	int (*rule_remove_engine)(struct map_t *, struct appl_t *))
{
	if (map_has_this_appl (map, appl)) {
		if(!rule_remove_engine(map, appl)) {
			oryx_logn ("uninstalling ... %s  done!", appl_alias(appl));
			appl->ul_map_mask &= ~(1 << map_id(map));
			return 0;
		}
	}
	return -1;
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
	INIT_LIST_HEAD(&(*map)->prio_node);

	(*map)->ul_flags = MAP_TRAFFIC_TRANSPARENT;
	
	/** make alias. */
	sprintf ((char *)&(*map)->sc_alias[0], "%s", ((alias != NULL) ? alias: MAP_PREFIX));

	(*map)->port_list_str[QUA_RX] = strdup (from);
	(*map)->port_list_str[QUA_TX] = strdup(to);
	/** Need Map's ul_id, so have to remove map_entry_split_and_mapping_port to map_table_entry_add. */

	oryx_thread_mutex_create(&(*map)->ol_lock);

	return 0;
}

void map_entry_destroy (struct map_t *map)
{
	kfree (map);
}

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
	mm->nb_acl_rules --;	
	return 0;
}

int acl_download_appl(struct map_t *map, struct appl_t *appl) {
	vlib_map_main_t *mm = &vlib_map_main;
	/** upload this appl to */
	struct acl_route entry;
	int hv = 0;
	struct appl_signature_t *sig = appl->instance;
	
	memset(&entry, 0, sizeof(entry));
	entry.u.k4.ip_src				= sig->ip_src;
	entry.u.k4.ip_dst				= sig->ip_dst;
	entry.ip_src_mask				= sig->ip_src_mask;
	entry.ip_dst_mask				= sig->ip_dst_mask;

	entry.u.k4.port_src				= (sig->l4_port_src_mask == ANY_PORT)		? 1 : sig->l4_port_src;
	entry.port_src_mask				= (sig->l4_port_src_mask == ANY_PORT)		? 0xFFFF : sig->l4_port_src_mask;
	entry.u.k4.port_dst				= (sig->l4_port_dst_mask == ANY_PORT)		? 1 : sig->l4_port_dst;
	entry.port_dst_mask				= (sig->l4_port_dst_mask == ANY_PORT) 		? 0xFFFF : sig->l4_port_dst_mask;
	entry.u.k4.proto				= (sig->ip_next_proto_mask == ANY_PROTO)	? 1 : sig->ip_next_proto;
	entry.ip_next_proto_mask		= (sig->ip_next_proto_mask == ANY_PROTO)	? 0xFF : sig->ip_next_proto_mask;
	entry.id						= map_id(map);


	hv = acl_add_entries(&entry, 1);
	if(hv < 0) {
		oryx_loge(-1,
			"(%d) add acl error", hv);
		return -1;
	} else {
		/** [key && map ]*/
		oryx_logn("(%d) download ACL rule ... %08x/%d %08x/%d %5d:%5d %5d:%5d %02x:%02x", hv,
			entry.u.k4.ip_src, entry.ip_src_mask,
			entry.u.k4.ip_dst, entry.ip_dst_mask,
			entry.u.k4.port_src, 
			entry.port_src_mask,
			entry.u.k4.port_dst,
			entry.port_dst_mask,
			entry.u.k4.proto,
			entry.ip_next_proto_mask);

		mm->nb_acl_rules ++;
		
		return 0;
	}
}

