#include "oryx.h"
#include "common_private.h"
#include "iface_private.h"
#include "util_map.h"
#include "dpdk_classify.h"

void map_entry_add_port (struct iface_t *port, struct map_t *map, u8 from_to)
{
	oryx_vector v;
	uint32_t *panel_port_mask = NULL;

	/** Map.Port */
	if(from_to == QUA_RX) {
		map->rx_panel_port_mask = (1 << iface_id(port));
	}

	if(from_to == QUA_TX) {
		map->tx_panel_port_mask = (1 << iface_id(port));
	}
	
	/** Port.Map */
	port->ul_map_mask |= (1 << map_id(map));
	
}

void map_entry_remove_port (struct iface_t *port, struct map_t *map, u8 from_to)
{
	/** Port.Map */
	port->ul_map_mask &= ~(1 << map_id(map));
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
		if(!rule_download_engine(map, appl)) {
			oryx_logn ("installing ... %s  done!", appl_alias(appl));		
			appl->ul_map_mask |= (1 << map_id(map));
			return 0;
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

void map_entry_lookup_alias (vlib_map_main_t *mm, char *alias, struct map_t **map)
{
	(*map) = NULL;
	
	if (!alias) return;

	void *s = oryx_htable_lookup (mm->htable,
		alias, strlen((const char *)alias));

	if (s) {
		(*map) = (struct map_t *) container_of (s, struct map_t, sc_alias);
	}
}

void map_entry_lookup_id (vlib_map_main_t *mm, u32 id, struct map_t **m)
{
	BUG_ON(mm->map_curr_table == NULL);
	BUG_ON(id > vec_active(mm->map_curr_table));
	
	if (vec_active(mm->map_curr_table)) {
		(*m) = vec_lookup (mm->map_curr_table, id);
	}
}

void map_table_entry_lookup (struct prefix_t *lp, 
	struct map_t **m)
{
	vlib_map_main_t *mm = &vlib_map_main;

	ASSERT (lp);
	ASSERT (m);
	(*m) = NULL;
	
	switch (lp->cmd) {
		case LOOKUP_ID:
			map_entry_lookup_id(mm, (*(u32*)lp->v), m);
			break;
		case LOOKUP_ALIAS:
			map_entry_lookup_alias(mm, (const char*)lp->v, m);
			break;
		default:
			break;
	}
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
				.s = strlen ((char *)argv),
			};
			map_table_entry_lookup (&lp_id, &v);
		}
	}
	(*map) = v;

	return 0;
}

#if 0
int em_download_appl(struct map_t *map, struct appl_t *appl) {
	vlib_map_main_t *mm = &vlib_map_main;
	/** upload this appl to */
	struct em_route entry;
	int hv = 0;
	struct appl_signature_t *sig = appl->instance;
	
	memset(&entry, 0, sizeof(entry));
	entry.u.k4.ip_src = ntoh32(sig->ip4[HD_SRC].prefix.s_addr);
	entry.u.k4.ip_dst = ntoh32(sig->ip4[HD_DST].prefix.s_addr);
	entry.u.k4.port_src = sig->port_start[HD_SRC];
	entry.u.k4.port_dst = sig->port_start[HD_DST];
	entry.u.k4.proto = sig->ip_next_proto;
	entry.id = map_id(map);

	hv = em_add_hash_key(&entry);
	if(hv < 0) {
		oryx_loge(-1,
			"(%d) add hash key error", hv);
		return -1;
	} else {
		//em_map_hash_key(hv, map);
		/** [key && map ]*/
		oryx_logn("(%d) add hash key ... %08x %08x %5d %5d %02x", hv,
			entry.u.k4.ip_src, entry.u.k4.ip_dst,
			entry.u.k4.port_src, entry.u.k4.port_dst,
			entry.u.k4.proto);
		return 0;
	}
}
#endif

int acl_remove_appl(struct map_t *map, struct appl_t *appl) {
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

	hv = acl_add_entry(&entry);
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

