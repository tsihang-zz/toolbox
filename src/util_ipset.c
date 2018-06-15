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
	struct prefix_ipv4 ip4;
	uint32_t val_start, val_end;

	BUG_ON(appl == NULL);
	
	if (vlan) {
		/** default is ANY_VLAN */
		if(!strncmp(vlan, "a", 1)) {
			appl->l2_vlan_id_mask = ANY_VLAN;
		} else {
			if (isalldigit (vlan)) {
				appl->vlan_id = appl->l2_vlan_id_mask = atoi (vlan);
			} else {
				if(!format_range(vlan, 2048, 0, ':', &val_start, &val_end)) {
					appl->vlan_id = val_start;
					appl->l2_vlan_id_mask = val_end;
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
			appl->ip_src_mask = ANY_IPADDR;
		}else {
			str2prefix_ipv4 (sip, &ip4);
			appl->ip_src = ntoh32(ip4.prefix.s_addr);
			appl->ip_src_mask = ip4.prefixlen;
		}
	}
	
	if (dip) {
		if(!strncmp(dip, "a", 1)) {
			appl->ip_dst_mask = ANY_IPADDR;
		}else {
			str2prefix_ipv4 (dip, &ip4);
			appl->ip_dst = ntoh32(ip4.prefix.s_addr);
			appl->ip_dst_mask = ip4.prefixlen;
		}
	}

	/** TCP/UDP Port. */
	if (sp) {
		/** default is ANY_PORT */
		if (!strncmp(sp, "a", 1)) {
			appl->l4_port_src_mask = ANY_PORT; /** To avoid warnings. */
		} else {
			/** single port */
			if (isalldigit (sp)) {
				appl->l4_port_src = appl->l4_port_src_mask = atoi (sp);
			}
			/** range */
			else {
				if(!format_range(sp, UINT16_MAX, 0, ':', &val_start, &val_end)) {
					appl->l4_port_src = val_start;
					appl->l4_port_src_mask = val_end;
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
			appl->l4_port_dst_mask = ANY_PORT;
		} else {
			if (isalldigit (dp)) {
				appl->l4_port_dst = appl->l4_port_dst_mask= atoi (dp);
			}
			else {
				if(!format_range(dp, UINT16_MAX, 0, ':', &val_start, &val_end)) {
					appl->l4_port_dst = val_start;
					appl->l4_port_dst_mask = val_end;
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
			appl->ip_next_proto_mask = ANY_PROTO;
		} else {
			if (isalldigit (proto)) {
				appl->ip_next_proto  = atoi (proto);
				appl->ip_next_proto_mask = 0xFF;
			}
			else {
				if(!format_range (proto, UINT8_MAX, 0, '/', &val_start, &val_end)) {
					appl->ip_next_proto = val_start;
					appl->ip_next_proto_mask = val_end;
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
	(*appl)->ul_type			= APPL_TYPE_STREAM;
	(*appl)->ul_id				= APPL_INVALID_ID;
	(*appl)->ull_create_time	= time(NULL);
	(*appl)->vlan_id			= 0;
	(*appl)->l4_port_src		= 0;
	(*appl)->l4_port_dst		= 0;
	(*appl)->ip_next_proto		= 0;
	(*appl)->l2_vlan_id_mask	= ANY_VLAN;
	(*appl)->ip_next_proto_mask	= ANY_PROTO;
	(*appl)->l4_port_src_mask	= ANY_PORT;
	(*appl)->l4_port_dst_mask	= ANY_PORT;
	(*appl)->priority			= 0;	/** Min PRIORITY in DPDK. 
										 *  Defaulty, the priority is the same with its ID. */

}

int appl_entry_del (vlib_appl_main_t *am, struct appl_t *appl)
{
	if (!appl ||
		(appl && 
		appl_id(appl) == APPL_INVALID_ID/** Delete appl from oryx_vector */)) {
		return 0;
	}
	
	do_lock (&am->lock);
	int r = oryx_htable_del(am->htable,
				(ht_value_t)appl_alias(appl), strlen((const char *)appl_alias(appl)));
	if (r == 0 /** success */) {
		//vec_unset (am->entry_vec, appl_id(appl));
		appl->ul_flags &= ~APPL_VALID;
	}
	do_unlock (&am->lock);
	
	/** Should you free appl here ? */
	appl->ul_flags &= ~APPL_VALID;
	//kfree (appl);

	return 0;  
}

int appl_entry_add (vlib_appl_main_t *am, struct appl_t *appl)
{
	do_lock (&am->lock);
	
	/** Add appl_alias(appl) to hash table for controlplane fast lookup */
	int r = oryx_htable_add(am->htable, appl_alias(appl), strlen((const char *)appl_alias(appl)));
	if (r == 0 /** success*/) {

		if(vec_active(am->entry_vec) == 0) {
			appl_id(appl) = vec_set (am->entry_vec, appl);
			appl->priority = appl_id(appl);
			appl->ul_flags |= APPL_VALID;
		}
		else {
			int each;
			struct appl_t *a = NULL;
			vec_foreach_element(am->entry_vec, each, a) {
				if (unlikely(!a))
					continue;
				if (!(a->ul_flags & APPL_VALID)) {
					uint32_t id = appl_id(a);
					memcpy (a, appl, sizeof(struct appl_t));
					appl_id(a) = id;
					appl->priority = appl_id(appl);
					a->ul_flags |= APPL_VALID;
					kfree(appl);
					goto finish;
				}
			}
			appl_id(appl) = vec_set (am->entry_vec, appl);
			appl->priority = appl_id(appl);
			appl->ul_flags |= APPL_VALID;
		}
	}
	
finish:
	do_unlock (&am->lock);
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


