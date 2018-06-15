#ifndef APPL_PRIVATE_H
#define APPL_PRIVATE_H

#define APPL_PREFIX	"appl"
#define MAX_APPLICATIONS (1 << 6)	/** 1 million */
#define APPL_INVALID_ID		(MAX_APPLICATIONS)

//#define APPL_DUMP_BANDARY		"%-16s%-16d%-"
#define foreach_appl_type			\
  _(STREAM, "Stream application, 7 tuple.")	\
  _(KEYWORD, "Keyword application")
  
enum appl_type_t {
#define _(f,s) APPL_TYPE_##f,
	foreach_appl_type
#undef _
	APPL_N_TYPES,
};

enum {
	HD_SRC,
	HD_DST,
	SRC_DST,
};

/** vlan=0 is reserved by system, we use this for "any" VLAN.*/
#define ANY_VLAN	(0)
#define ANY_PROTO	(0)
#define ANY_PORT	(0)
#define ANY_IPADDR	(0)

/** flags for appl_t.ul_flags */
#define	APPL_SYNCED	(1 << 0)
#define APPL_VALID	(1 << 1)

struct appl_priv_t {
	uint32_t			ul_flags;		/** MAX_MAPS */
	uint32_t			ul_map_id;		/** map id this application belong to. */
};

struct appl_t {
	char				sc_alias[32];		/** Unique, and can be well human-readable. */
	uint32_t			ul_id;				/** Unique, and can be allocated by a oryx_vector function automatically. */
	uint32_t			ul_type;			/** We may define as many different applications as we want. */
	uint32_t			ul_flags;			/** */
	uint8_t				uc_keyword_encrypt[256];	/** RC4 encrypt keyword. */
	char				*sc_keyword;		/** Keyword before enctypt. */
	uint64_t			ull_create_time;
	os_lock_t			ol_lock;

	uint32_t			ul_map_mask;		/** map for this application belong to. */
	struct appl_priv_t	priv[32];			/** private data for this application. */
	uint32_t			nb_maps;			/** number of maps in appl_priv */


	uint32_t			vlan_id	:				12;
	uint32_t			l2_vlan_id_mask	:		12;
	uint32_t			pad0 :					8;
	uint32_t			priority;
	uint32_t			ip_src;
	uint32_t			ip_dst;	
	uint32_t			ip_src_mask	:			8;
	uint32_t			ip_dst_mask	:			8;
	uint32_t			ip_next_proto:			8;
	uint32_t			ip_next_proto_mask:		8;	
	uint16_t			l4_port_src;
	uint16_t			l4_port_src_mask;
	uint16_t			l4_port_dst;		/** */
	uint16_t			l4_port_dst_mask;	/** if l4_port_dst is */

};

#define appl_id(appl) ((appl)->ul_id)
#define appl_alias(appl) ((appl)->sc_alias)

#define VLIB_AM_XXXXXXXXXX		(1 << 0)
typedef struct {
	uint32_t				ul_flags;
	int						ul_n_appls;
	os_lock_t				lock;
	oryx_vector				entry_vec;
	struct oryx_htable_t 	*htable;
	uint32_t				nb_appls;
}vlib_appl_main_t;

extern vlib_appl_main_t vlib_appl_main;

static __oryx_always_inline__
void appl_entry_lookup_alias (vlib_appl_main_t *am, const char *alias, struct appl_t **appl)
{
	BUG_ON(alias == NULL);
	void *s = oryx_htable_lookup (am->htable, (ht_value_t)alias,
						strlen((const char *)alias));
	if (s) {
		(*appl) = (struct appl_t *) container_of (s, struct appl_t, sc_alias);
	}
}

static __oryx_always_inline__
void appl_entry_lookup_id (vlib_appl_main_t *am, u32 id, struct appl_t **appl)
{
	BUG_ON(am->entry_vec == NULL);
	
	if (!vec_active(am->entry_vec) ||
		id > vec_active(am->entry_vec))
		return;

	(*appl) = NULL;
	(*appl) = (struct appl_t *)vec_lookup (am->entry_vec, id);
}

static __oryx_always_inline__
void appl_table_entry_lookup (struct prefix_t *lp, 
				struct appl_t **appl)
{
	vlib_appl_main_t *am = &vlib_appl_main;

	ASSERT (lp);
	ASSERT (appl);
	(*appl) = NULL;
	
	switch (lp->cmd) {
		case LOOKUP_ID:
			appl_entry_lookup_id(am, (*(u32*)lp->v), appl);
			break;
		case LOOKUP_ALIAS:
			appl_entry_lookup_alias(am, (const char*)lp->v, appl);
			break;
		default:
			break;
	}
}

#endif

