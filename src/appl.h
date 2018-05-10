#ifndef __APPL_H__
#define __APPL_H__

#define APPL_PREFIX	"appl"
#define APPL_N_ELEMENTS (1 << 6)	/** 1 million */
#define APPL_INVALID_ID		(APPL_N_ELEMENTS)

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
	L4_PORT_SRC,
	L4_PORT_DST,
	L4_PORTS,
};

enum {
	L3_IP_ADDR_SRC,
	L3_IP_ADDR_DST,
	L3_IP_ADDR,
};

enum {
	L2_ETHERNET_ADDR_SRC,
	L2_ETHERNET_ADDR_DST,
	L2_ETHERNET,
};

/** vlan=0 is reserved by system, we use this for "any" VLAN.*/
#define ANY_VLAN	(0)
#define ANY_PROTO	(0)
#define ANY_PORT	(0)
#define ANY_IPADDR	(0)

struct appl_signature_t {
	
	/**
	  * Vlan
	  */
	u16 us_vlan;

	/**
	  * ethernet address.
	  */
	/** TODO. */	
	
	/**
	  * ip address for this application.
	  */
	struct prefix_ipv4 ip4[L3_IP_ADDR];

	/**
	  * l4 port for this application.
	  */
	u16 us_port[L4_PORTS];

	/**
	  * Protocol
	  */
	u8 uc_proto;

	/**
	  * User-Defined Pattern for this appl signature.
	  */
	vector udp;
};


/**
  * Default application action.
  */
#define APPL_DEFAULT_ACTIONS	(0)

struct appl_t {

	/** 
	  * Unique, and can be well human-readable.
	  */
	char sc_alias[32];
	
	/** 
	  * Unique, and can be allocated by a vector function automatically. 
	  */
	u32 ul_id;

	/** 
	  * We may define as many different applications as we want.
	  */
	u32 ul_type;

	/**
	  * Application instance point to a TCAM entry or RFC entry.
	  */
	void *instance;

	/** 
	  * Defined by appl_action_type_t.
	  * and default ACTIONS defined for an APPL is in APPL_DEFAULT_ACTIONS. 
	  */
	u32 ul_flags;	

	/**
	  * Datapath Output is not defined. 
	  */
	u32 ul_dpo;

	/**
	  * RC4 encrypt keyword.
	  */
	u8 uc_keyword_encrypt[256];


	/**
	  * Keyword before enctypt.
	  */
	char *sc_keyword;

	/**
	  * Create time.
	  */
	u64 ull_create_time;

	/**
	  * Lock.
	  */
	os_lock_t ol_lock;
};

#define appl_slot(u) ((u)->ul_id)
#define appl_alias(u) ((u)->sc_alias)

#define appl_entry_lock(appl)\
	(pthread_mutex_lock(&appl->ol_lock))
#define appl_entry_unlock(appl)\
	(pthread_mutex_unlock(&appl->ol_lock))

extern struct oryx_htable_t *appl_hash_table;
extern vector appl_vector_table;

extern int
appl_table_entry_add (struct appl_t *appl);

extern int
appl_table_entry_remove (struct appl_t *appl);

extern int
appl_entry_add_udp (struct appl_t *appl, struct udp_t *udp);

extern int
appl_table_entry_destroy (struct appl_t *appl);

extern void
appl_table_entry_lookup (char *alias, struct appl_t **appl);

extern int
appl_table_entry_remove_and_destroy (struct appl_t *appl);

extern struct appl_t *
appl_table_entry_lookup_i (u32 id);

extern void 
appl_entry_new (struct appl_t **appl, char *alias, u32 type);

extern void
appl_entry_destroy (struct appl_t *appl);

extern void 
appl_entry_format (struct appl_t *appl, 
	u32 *rule_id, char *rule_action, 
	char __oryx_unused__*vlan, 
	char __oryx_unused__*sip, 
	char __oryx_unused__*dip, 
	char __oryx_unused__*sp, 
	char __oryx_unused__*dp, 
	char __oryx_unused__*proto);



#if defined(HAVE_QUAGGA)
extern atomic_t n_application_elements;

extern vector appl_vector_table;
	
#define split_foreach_application_func1_param0(argv_x, func)\
		char *split = ",";/** split tokens */\
		char *token = NULL;\
		char *save = NULL;\
		char alias_list[128] = {0};\
		int foreach_element;\
		atomic_set(&n_application_elements, 0);\
		memcpy (alias_list, (char *)argv_x, strlen ((char *)argv_x));\
		token = strtok_r (alias_list, split, &save);\
		while (token) {\
			if (token) {\
				struct appl_t *v = NULL;\
				if (isalldigit (token)) {\
					/** Lookup by ID. */\
					v = appl_table_entry_lookup_i (atoi(token));\
					if (!v) {\
						goto lookup_by_alias_exactly;\
					}\
				} else {\
					/** Lookup by ALIAS fuzzy. */\
					if (strchr (token, '*')){\
						goto lookup_by_alias_posted_fuzzy;\
					}\
					else {\
	lookup_by_alias_exactly:\
						/** Lookup by ALIAS exactly. */\
						appl_table_entry_lookup (token, &v);\
						if (unlikely(!v)) {\
							goto lookup_next;\
						}\
					}\
				}\
				if (v) {\
					func (v);\
					atomic_inc(&n_application_elements);\
					goto lookup_next;\
				}\
	lookup_by_alias_posted_fuzzy:\
				/** lookup alias with Post-Fuzzy match */\
				vector_foreach_element(appl_vector_table, foreach_element, v){\
					if (v && !strncmp (v->sc_alias, token, (strlen(token) - 1/** ignore '*'  */))){\
						func (v);\
						atomic_inc(&n_application_elements);\
					}\
				}\
			}\
	lookup_next:\
			token = strtok_r (NULL, split, &save);\
		}
	
	
#define split_foreach_application_func1_param1(argv_x, func, param0)\
		char *split = ",";/** split tokens */\
		char *token = NULL;\
		char *save = NULL;\
		char alias_list[128] = {0};\
		int foreach_element;\
		atomic_set(&n_application_elements, 0);\
		memcpy (alias_list, (char *)argv_x, strlen ((char *)argv_x));\
		token = strtok_r (alias_list, split, &save);\
		while (token) {\
			if (token) {\
				struct appl_t *v = NULL;\
				if (isalldigit (token)) {\
					/** Lookup by ID. */\
					v = appl_table_entry_lookup_i (atoi(token));\
					if (!v) {\
						goto lookup_by_alias_exactly;\
					}\
				} else {\
					/** Lookup by ALIAS fuzzy. */\
					if (strchr (token, '*')){\
						goto lookup_by_alias_posted_fuzzy;\
					}\
					else {\
	lookup_by_alias_exactly:\
						/** Lookup by ALIAS exactly. */\
						appl_table_entry_lookup (token, &v);\
						if (unlikely(!v)) {\
							goto lookup_next;\
						}\
					}\
				}\
				if (v) {\
					func (v, param0);\
					atomic_inc(&n_application_elements);\
					goto lookup_next;\
				}\
	lookup_by_alias_posted_fuzzy:\
				/** lookup alias with Post-Fuzzy match */\
				vector_foreach_element(appl_vector_table, foreach_element, v){\
					if (v && !strncmp (v->sc_alias, token, (strlen(token) - 1/** ignore '*'  */))){\
						func (v, param0);\
						atomic_inc(&n_application_elements);\
					}\
				}\
			}\
	lookup_next:\
			token = strtok_r (NULL, split, &save);\
		}
	
	
#define split_foreach_application_func1_param2(argv_x, func, param0, param_1)\
		char *split = ",";/** split tokens */\
		char *token = NULL;\
		char *save = NULL;\
		char alias_list[128] = {0};\
		int foreach_element;\
		atomic_set(&n_application_elements, 0);\
		memcpy (alias_list, (char *)argv_x, strlen ((char *)argv_x));\
		token = strtok_r (alias_list, split, &save);\
		while (token) {\
			if (token) {\
				struct appl_t *v = NULL;\
				if (isalldigit (token)) {\
					/** Lookup by ID. */\
					v = appl_table_entry_lookup_i (atoi(token));\
					if (!v) {\
						goto lookup_by_alias_exactly;\
					}\
				} else {\
					/** Lookup by ALIAS fuzzy. */\
					if (strchr (token, '*')){\
						goto lookup_by_alias_posted_fuzzy;\
					}\
					else {\
	lookup_by_alias_exactly:\
						/** Lookup by ALIAS exactly. */\
						appl_table_entry_lookup (token, &v);\
						if (unlikely(!v)) {\
							goto lookup_next;\
						}\
					}\
				}\
				if (v) {\
					func (v, param0, param_1);\
					atomic_inc(&n_application_elements);\
					goto lookup_next;\
				}\
	lookup_by_alias_posted_fuzzy:\
				/** lookup alias with Post-Fuzzy match */\
				vector_foreach_element(appl_vector_table, foreach_element, v){\
					if (v && !strncmp (v->sc_alias, token, (strlen(token) - 1/** ignore '*'  */))){\
						func (v, param0, param_1);\
						atomic_inc(&n_application_elements);\
					}\
				}\
			}\
	lookup_next:\
			token = strtok_r (NULL, split, &save);\
		}
	
	
#define foreach_application_func1_param0(argv_x, func)\
		int foreach_element;\
		atomic_set(&n_application_elements, 0);\
		struct appl_t *v;\
		vector_foreach_element(appl_vector_table, foreach_element, v){\
			if (v) {\
				func (v);\
				atomic_inc(&n_application_elements);\
			}\
		}
	
#define foreach_application_func1_param1(argv_x, func, param0)\
		int foreach_element;\
		atomic_set(&n_application_elements, 0);\
		struct appl_t *v;\
		vector_foreach_element(appl_vector_table, foreach_element, v){\
			if (v) {\
				func (v, param0);\
				atomic_inc(&n_application_elements);\
			}\
		}
	
#endif

#endif
