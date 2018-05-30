#include "oryx.h"
#include "common_private.h"
#include "iface_private.h"
#include "util_map.h"

extern volatile oryx_vector map_curr_table;

void map_entry_add_port (struct iface_t *port, struct map_t *map, u8 from_to)
{
	oryx_vector v;

	/** Map.Port */
	v = map->port_list[from_to % MAP_N_PORTS];
	vec_set_index (v, port->ul_id, (void *)port);
	
	/** Port.Map */
	v = port->belong_maps;
	vec_set_index (v, map->ul_id, (void *)map);

	printf ("%s -> %s, and now %d maps ->>> ", port->sc_alias, map->sc_alias, vec_active (port->belong_maps));
	int i = 0;
	struct map_t *m;
	v = port->belong_maps;
	vec_foreach_element (v, i, m) {
		printf ("%s ", m->sc_alias);
	}

	printf ("\n");
	
}

void map_entry_remove_port (struct iface_t *port, struct map_t *map, u8 from_to)
{
	oryx_vector v;
	
	v = map->port_list[from_to % MAP_N_PORTS];
	/** Map.Port. */
	vec_unset (v, port->ul_id);
	/** Port.Map */
	vec_unset (port->belong_maps, map->ul_id);
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
			if (u->ul_id  == appl->ul_id)
				return 1;
		}
	}
	return 0;
}

__oryx_always_extern__
int map_entry_add_appl (struct appl_t __oryx_unused__*appl, struct map_t __oryx_unused__*map)
{

	int xret = 0;
	oryx_vector v = map->appl_set;
	
	xret = vector_has_this_appl (v, appl);
	
	if (xret < 0 /** critical error */)
		xret  = -1;
	else {
		if (xret == 0 /** no such element. */)
			if (likely(v)) {
				vec_set_index (v, appl->ul_id, appl);
				oryx_logn ("installing ... %s  done!", appl_alias(appl));
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
				vec_unset (v, appl->ul_id);
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
	oryx_vector quat_vec;
	
	quat = QUA_PASS;
	quat_vec = udp->qua[quat];

	vec_set_index (quat_vec, map_slot(map), map);
}

__oryx_always_extern__
void map_entry_add_udp_to_drop_quat (struct udp_t *udp, struct map_t *map)
{
	u8 quat;
	oryx_vector quat_vec;
	
	quat = QUA_DROP;
	quat_vec = udp->qua[quat];

	vec_set_index (quat_vec, map_slot(map), map);
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

	vec_set_index (quat_vec, map_slot(map), map);
}

__oryx_always_extern__
void map_entry_unsetup_udp_quat (struct udp_t *udp, struct map_t *map)
{

	u8 quat;
	oryx_vector quat_vec;
	
	quat = QUA_DROP;
	quat_vec = udp->qua[quat];
	vec_unset (quat_vec, map_slot(map));


	quat = QUA_PASS;
	quat_vec = udp->qua[quat];
	vec_unset (quat_vec, map_slot(map));

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

	(*map)->port_list[MAP_N_PORTS_FROM] = vec_init (MAX_PORTS);
	(*map)->port_list[MAP_N_PORTS_TO] = vec_init (MAX_PORTS);
	
	(*map)->port_list_str[MAP_N_PORTS_FROM] = strdup (from);
	(*map)->port_list_str[MAP_N_PORTS_TO] = strdup(to);
	/** Need Map's ul_id, so have to remove map_entry_split_and_mapping_port to map_table_entry_add. */

	oryx_thread_mutex_create(&(*map)->ol_lock);

	return 0;
}

void map_entry_destroy (struct map_t *map)
{
	vec_free (map->port_list[MAP_N_PORTS_FROM]);
	vec_free (map->port_list[MAP_N_PORTS_TO]);
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
	(*m) = vec_lookup (map_curr_table, id);
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

