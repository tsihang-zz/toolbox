#include "oryx.h"
#include "prefix.h"
#include "util_ipset.h"


static __oryx_always_inline__
void appl_inherit(struct appl_t *son, struct appl_t *father)
{
	uint32_t id = appl_id(son);

	/** inherit all data from father application. */
	memcpy (son, father, sizeof(struct appl_t));

	/** except THE ID and PRIORITY. */
	appl_id(son) = id;
	son->priority = id;
}

int appl_entry_format (struct appl_t *appl,
	const u32 __oryx_unused__*rule_id,
	const char *unused_var,
	const char __oryx_unused__*vlan,
	const char __oryx_unused__*sip,
	const char __oryx_unused__*dip,
	const char __oryx_unused__*sp,
	const char __oryx_unused__*dp,
	const char __oryx_unused__*proto)
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
			const char *alias, u32 __oryx_unused__ type)
{
	/** create an appl */
	(*appl) = kmalloc (sizeof (struct appl_t), MPF_CLR, __oryx_unused_val__);
	ASSERT ((*appl));
	
	/** make appl alias. */
	sprintf ((char *)&(*appl)->sc_alias[0], "%s", ((alias != NULL) ? alias: APPL_PREFIX));
	(*appl)->ul_flags			=	(APPL_CHANGED);
	(*appl)->ul_type			=	APPL_TYPE_STREAM;
	(*appl)->ul_id				=	APPL_INVALID_ID;
	(*appl)->ull_create_time	=	time(NULL);
	(*appl)->vlan_id			=	0;
	(*appl)->l4_port_src		=	0;
	(*appl)->l4_port_dst		=	0;
	(*appl)->ip_next_proto		=	0;
	(*appl)->l2_vlan_id_mask	=	ANY_VLAN;
	(*appl)->ip_next_proto_mask	=	ANY_PROTO;
	(*appl)->l4_port_src_mask	=	ANY_PORT;
	(*appl)->l4_port_dst_mask	=	ANY_PORT;
	(*appl)->priority			=	0;	/** Min PRIORITY in DPDK. 
										 *  Defaulty, the priority is the same with its ID. */

}

int appl_entry_del (vlib_appl_main_t *am, struct appl_t *appl)
{
	if (!appl ||
		(appl && 
		appl_id(appl) == APPL_INVALID_ID/** Delete appl from oryx_vector */)) {
		return 0;
	}

	do_mutex_lock (&am->lock);
	int r = oryx_htable_del(am->htable,
				(ht_value_t)appl_alias(appl), strlen((const char *)appl_alias(appl)));
	if (r == 0 /** success */) {
		appl->ul_flags &= ~APPL_VALID;
		am->nb_appls --;

		/** do not set appl_id to APPL_INVALID_ID here, because appl_inherit. */
	}
	do_mutex_unlock (&am->lock);
	
	/** Should you free appl here ? */
	return 0;  
}

int appl_entry_add (vlib_appl_main_t *am, struct appl_t *appl)
{
	do_mutex_lock (&am->lock);
	int r = 0;
	int each;
	struct appl_t *son = NULL, *a = NULL;

	/** lookup for an empty slot for this application. */
	vec_foreach_element(am->entry_vec, each, a) {
		if (unlikely(!a))
			continue;
		if (!(a->ul_flags & APPL_VALID)) {
			son = a;
			break;
		}
	}

	if (son) {			
		/** if there is an unused application, update its data with formatted appl */
		appl_inherit(son, appl);
		
		/** Add appl_alias(appl) to hash table for controlplane fast lookup */
		r = oryx_htable_add(am->htable, appl_alias(son), strlen((const char *)appl_alias((son))));
		if (r != 0)
			goto finish;

		son->ul_flags |= APPL_VALID;
		am->nb_appls ++;
		kfree(appl);
	
	} else {
		/** Add appl_alias(appl) to hash table for controlplane fast lookup */
		r = oryx_htable_add(am->htable, (ht_value_t)appl_alias(appl), strlen((const char *)appl_alias(appl)));
		if (r != 0)
			goto finish;
		
		/** else, set this application to vector. */
		appl_id(appl) = vec_set (am->entry_vec, appl);
		appl->priority = appl_id(appl);
		appl->ul_flags |= APPL_VALID;
		am->nb_appls ++;
	}

finish:
	do_mutex_unlock (&am->lock);
	return r;
}

int appl_table_entry_deep_lookup(const char *argv, 
				struct appl_t **appl)
{	
	struct appl_t *v;
	(*appl) = NULL;
	
	struct prefix_t lp_al = {
		.cmd = LOOKUP_ALIAS,
		.s = strlen (argv),
		.v = (void *)argv,
	};

	appl_table_entry_lookup (&lp_al, &v);
	if (likely(v)) {
		(*appl) = v;
	} else {
		/** try id lookup if alldigit input. */
		if (isalldigit (argv)) {
			u32 id = atoi(argv);
			struct prefix_t lp_id = {
				.cmd = LOOKUP_ID,
				.v = (void *)&id,
				.s = strlen (argv),
			};
			appl_table_entry_lookup (&lp_id, &v);
			(*appl) = v;
		}
	}
	

	return 0;
}


