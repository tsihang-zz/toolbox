#include "oryx.h"
#include "prefix.h"
#include "util_ipset.h"

int appl_entry_format (struct appl_t *appl,
	u32 __oryx_unused__*rule_id, const char *unused_var,
	char __oryx_unused__*vlan,
	char __oryx_unused__*sip,
	char __oryx_unused__*dip,
	char __oryx_unused__*sp,
	char __oryx_unused__*dp,
	char __oryx_unused__*proto)
{
	struct appl_signature_t *as;
	struct prefix_ipv4 ip4;
	uint32_t val_start, val_end;

	BUG_ON(appl == NULL);
	
	as = appl->instance;
	BUG_ON(as == NULL);

	if (vlan) {
		/** default is ANY_VLAN */
		if(!strncmp(vlan, "a", 1)) {
			as->l2_vlan_id_mask = ANY_VLAN;
		} else {
			if (isalldigit (vlan)) {
				as->vlan_id = as->l2_vlan_id_mask = atoi (vlan);
			} else {
				if(!format_range(vlan, 2048, 0, ':', &val_start, &val_end)) {
					as->vlan_id = val_start;
					as->l2_vlan_id_mask = val_end;
				} else {
					oryx_logn("range error \"%s\" %d %d", vlan, val_start, val_end);
					return -1;
				}
			}
		}
	}

	/**  */
	if (sip) {
		if(!strncmp(sip, "a", 1)) {
			as->ip_src_mask = ANY_IPADDR;
		}else {
			str2prefix_ipv4 (sip, &ip4);
			as->ip_src = ntoh32(ip4.prefix.s_addr);
			as->ip_src_mask = ip4.prefixlen;
		}
	}
	
	if (dip) {
		if(!strncmp(dip, "a", 1)) {
			as->ip_dst_mask = ANY_IPADDR;
		}else {
			str2prefix_ipv4 (dip, &ip4);
			as->ip_dst = ntoh32(ip4.prefix.s_addr);
			as->ip_dst_mask = ip4.prefixlen;
		}
	}

	/** TCP/UDP Port. */
	if (sp) {
		/** default is ANY_PORT */
		if (!strncmp(sp, "a", 1)) {
			as->l4_port_src_mask = ANY_PORT; /** To avoid warnings. */
		} else {
			/** single port */
			if (isalldigit (sp)) {
				as->l4_port_src = as->l4_port_src_mask = atoi (sp);
			}
			/** range */
			else {
				if(!format_range(sp, UINT16_MAX, 0, ':', &val_start, &val_end)) {
					as->l4_port_src = val_start;
					as->l4_port_src_mask = val_end;
				} else {
					oryx_logn("range error \"%s\" %d %d", sp, val_start, val_end);
					return -1;
				}
			}
		}
	}
	
	if (dp) {
		/** default is ANY_PORT */
		if (!strncmp(dp, "a", 1)) {
			as->l4_port_dst_mask = ANY_PORT;
		} else {
			if (isalldigit (dp)) {
				as->l4_port_dst = as->l4_port_dst_mask= atoi (dp);
			}
			else {
				if(!format_range(dp, UINT16_MAX, 0, ':', &val_start, &val_end)) {
					as->l4_port_dst = val_start;
					as->l4_port_dst_mask = val_end;
				}else {
					oryx_logn("range error \"%s\" %d %d", dp, val_start, val_end);
					return -1;
				}
			}
		}
	}
	
	/** Format TCP/UDP Protocol. */
	if (proto) {
		/** default is ANY_PROTO */
		if (!strncmp(proto, "a", 1)) {
			as->ip_next_proto_mask = ANY_PROTO;
		} else {
			if (isalldigit (proto)) {
				as->ip_next_proto  = as->ip_next_proto_mask= atoi (proto);
			}
			else {
				if(!format_range (proto, UINT8_MAX, 0, ':', &val_start, &val_end)) {
					as->ip_next_proto = val_start;
					as->ip_next_proto_mask = val_end;
				}else {
					oryx_logn("range error \"%s\" %d %d", proto, val_start, val_end);
					return -1;
				}
			}
		}
	}

	return 0;
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
	as->vlan_id = 0;
	as->l4_port_src = 0;
	as->l4_port_dst = 0;
	as->ip_next_proto = 0;
	as->l2_vlan_id_mask = ANY_VLAN;
	as->ip_next_proto_mask = ANY_PROTO;
	as->l4_port_src_mask = ANY_PORT;
	as->l4_port_dst_mask = ANY_PORT;
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
	int r = oryx_htable_del(vp->htable, (ht_value_t)appl_alias(appl), strlen((const char *)appl_alias(appl)));
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

int appl_table_entry_deep_lookup(const char *argv, 
				struct appl_t **appl)
{	
	struct appl_t *v;
	(*appl) = NULL;
	
	struct prefix_t lp_al = {
		.cmd = LOOKUP_ALIAS,
		.s = strlen ((char *)argv),
		.v = (char *)argv,
	};

	appl_table_entry_lookup (&lp_al, &v);
	if (likely(v)) {
		(*appl) = v;
	} else {
		/** try id lookup if alldigit input. */
		if (isalldigit ((char *)argv)) {
			u32 id = atoi((char *)argv);
			struct prefix_t lp_id = {
				.cmd = LOOKUP_ID,
				.v = (void *)&id,
				.s = strlen ((char *)argv),
			};
			appl_table_entry_lookup (&lp_id, &v);
			(*appl) = v;
		}
	}
	

	return 0;
}


