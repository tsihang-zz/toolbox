#include "oryx.h"
#include "prefix.h"
#include "util_ipset.h"

void appl_entry_format (struct appl_t *appl, 
	u32 __oryx_unused__*rule_id, const char *unused_var, 
	char __oryx_unused__*vlan, 
	char __oryx_unused__*sip, 
	char __oryx_unused__*dip, 
	char __oryx_unused__*sp, 
	char __oryx_unused__*dp, 
	char __oryx_unused__*proto)
{

	u8 action = APPL_DEFAULT_ACTIONS;
	struct appl_signature_t *as;
	
	if (!appl) return;

	as = appl->instance;
	if (!as) return;

	if (vlan) {
		if (isalldigit (vlan)) 
			as->us_vlan = atoi (vlan);
		else {
			/** default is ANY_VLAN */
			as->us_vlan = ANY_VLAN; /** To avoid warnings. */
		}
	}
	
	if (sip) str2prefix_ipv4 (sip, &as->ip4[HD_SRC]);
	if (dip) str2prefix_ipv4 (dip, &as->ip4[HD_DST]);

	/** Format TCP/UDP Port. */
	if (sp) {
		u8 p = HD_SRC;
		if (isalldigit (sp)) {
			as->port_start[p] = as->port_end[p] = atoi (sp);
		}
		else {
			/** default is ANY_PORT */
			as->port_start[p] = as->port_end[p] = ANY_PORT; /** To avoid warnings. */
			
		}
	}
	
	if (dp) {
		u8 p = HD_DST;
		if (isalldigit (dp)) {
			as->port_start[p] = as->port_end[p] = atoi (dp);
		}
		else {
			/** default is ANY_PORT */
			as->port_start[p] = as->port_end[p] = ANY_PORT; /** To avoid warnings. */
		}
	}
	
	/** Format TCP/UDP Protocol. */
	if (proto) {
		if (isalldigit (proto)) {
			as->uc_proto  = atoi (proto);
		}
		else {
			/** default is ANY_PROTO */
			as->uc_proto  = ANY_PROTO; /** To avoid warnings. */
		}
	}
}

void appl_entry_new (struct appl_t **appl, 
			char *alias, u32 __oryx_unused__ type)
{
	/** create an appl */
	(*appl) = kmalloc (sizeof (struct appl_t), MPF_CLR, __oryx_unused_val__);
	ASSERT ((*appl));
	
	/** make appl alias. */
	sprintf ((char *)&(*appl)->sc_alias[0], "%s", ((alias != NULL) ? alias: APPL_PREFIX));
	(*appl)->ul_type = APPL_TYPE_STREAM;
	(*appl)->ul_id = APPL_INVALID_ID;
	(*appl)->ul_flags = APPL_DEFAULT_ACTIONS;
	(*appl)->ull_create_time = time(NULL);
	
	(*appl)->instance = kmalloc (sizeof (struct appl_signature_t), MPF_CLR, __oryx_unused_val__);
	ASSERT ((*appl)->instance);
	
	struct appl_signature_t *as = (*appl)->instance;
	as->us_vlan = ANY_VLAN;
	as->uc_proto = ANY_PROTO;
	as->port_start[HD_SRC] = as->port_end[HD_SRC] = ANY_PORT;
	as->port_start[HD_DST] = as->port_end[HD_DST] = ANY_PORT;
	as->udp = vec_init (12);
}

void appl_entry_lookup (vlib_appl_main_t *vp, const char *alias, struct appl_t **appl)
{
	(*appl) = NULL;
	
	if (!alias) return;

	void *s = oryx_htable_lookup (vp->htable,
		alias, strlen((const char *)alias));

	if (s) {
		(*appl) = (struct appl_t *) container_of (s, struct appl_t, sc_alias);
	}
}
void appl_entry_lookup_i (vlib_appl_main_t *vp, u32 id, struct appl_t **appl)
{
	(*appl) = NULL;
	(*appl) = vec_lookup (vp->entry_vec, id);
}

int appl_entry_del (vlib_appl_main_t *vp, struct appl_t *appl)
{
	if (!appl ||
		(appl && 
		appl_id(appl) == APPL_INVALID_ID/** Delete appl from oryx_vector */)) {
		return 0;
	}
	
	if (likely(THIS_ELEMENT_IS_INUNSE(appl))) {
		return 0;
	}
	
	do_lock (&vp->lock);
	int r = oryx_htable_del(vp->htable, appl_alias(appl), strlen((const char *)appl_alias(appl)));
	if (r == 0 /** success */) {
		vec_unset (vp->entry_vec, appl_id(appl));
	}
	do_unlock (&vp->lock);
	/** Should you free appl here ? */

	kfree (appl->instance);
	kfree (appl);

	return 0;  
}

int appl_entry_add (vlib_appl_main_t *vp, struct appl_t *appl)
{
	BUG_ON(appl_id(appl) != APPL_INVALID_ID);

	do_lock (&vp->lock);
	
	/** Add appl_alias(appl) to hash table for controlplane fast lookup */
	int r = oryx_htable_add(vp->htable, appl_alias(appl), strlen((const char *)appl_alias(appl)));
	if (r == 0 /** success*/) {
		/** Add appl to a oryx_vector table for dataplane fast lookup. */
		appl_id(appl) = vec_set (vp->entry_vec, appl);
	}

	do_unlock (&vp->lock);
	return r;
}

void appl_table_entry_lookup (struct prefix_t *lp, 
				struct appl_t **a)
{
	vlib_appl_main_t *am = &vlib_appl_main;

	ASSERT (lp);
	ASSERT (a);
	(*a) = NULL;
	
	switch (lp->cmd) {
		case LOOKUP_ID:
			appl_entry_lookup_i(am, (*(u32*)lp->v), a);
			break;
		case LOOKUP_ALIAS:
			appl_entry_lookup(am, (const char*)lp->v, a);
			break;
		default:
			break;
	}
}

int appl_table_entry_deep_lookup(const char *argv, 
				struct appl_t **appl)
{	
	struct appl_t *v;
	
	struct prefix_t lp_al = {
		.cmd = LOOKUP_ALIAS,
		.s = strlen ((char *)argv),
		.v = (char *)argv,
	};

	appl_table_entry_lookup (&lp_al, &v);
	if (unlikely (!v)) {
		/** try id lookup if alldigit input. */
		if (isalldigit ((char *)argv)) {
			u32 id = atoi((char *)argv);
			struct prefix_t lp_id = {
				.cmd = LOOKUP_ID,
				.v = (void *)&id,
				.s = strlen ((char *)argv),
			};
			appl_table_entry_lookup (&lp_id, &v);
		}
	}
	(*appl) = v;

	return 0;
}


