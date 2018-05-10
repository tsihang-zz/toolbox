#ifndef __USR_DEFINED_PATTERN_H__
#define __USR_DEFINED_PATTERN_H__


#define UDP_PREFIX	"udp"
/** Maximum UDP entries. */
#define UDP_N_ELEMENTS (1 << 3)	/** 1 million */
/** Maximum patterns in a udp entry. */
#define UDP_N_PATTERNS		128
#define UDP_INVALID_ID		(UDP_N_ELEMENTS)

#define foreach_pattern_flags	\
  _(NOCASE, 0, "Case sensitive pattern.") \
  _(INSTALLED, 1, "Has this pattern already been installed to MPM.")\
  _(RESV, 1, "Resv.")

enum pattern_flags_t {
#define _(f,o,s) PATTERN_FLAGS_##f = (1<<o),
	foreach_pattern_flags
#undef _
	PATTERN_FLAGS,
};

#define PATTERN_SIZE	128
#define PATTERN_DEFAULT_FLAGS	(PATTERN_FLAGS_NOCASE/** defaultly */)
#define PATTERN_DEFAULT_OFFSET	64
#define PATTERN_DEFAULT_DEPTH		128
#define PATTERN_INVALID_ID		(UDP_N_PATTERNS)
struct pattern_t {

	/** ID of current pattern. */
	u32 ul_id;

	/** Pattern length. */
	size_t ul_size;

	/** Pattern Value. */
	char sc_val[PATTERN_SIZE];
	
	/** Defined by PATTERN_FLAGS_XXXX. */
	u32 ul_flags;

	/** Offset where this pattern first appears in a packet. */
	u16 us_offset;

	/** Depth of search for this pattern in a packet. */
	u16 us_depth;

	/** Statistics for this pattern. */
	struct stats_trunk_t hits;
};

#define PATTERN_HIT(hits,m) (hits.m)

#define PATTERN_STATS_B(pt)\
	PATTERN_HIT(((pt)->hits), bytes)

#define PATTERN_STATS_P(pt)\
	PATTERN_HIT(((pt)->hits), packets)

#define PATTERN_STATS_INC_BYTE(pt, v)\
	(PATTERN_STATS_B(pt) += v)

#define PATTERN_STATS_INC_PKT(pt, v)\
	(PATTERN_STATS_P(pt) += v)
	

/** User Defined Pattern.*/
struct udp_t {
	/** Unique, and can be well human-readable. */
	char sc_alias[32];
	/** Unique, and can be allocated by a vector function automatically. */
	u32 ul_id;
	/** Patterns stored by a vector. */
	vector patterns;
	/** Unused. */
	u32 ul_flags;	
	/** Datapath Output is not defined. */
	u32 ul_dpo;

	/** Create time. */
	u64 ull_create_time;

	/** Which map this udp_t belongs to. */
	vector qua[QUAS];
};
#define udp_slot(u) ((u)->ul_id)
#define udp_alias(u) ((u)->sc_alias)

extern int udp_init (void);

extern int udp_entry_new (struct udp_t **udp, char *alias);
extern int udp_entry_add (struct udp_t *udp);

extern struct udp_t *udp_entry_lookup_id (u32 id);
extern struct udp_t *udp_entry_lookup (char *alias);
extern int udp_entry_remove (struct udp_t *udp);
extern int udp_entry_destroy (struct udp_t *udp);

extern int udp_pattern_new (struct pattern_t **pattern);
extern struct pattern_t *udp_entry_pattern_lookup (struct udp_t *udp, char *pattern, size_t pl);
extern struct pattern_t *udp_entry_pattern_lookup_id (struct udp_t *udp, u32 id);
extern int udp_entry_pattern_add (struct pattern_t *pat, struct udp_t *udp);
extern int udp_entry_pattern_remove (struct pattern_t *pat, struct udp_t *udp);

#if defined(HAVE_QUAGGA)

extern vector udp_vector_table;
extern atomic_t n_udp_elements;

#define split_foreach_udp_func1_param0(argv_x, func)\
	char *split = ",";/** split tokens */\
	char *token = NULL;\
	char *save = NULL;\
	char alias_list[128] = {0};\
	int foreach_element;\
	atomic_set(&n_udp_elements, 0);\
	memcpy (alias_list, (char *)argv_x, strlen ((char *)argv_x));\
	token = strtok_r (alias_list, split, &save);\
	while (token) {\
		if (token) {\
			struct udp_t *v = NULL;\
			if (isalldigit (token)) {\
				/** Lookup by ID. */\
				v = udp_entry_lookup_id (atoi(token));\
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
					v = udp_entry_lookup (token);\
					if (!v) {\
						goto lookup_next;\
					}\
				}\
			}\
			if (v) {\
				func (v);\
				atomic_inc(&n_udp_elements);\
				goto lookup_next;\
			}\
lookup_by_alias_posted_fuzzy:\
			/** lookup alias with Post-Fuzzy match */\
			vector_foreach_element(udp_vector_table, foreach_element, v){\
				if (v && !strncmp (v->sc_alias, token, (strlen(token) - 1/** ignore '*'  */))){\
					func (v);\
					atomic_inc(&n_udp_elements);\
				}\
			}\
		}\
lookup_next:\
		token = strtok_r (NULL, split, &save);\
	}

#define split_foreach_udp_func1_param1(argv_x, func, param0)\
	char *split = ",";/** split tokens */\
	char *token = NULL;\
	char *save = NULL;\
	char alias_list[128] = {0};\
	int foreach_element;\
	atomic_set(&n_udp_elements, 0);\
	memcpy (alias_list, (char *)argv_x, strlen ((char *)argv_x));\
	token = strtok_r (alias_list, split, &save);\
	while (token) {\
		if (token) {\
			struct udp_t *v = NULL;\
			if (isalldigit (token)) {\
				/** Lookup by ID. */\
				v = udp_entry_lookup_id (atoi(token));\
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
					v = udp_entry_lookup (token);\
					if (!v) {\
						goto lookup_next;\
					}\
				}\
			}\
			if (v) {\
				func (v, param0);\
				atomic_inc(&n_udp_elements);\
				goto lookup_next;\
			}\
lookup_by_alias_posted_fuzzy:\
			/** lookup alias with Post-Fuzzy match */\
			vector_foreach_element(udp_vector_table, foreach_element, v){\
				if (v && !strncmp (v->sc_alias, token, (strlen(token) - 1/** ignore '*'  */))){\
					func (v, param0);\
					atomic_inc(&n_udp_elements);\
				}\
			}\
		}\
lookup_next:\
		token = strtok_r (NULL, split, &save);\
	}


#define split_foreach_udp_func1_param2(argv_x, func, param0, param_1)\
	char *split = ",";/** split tokens */\
	char *token = NULL;\
	char *save = NULL;\
	char alias_list[128] = {0};\
	int foreach_element;\
	atomic_set(&n_udp_elements, 0);\
	memcpy (alias_list, (char *)argv_x, strlen ((char *)argv_x));\
	token = strtok_r (alias_list, split, &save);\
	while (token) {\
		if (token) {\
			struct udp_t *v = NULL;\
			if (isalldigit (token)) {\
				/** Lookup by ID. */\
				v = udp_entry_lookup_id (atoi(token));\
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
					v = udp_entry_lookup (token);\
					if (!v) {\
						goto lookup_next;\
					}\
				}\
			}\
			if (v) {\
				func (v, param0, param_1);\
				atomic_inc(&n_udp_elements);\
				goto lookup_next;\
			}\
lookup_by_alias_posted_fuzzy:\
			/** lookup alias with Post-Fuzzy match */\
			vector_foreach_element(udp_vector_table, foreach_element, v){\
				if (v && !strncmp (v->sc_alias, token, (strlen(token) - 1/** ignore '*'  */))){\
					func (v, param0, param_1);\
					atomic_inc(&n_udp_elements);\
				}\
			}\
		}\
lookup_next:\
		token = strtok_r (NULL, split, &save);\
	}


#define split_foreach_udp_func2_param1(argv_x, func1, func1_param0, func2, func2_param0)\
	char *split = ",";/** split tokens */\
	char *token = NULL;\
	char *save = NULL;\
	char alias_list[128] = {0};\
	int foreach_element;\
	atomic_set(&n_udp_elements, 0);\
	memcpy (alias_list, (char *)argv_x, strlen ((char *)argv_x));\
	token = strtok_r (alias_list, split, &save);\
	while (token) {\
		if (token) {\
			struct udp_t *v = NULL;\
			if (isalldigit (token)) {\
				/** Lookup by ID. */\
				v = udp_entry_lookup_id (atoi(token));\
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
					v = udp_entry_lookup (token);\
					if (!v) {\
						goto lookup_next;\
					}\
				}\
			}\
			if (v) {\
				func1(v, func1_param0);\
				func2 (v, func2_param0);\
				atomic_inc(&n_udp_elements);\
				goto lookup_next;\
			}\
lookup_by_alias_posted_fuzzy:\
			/** lookup alias with Post-Fuzzy match */\
			vector_foreach_element(udp_vector_table, foreach_element, v){\
				if (v && !strncmp (v->sc_alias, token, (strlen(token) - 1/** ignore '*'  */))){\
					func1(v, func1_param0);\
					func2 (v, func2_param0);\
					atomic_inc(&n_udp_elements);\
				}\
			}\
		}\
lookup_next:\
		token = strtok_r (NULL, split, &save);\
	}


#define split_foreach_udp_pattern_func1_param1(argv_x, func, param0)\
	char *split = ",";/** split tokens */\
	char *token = NULL;\
	char *save = NULL;\
	char alias_list[128] = {0};\
	atomic_set(&n_udp_elements, 0);\
	memcpy (alias_list, (char *)argv_x, strlen ((char *)argv_x));\
	token = strtok_r (alias_list, split, &save);\
	while (token) {\
		if (token) {\
			struct pattern_t *v = NULL;\
			if (isalldigit (token)) {\
				/** Lookup by ID. */\
				v = udp_entry_pattern_lookup_id (param0, atoi(token));\
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
					goto lookup_next;\
					/** NOT SUPPORTED. */\
					v = udp_entry_pattern_lookup (param0, NULL, -1);\
					if (!v) {\
						goto lookup_next;\
					}\
				}\
			}\
			if (v) {\
				func (v, param0);\
				atomic_inc(&n_udp_elements);\
				goto lookup_next;\
			}\
lookup_by_alias_posted_fuzzy:\
			goto lookup_next;\
		}\
lookup_next:\
		token = strtok_r (NULL, split, &save);\
	}
				

#define foreach_udp_func1_param0(argv_x, func)\
	int foreach_element;\
	atomic_set(&n_udp_elements, 0);\
	struct udp_t *v;\
	vector_foreach_element(udp_vector_table, foreach_element, v){\
		if (v) {\
			func (v);\
			atomic_inc(&n_udp_elements);\
		}\
	}

#define foreach_udp_func1_param1(argv_x, func, param0)\
	int foreach_element;\
	atomic_set(&n_udp_elements, 0);\
	struct udp_t *v;\
	vector_foreach_element(udp_vector_table, foreach_element, v){\
		if (v) {\
			func (v, param0);\
			atomic_inc(&n_udp_elements);\
			v = NULL;\
		}\
	}
	
#endif

#endif
