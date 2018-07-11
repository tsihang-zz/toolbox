#ifndef CLI_MAP_H
#define CLI_MAP_H

extern atomic_t n_map_elements;
extern void vlib_map_init(vlib_main_t *vm);

#define split_foreach_map_func1_param0(argv_x, func) {\
	const char *split = ",";/** split tokens */\
	char *token = NULL;\
	char *save = NULL;\
	char alias_list[128] = {0};\
	int each;\
	oryx_vector vec = vlib_map_main.entry_vec;\
	uint32_t elements_before = vec_count(vec);\
	elements_before = elements_before;\
	atomic_set(&n_map_elements, 0);\
	struct map_t *v = NULL;\
	if (!strcmp (alias_list, "*")) {\
		/** lookup alias with Post-Fuzzy match */\
		vec_foreach_element(vec, each, v){\
			if (v && (v->ul_flags & MAP_VALID)){\
				func (v);\
				atomic_inc(&n_map_elements);\
			}\
		}\
	}  else {\
		memcpy (alias_list, (const void *)argv_x, strlen ((const char *)argv_x));\
		token = strtok_r (alias_list, split, &save);\
		while (token) {\
			if (token) {\
				v = NULL;\
				if (isalldigit (token)) {\
					/** Lookup by ID. */\
					uint32_t id = atoi(token);\
					struct prefix_t lp = {\
						.cmd = LOOKUP_ID,\
						.s = strlen (token),\
						.v = (void *)&id,\
					};\
					map_table_entry_lookup(&lp, &v);\
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
						token = token;\
						/** Lookup by ALIAS exactly. */\
						struct prefix_t lp = {\
							.cmd = LOOKUP_ALIAS,\
							.s = strlen (token),\
							.v = token,\
						};\
						map_table_entry_lookup (&lp, &v);\
						if (unlikely(!v)) {\
							goto lookup_next;\
						}\
					}\
				}\
				if (v && (v->ul_flags & MAP_VALID)) {\
					func (v);\
					atomic_inc(&n_map_elements);\
					goto lookup_next;\
				}\
	lookup_by_alias_posted_fuzzy:\
				/** lookup alias with Post-Fuzzy match */\
				vec_foreach_element(vec, each, v){\
					if (v && (v->ul_flags & MAP_VALID)\
						&& !strncmp (v->sc_alias, token, (strlen(token) - 1/** ignore '*'  */))){\
						func (v);\
						atomic_inc(&n_map_elements);\
					}\
				}\
			}\
	lookup_next:\
			token = strtok_r (NULL, split, &save);\
		}\
	}\
}

#define split_foreach_map_func1_param1(argv_x, func, param0) {\
	const char *split = ",";/** split tokens */\
	char *token = NULL;\
	char *save = NULL;\
	char alias_list[128] = {0};\
	int each;\
	oryx_vector vec = vlib_map_main.entry_vec;\
	uint32_t elements_before = vec_count(vec);\
	elements_before = elements_before;\
	atomic_set(&n_map_elements, 0);\
	struct map_t *v = NULL;\
	if (!strcmp (alias_list, "*")) {\
		/** lookup alias with Post-Fuzzy match */\
		vec_foreach_element(vec, each, v){\
			if (v && (v->ul_flags & MAP_VALID)){\
				func (v, param0);\
				atomic_inc(&n_map_elements);\
			}\
		}\
	}  else {\
		memcpy (alias_list, (const void *)argv_x, strlen ((const char *)argv_x));\
		token = strtok_r (alias_list, split, &save);\
		while (token) {\
			if (token) {\
				v = NULL;\
				if (isalldigit (token)) {\
					/** Lookup by ID. */\
					uint32_t id = atoi(token);\
					struct prefix_t lp = {\
						.cmd = LOOKUP_ID,\
						.s = strlen (token),\
						.v = (void *)&id,\
					};\
					map_table_entry_lookup(&lp, &v);\
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
						token = token;\
						/** Lookup by ALIAS exactly. */\
						struct prefix_t lp = {\
							.cmd = LOOKUP_ALIAS,\
							.s = strlen (token),\
							.v = token,\
						};\
						map_table_entry_lookup (&lp, &v);\
						if (unlikely(!v)) {\
							goto lookup_next;\
						}\
					}\
				}\
				if (v && (v->ul_flags & MAP_VALID)) {\
					func (v, param0);\
					atomic_inc(&n_map_elements);\
					goto lookup_next;\
				}\
	lookup_by_alias_posted_fuzzy:\
				/** lookup alias with Post-Fuzzy match */\
				vec_foreach_element(vec, each, v){\
					if (v && !strncmp (v->sc_alias, token, (strlen(token) - 1/** ignore '*'  */))){\
						func (v, param0);\
						atomic_inc(&n_map_elements);\
					}\
				}\
			}\
	lookup_next:\
			token = strtok_r (NULL, split, &save);\
		}\
	}\
}


#define split_foreach_map_func1_param2(argv_x, func, param0, param_1)\
	const char *split = ",";/** split tokens */\
	char *token = NULL;\
	char *save = NULL;\
	char alias_list[128] = {0};\
	int each;\
	oryx_vector vec = vlib_map_main.entry_vec;\
	uint32_t elements_before = vec_count(vec);\
	elements_before = elements_before;\
	atomic_set(&n_map_elements, 0);\
	struct map_t *v = NULL;\
	if (!strcmp (alias_list, "*")) {\
		/** lookup alias with Post-Fuzzy match */\
		vec_foreach_element(vec, each, v){\
			if (v && (v->ul_flags & MAP_VALID)){\
				func (v, param0, param_1);\
				atomic_inc(&n_map_elements);\
			}\
		}\
	}  else {\
		memcpy (alias_list, (const void *)argv_x, strlen ((const char *)argv_x));\
		token = strtok_r (alias_list, split, &save);\
		while (token) {\
			if (token) {\
				v = NULL;\
				if (isalldigit (token)) {\
					/** Lookup by ID. */\
					uint32_t id = atoi(token);\
					struct prefix_t lp = {\
						.cmd = LOOKUP_ID,\
						.s = strlen (token),\
						.v = (void *)&id,\
					};\
					map_table_entry_lookup(&lp, &v);\
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
						token = token;\
						/** Lookup by ALIAS exactly. */\
						struct prefix_t lp = {\
							.cmd = LOOKUP_ALIAS,\
							.s = strlen (token),\
							.v = token,\
						};\
						map_table_entry_lookup (&lp, &v);\
						if (unlikely(!v)) {\
							goto lookup_next;\
						}\
					}\
				}\
				if (v && (v->ul_flags & MAP_VALID)) {\
					func (v, param0, param_1);\
					atomic_inc(&n_map_elements);\
					goto lookup_next;\
				}\
		lookup_by_alias_posted_fuzzy:\
				/** lookup alias with Post-Fuzzy match */\
				vec_foreach_element(vec, each, v){\
					if (v && (v->ul_flags & MAP_VALID)\
						&& !strncmp (v->sc_alias, token, (strlen(token) - 1/** ignore '*'  */))){\
						func (v, param0, param_1);\
						atomic_inc(&n_map_elements);\
					}\
				}\
			}\
		lookup_next:\
			token = strtok_r (NULL, split, &save);\
		}\
	}\
}

#define foreach_map_func1_param0(argv_x, func)\
	int each;\
	oryx_vector vec = vlib_map_main.entry_vec;\
	uint32_t elements_before = vec_count(vec);\
	elements_before = elements_before;\
	atomic_set(&n_map_elements, 0);\
	struct map_t *v;\
	vec_foreach_element(vec, each, v){\
		if (v && (v->ul_flags & MAP_VALID)) {\
			func (v);\
			atomic_inc(&n_map_elements);\
		}\
	}

#define foreach_map_func1_param1(argv_x, func, param0)\
	int each;\
	oryx_vector vec = vlib_map_main.entry_vec;\
	uint32_t elements_before = vec_count(vec);\
	elements_before = elements_before;\
	atomic_set(&n_map_elements, 0);\
	struct map_t *v;\
	vec_foreach_element(vec, each, v){\
		if (v && (v->ul_flags & MAP_VALID)) {\
			func (v, param0);\
			atomic_inc(&n_map_elements);\
		}\
	}
	
#endif


