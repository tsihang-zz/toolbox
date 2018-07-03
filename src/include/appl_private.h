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

#define	ACT_DROP		(1 << 0)
#define ACT_FWD			(1 << 1)
#define ACT_TIMESTAMP	(1 << 2)
#define ACT_MIRROR		(1 << 3)
#define ACT_DE_VxLAN	(1 << 4)
#define ACT_EN_VxLAN	(1 << 5)
#define ACT_DE_GRE		(1 << 6)
#define ACT_EN_GRE		(1 << 7)
#define ACT_SLICE		(1 << 8)
#define ACT_DEFAULT		(ACT_DROP)

union egress_options {
	struct {
		uint32_t v;
	}act;
	
	uint32_t data32;
}egress_options;

struct appl_priv_t {
	uint32_t			ul_flags;				/** egress_options */
	uint32_t			ul_slice_size	: 12;	/** Slicing size */
	uint32_t			pad0			: 20;
	uint32_t			ul_map_id;				/** map id this application belong to. */
}__attribute__((__packed__));

/** vlan=0 is reserved by system, we use this for "any" VLAN.*/
#define ANY_VLAN	(0)
#define ANY_PROTO	(0)
#define ANY_PORT	(0)
#define ANY_IPADDR	(0)

/** flags for appl_t.ul_flags */
#define APPL_VALID		(1 << 0)
#define	APPL_CHANGED	(1 << 1)

struct appl_t {
	char				sc_alias[32];		/** Unique, and can be well human-readable. */
	uint32_t			ul_id;				/** Unique, and can be allocated by a oryx_vector function automatically. */
	uint32_t			ul_type;			/** We may define as many different applications as we want. */
	uint32_t			ul_flags;			/** */
	uint8_t				uc_keyword_encrypt[256];	/** RC4 encrypt keyword. */
	char				*sc_keyword;		/** Keyword before enctypt. */
	uint64_t			ull_create_time;
	os_mutex_t			ol_lock;

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

}__attribute__((__packed__));

#define appl_id(appl)		((appl)->ul_id)
#define appl_alias(appl)	((appl)->sc_alias)

#define VLIB_AM_XXXXXXXXXX		(1 << 0)
typedef struct {
	uint32_t				ul_flags;
	os_mutex_t				lock;
	oryx_vector				entry_vec;
	struct oryx_htable_t 	*htable;
	uint32_t				nb_appls;
	void					*vm;
}vlib_appl_main_t;

extern vlib_appl_main_t vlib_appl_main;

#define appl_is_invalid(appl)\
	(unlikely(!(appl)) || !((appl)->ul_flags & APPL_VALID))
	
static __oryx_always_inline__
int appl_is_inuse (struct appl_t *appl)
{
	return ((appl->ul_flags & APPL_VALID) && (appl->ul_map_mask != 0));
}

static __oryx_always_inline__
int appl_is_unused (struct appl_t *appl)
{
	return !appl_is_inuse(appl);
}

static __oryx_always_inline__
void appl_entry_lookup_alias (vlib_appl_main_t *am, const char *alias, struct appl_t **appl)
{
	BUG_ON(alias == NULL);
	void *s = oryx_htable_lookup (am->htable, (ht_value_t)alias,
						strlen(alias));
	if (s) {
		(*appl) = (struct appl_t *) container_of (s, struct appl_t, sc_alias);
	}
}

static __oryx_always_inline__
void appl_entry_lookup_id0 (vlib_appl_main_t *am, u32 id, struct appl_t **appl)
{
	BUG_ON(am->entry_vec == NULL);

	(*appl) = NULL;
	if (!vec_active(am->entry_vec) ||
		id > vec_active(am->entry_vec))
		return;

	(*appl) = (struct appl_t *)vec_lookup (am->entry_vec, id);
}

#if defined(BUILD_DEBUG)
#define appl_entry_lookup_id(am,id,appl)\
	appl_entry_lookup_id0(am,id,appl);
#else
#define appl_entry_lookup_id(am,id,appl)\
		(*(appl)) = NULL;\
		(*(appl)) = (struct appl_t *)vec_lookup ((am)->entry_vec, (id));
#endif

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
			appl_entry_lookup_id0(am, (*(u32*)lp->v), appl);
			break;
		case LOOKUP_ALIAS:
			appl_entry_lookup_alias(am, (const char*)lp->v, appl);
			break;
		default:
			break;
	}
}

#endif
