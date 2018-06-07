#include "oryx.h"
#include "common_private.h"
#include "iface_private.h"
#include "util_map.h"
#include "dpdk_classify.h"

extern volatile oryx_vector map_curr_table;

void map_entry_add_port (struct iface_t *port, struct map_t *map, u8 from_to)
{
	oryx_vector v;
	uint32_t *panel_port_mask = NULL;

	/** Map.Port */
	v = map->port_list[from_to % QUA_RXTX];
	vec_set_index (v, port->ul_id, (void *)port);

	if(from_to == QUA_RX) {
		map->rx_panel_port_mask = (1 << iface_id(port));
	}

	if(from_to == QUA_TX) {
		map->tx_panel_port_mask = (1 << iface_id(port));
	}
	
	/** Port.Map */
	v = port->belong_maps;
	vec_set_index (v, map_id(map), (void *)map);
	port->map_mask |= (1 << map_id(map));

	printf ("%s -> %s, and now %d maps ->>> ", iface_alias(port), map_alias(map), vec_active (port->belong_maps));
	int i = 0;
	struct map_t *m;
	v = port->belong_maps;
	vec_foreach_element (v, i, m) {
		printf ("%s ", map_alias(m));
	}

	printf ("\n");
	
}

void map_entry_remove_port (struct iface_t *port, struct map_t *map, u8 from_to)
{
	oryx_vector v;
	
	v = map->port_list[from_to % QUA_RXTX];
	/** Map.Port. */
	vec_unset (v, port->ul_id);
	/** Port.Map */
	vec_unset (port->belong_maps, map_id(map));
	port->map_mask &= ~(1 << map_id(map));
}

static __oryx_always_inline__
int vector_has_this_udp (oryx_vector v, struct udp_t *udp)
{
	int foreach;
	struct udp_t *u;
	
	vec_foreach_element (v, foreach, u) {
		if (!u) continue;
		else {
			if (u->ul_id  == udp->ul_id)
				return 1;
		}
	}
	return 0;
}

static __oryx_always_inline__
int vector_has_this_appl (oryx_vector v, struct appl_t *appl)
{
	int foreach;
	struct appl_t *u;

	vec_foreach_element (v, foreach, u) {
		if (!u) continue;
		else {
			if (u->ul_id  == appl_id(appl))
				return 1;
		}
	}
	return 0;
}

__oryx_always_extern__
int map_entry_add_appl (struct appl_t __oryx_unused__*appl, struct map_t __oryx_unused__*map , 
	void (*download)(struct map_t *, struct appl_t *))
{
	int xret = 0;
	oryx_vector v = map->appl_set;
	struct appl_signature_t *sig = appl->instance;
	
	xret = vector_has_this_appl (v, appl);
	
	if (xret < 0 /** critical error */)
		xret  = -1;
	else {
		if (xret == 0 /** no such element. */)
			if (likely(v)) {
				vec_set_index (v, appl_id(appl), appl);
				oryx_logn ("installing ... %s  done!", appl_alias(appl));
				download(map, appl);
			}
	}

	return xret;
}

__oryx_always_extern__
int map_entry_add_appl0 (struct appl_t __oryx_unused__*appl, struct map_t __oryx_unused__*map , 
	const char *hit_action, void (*dfn)(struct map_t *, struct appl_t *))
{
	int xret = 0;
	oryx_vector v = map->appl_set;
	struct appl_signature_t *sig = appl->instance;
	action_t action = {
		.act = 0,
	};
	
	xret = vector_has_this_appl (v, appl);
	
	if (xret < 0 /** critical error */)
		xret  = -1;
	else {
		if (xret == 0 /** no such element. */)
			if (likely(v)) {
				vec_set_index (v, appl_id(appl), appl);
				oryx_logn ("installing ... %s  done!", appl_alias(appl));
				if(!strncmp(hit_action, "p", 1)) {
					action.act |= CRITERIA_FLAGS_PASS;
				}
				if(!strncmp(hit_action, "d", 1)) {
					action.act &= ~CRITERIA_FLAGS_PASS;
					action.act |= CRITERIA_FLAGS_DROP;
				}

				map->appl_set_action[appl_id(appl)].act = action.act;
				dfn(map, appl);
			}
	}

	return xret;
}

__oryx_always_extern__
int map_entry_remove_appl (struct appl_t *appl, struct map_t *map)
{
	int xret = 0;
	oryx_vector v = map->appl_set;
	vlib_map_main_t *mm = &vlib_map_main;

	xret = vector_has_this_appl (v, appl);
	if (xret < 0 /** critical error */)
		xret  = -1;
	else {
		if (xret == 1 /** find same element. */)
			if (likely(v)) {
				/** remove appl from LPM table. */
				vec_unset (v, appl_id(appl));
				oryx_logn ("uninstalling ... %s  done!", appl_alias(appl));
			}
	}

	return xret;
}


/** add a user defined pattern to existent Map */
__oryx_always_extern__
int map_entry_add_udp (struct udp_t *udp, struct map_t *map)
{

	int xret = 0;
	oryx_vector v = map->udp_set;
	
	xret = vector_has_this_udp (v, udp);
	
	if (xret < 0 /** critical error */)
		xret  = -1;
	else {
		if (xret == 0 /** no such element. */)
			if (likely(v)) {
				vec_set_index (v, udp->ul_id, udp);
				oryx_logn ("installing ... %s  done(%d)!", udp_alias(udp), vec_count(v));
			}
	}
	return xret;
}


__oryx_always_extern__
int map_entry_remove_udp (struct udp_t *udp, struct map_t *map)
{

	int xret = 0;
	oryx_vector v = map->udp_set;
	
	xret = vector_has_this_udp (v, udp);
	
	if (xret < 0 /** critical error */)
		xret  = -1;
	else {
		if (xret == 1 /** find same element. */)
			if (likely(v)) {
				vec_unset (v, udp->ul_id);
				oryx_logn ("installing ... %s  done(%d)!", udp_alias(udp), vec_count(v));
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
	if (vec_count(map->udp) <= 0) {
		return 0;
	}
#endif

	struct udp_t *u;
	
	vec_foreach_element(map->udp_set, i, u) {
		if (!u) continue;
		
		struct pattern_t *p;
		vec_foreach_element(u->patterns, j, p) {
			if (!p) continue;

			ul_pid = mkpid (map_id(map), u->ul_id, p->ul_id);
			/** overwrite. */
			ul_sid = mksid (map_id(map), u->ul_id, p->ul_id);
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
	oryx_vector quat_vec;
	
	quat = QUA_PASS;
	quat_vec = udp->qua[quat];

	vec_set_index (quat_vec, map_id(map), map);
}

__oryx_always_extern__
void map_entry_add_udp_to_drop_quat (struct udp_t *udp, struct map_t *map)
{
	u8 quat;
	oryx_vector quat_vec;
	
	quat = QUA_DROP;
	quat_vec = udp->qua[quat];

	vec_set_index (quat_vec, map_id(map), map);
}

__oryx_always_extern__
void map_entry_setup_udp_quat (struct udp_t *udp, u8 quat, struct map_t *map)
{
	oryx_vector quat_vec = NULL;

	switch (quat) {
		case QUA_PASS:
			quat_vec = udp->qua[QUA_PASS];
			break;
		case QUA_DROP:
			quat_vec = udp->qua[QUA_DROP];
		default:
			ASSERT(0);
	}

	vec_set_index (quat_vec, map_id(map), map);
}

__oryx_always_extern__
void map_entry_unsetup_udp_quat (struct udp_t *udp, struct map_t *map)
{

	u8 quat;
	oryx_vector quat_vec;
	
	quat = QUA_DROP;
	quat_vec = udp->qua[quat];
	vec_unset (quat_vec, map_id(map));


	quat = QUA_PASS;
	quat_vec = udp->qua[quat];
	vec_unset (quat_vec, map_id(map));

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
	
	(*map)->udp_set = vec_init (1);
	(*map)->appl_set = vec_init (1);
	(*map)->ul_flags = MAP_TRAFFIC_TRANSPARENT;
	
	/** make alias. */
	sprintf ((char *)&(*map)->sc_alias[0], "%s", ((alias != NULL) ? alias: MAP_PREFIX));

	(*map)->port_list[QUA_RX] = vec_init (MAX_PORTS);
	(*map)->port_list[QUA_TX] = vec_init (MAX_PORTS);
	
	(*map)->port_list_str[QUA_RX] = strdup (from);
	(*map)->port_list_str[QUA_TX] = strdup(to);
	/** Need Map's ul_id, so have to remove map_entry_split_and_mapping_port to map_table_entry_add. */

	oryx_thread_mutex_create(&(*map)->ol_lock);

	return 0;
}

void map_entry_destroy (struct map_t *map)
{
	vec_free (map->port_list[QUA_RX]);
	vec_free (map->port_list[QUA_TX]);
	vec_free (map->appl_set);
	vec_free (map->udp_set);
	
	MpmDestroyThreadCtx (map->mpm_runtime_ctx, &map->mpm_thread_ctx);
	PmqFree(&map->pmq);

	kfree (map);
}

void map_entry_lookup (vlib_map_main_t *mm, char *alias, struct map_t **map)
{
	(*map) = NULL;
	
	if (!alias) return;

	void *s = oryx_htable_lookup (mm->htable,
		alias, strlen((const char *)alias));

	if (s) {
		(*map) = (struct map_t *) container_of (s, struct map_t, sc_alias);
	}
}

void map_entry_lookup_i (vlib_map_main_t *mm, u32 id, struct map_t **m)
{
	BUG_ON(map_curr_table == NULL);
	BUG_ON(id > vec_active(map_curr_table));
	
	if (vec_active(map_curr_table)) {
		(*m) = vec_lookup (map_curr_table, id);
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
			map_entry_lookup_i(mm, (*(u32*)lp->v), m);
			break;
		case LOOKUP_ALIAS:
			map_entry_lookup(mm, (const char*)lp->v, m);
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

void em_download_appl(struct map_t *map, struct appl_t *appl) {
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
	entry.u.k4.proto = sig->uc_proto;
	entry.id = map_id(map);

	hv = em_add_hash_key(&entry);
	if(hv < 0) {
		oryx_loge(-1,
			"(%d) add hash key error", hv);
	} else {
		//em_map_hash_key(hv, map);
		/** [key && map ]*/
		oryx_logn("(%d) add hash key ... %08x %08x %5d %5d %02x", hv,
			entry.u.k4.ip_src, entry.u.k4.ip_dst,
			entry.u.k4.port_src, entry.u.k4.port_dst,
			entry.u.k4.proto);
	}
}

void acl_download_appl(struct map_t *map, struct appl_t *appl) {
	vlib_map_main_t *mm = &vlib_map_main;
	/** upload this appl to */
	struct acl_route entry;
	int hv = 0;
	struct appl_signature_t *sig = appl->instance;
	
	memset(&entry, 0, sizeof(entry));
	entry.u.k4.ip_src		= ntoh32(sig->ip4[HD_SRC].prefix.s_addr);
	entry.u.k4.ip_dst		= ntoh32(sig->ip4[HD_DST].prefix.s_addr);
	entry.u.k4.port_src		= sig->port_start[HD_SRC];
	entry.u.k4.port_dst		= sig->port_start[HD_DST];
	entry.u.k4.proto		= sig->uc_proto;
	entry.ip_src_mask		= sig->ip4[HD_SRC].prefixlen;
	entry.ip_dst_mask		= sig->ip4[HD_DST].prefixlen;
	entry.port_src_mask		= sig->port_end[HD_SRC];
	entry.port_dst_mask		= sig->port_end[HD_DST];
	entry.proto_mask		= sig->proto_mask;
	entry.id				= map_id(map);

	hv = acl_add_entry(&entry);
	if(hv < 0) {
		oryx_loge(-1,
			"(%d) add acl error", hv);
	} else {
		/** [key && map ]*/
		oryx_logn("(%d) download ACL rule ... %08x %08x %5d %5d %02x", hv,
			entry.u.k4.ip_src, entry.u.k4.ip_dst,
			entry.u.k4.port_src, entry.u.k4.port_dst,
			entry.u.k4.proto);
	}
}

