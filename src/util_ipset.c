#include "oryx.h"
#include "prefix.h"
#include "appl_private.h"
#include "util_ipset.h"
#include "rfc.h"

void appl_entry_format (struct appl_t *appl, 
	u32 __oryx_unused__*rule_id, char *unused_var, 
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
			as->us_port[p]  = atoi (sp);
		}
		else {
			/** default is ANY_PORT */
			as->us_port[p] = ANY_PORT; /** To avoid warnings. */
		}
	}
	
	if (dp) {
		u8 p = HD_DST;
		if (isalldigit (dp)) {
			as->us_port[p]  = atoi (dp);
		}
		else {
			/** default is ANY_PORT */
			as->us_port[p] = ANY_PORT; /** To avoid warnings. */
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
	as->us_port[HD_SRC] = as->us_port[HD_DST] = ANY_PORT;
	as->udp = vec_init (12);
}

void appl_table_entry_lookup (char *alias, struct appl_t **appl)
{
	vlib_appl_main_t *vp = &vlib_appl_main;
	(*appl) = NULL;
	
	if (!alias) return;

	void *s = oryx_htable_lookup (vp->htable,
		alias, strlen((const char *)alias));

	if (s) {
		(*appl) = (struct appl_t *) container_of (s, struct appl_t, sc_alias);
	}
}
void appl_table_entry_lookup_i (u32 id, struct appl_t **appl)
{
	vlib_appl_main_t *vp = &vlib_appl_main;
	(*appl) = NULL;
	(*appl) = vec_lookup (vp->entry_vec, id);
}

int appl_table_entry_del (struct appl_t *appl)
{
	vlib_appl_main_t *vp = &vlib_appl_main;

	if (!appl ||
		(appl && 
		appl->ul_id == APPL_INVALID_ID/** Delete appl from oryx_vector */)) {
		return 0;
	}
	
	if (likely(THIS_ELEMENT_IS_INUNSE(appl))) {
		return 0;
	}
	
	do_lock (&vp->lock);
	int r = oryx_htable_del(vp->htable, appl->sc_alias, strlen((const char *)appl->sc_alias));
	if (r == 0 /** success */) {
		vec_unset (vp->entry_vec, appl->ul_id);
	}
	do_unlock (&vp->lock);
	/** Should you free appl here ? */

	kfree (appl->instance);
	kfree (appl);

	return 0;  
}

int appl_table_entry_add (vlib_appl_main_t *vp, struct appl_t *appl)
{
	BUG_ON(appl->ul_id != APPL_INVALID_ID);

	do_lock (&vp->lock);
	
	/** Add appl->sc_alias to hash table for controlplane fast lookup */
	int r = oryx_htable_add(vp->htable, appl->sc_alias, strlen((const char *)appl->sc_alias));
	if (r == 0 /** success*/) {
		/** Add appl to a oryx_vector table for dataplane fast lookup. */
		appl->ul_id = vec_set (vp->entry_vec, appl);
	}

	do_unlock (&vp->lock);
	return r;
}

#if 0
/*******************************************************************************
 @Function Name:   acl_sync_ruleg_ipv4
 @Description:     sync group's ipv4 five tuple rule into search nodes
 @param: group_id  input:  group id
 @return:          uint8_t RETURN_TRUE:success RETURN_FALSE:failed
********************************************************************************/
uint8_t ipset_flush(vlib_appl_main_t *am, vlib_rfc_main_t *rm)
{
    uint32_t i, com;
    acl_rule_set_t *ruleset;
    int ret;
    uint8_t ip_ver = 4;
    rfc_node_t *search_node = NULL;
	int fe = 0;
	struct appl_t *v = NULL;
    
    search_node = (rfc_node_t*)&rm->search;
    memset(search_node, 0, sizeof(rfc_node_t));

    /* format acl rules */
    ruleset = &rm->ipset;
    ipv4_filtset.n_filts = 0;
    search_node->f2r.n_filt = 0;

	/** foreach appl.instance in signature. */
	vec_foreach_element(appl_vector_table, fe, v){
		if(likely(v)) {
			struct appl_signature_t *as;
			as = v->instance;
			if (likely(as)) {
	/**
    uint8_t       sip_mask;
    uint8_t       dip_mask;
    uint8_t       start_proto;
    uint8_t       end_proto;
    uint32_t      start_sport;
    uint32_t      end_sport;
    uint32_t      start_dport;
    uint32_t      end_dport;
    uint32_t      start_vid;
    uint32_t      end_vid;
    rfc_ip_addr_t start_sip;
    rfc_ip_addr_t end_sip;
    rfc_ip_addr_t start_dip;
    rfc_ip_addr_t end_dip;
    rfc_ip_addr_t sip;
    rfc_ip_addr_t dip;
	*/
#if 0
				struct prefix *p;
				int qua;
				rfc_5tuple_t t;
				memset (&t, 0, sizeof(t));			

				/** format src ip and port */
				qua = HD_SRC;	
				p = &as->ip4[qua];
				t.sip_mask = p->prefixlen;
				t.sip.addr_v4 = p->u.prefix4;
				t.start_sport = t.end_sport = as->us_port[qua];

				/** format dst ip and port */
				qua = HD_DST;	
				p = &as->ip4[qua];
				t.dip_mask = p->prefixlen;
				t.dip.addr_v4 = p->u.prefix4;
				t.start_dport = t.end_dport = as->us_port[qua];

				/** format protocol */
				t.start_proto = t.end_proto = as->uc_proto;
#endif				
			}
		}
	}
	
    /* encode phase0 */
    for (com = 0; com < DIM_P0_MAX; com++)
    {
        ret = rfc_set_phase0_dim_cell(rm, &search_node->p0_node[com], com);
        if (ret < 0) 
        {
            goto sync_error;
        }
    }
    
    /* encode phase1 */
    com = 0;
    ret = rfc_set_phase1_dim_cell(&search_node->p1_node[0], com, ip_ver);
    if (ret < 0) 
    {
        oryx_loge(-1, "rule phase 1 com [%d] cell entry is full\n", com);
        goto sync_error;
    }
    
    com = 1;
    ret = rfc_set_phase1_dim_cell(&search_node->p1_node[1], com, ip_ver);
    if (ret < 0) 
    {
        oryx_loge(-1, "rule phase 1 com [%d] cell entry is full\n", com);
        goto sync_error;
    }
    
    /* encode phase2 */
    ret =  rfc_set_phase2_cell(&search_node->p2_node);
    if (ret < 0) 
    {
        goto sync_error;
    }

    return 1;
sync_error:
    return 0;
}
#endif
