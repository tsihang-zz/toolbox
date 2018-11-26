#ifndef CLI_UDP_H
#define CLI_UDP_H

atomic_extern(uint32_t, n_udp_elements);
extern oryx_vector udp_vector_table;

#define split_foreach_udp_func1_param0(argv_x, func)\
	const char *split = ",";/** split tokens */\
	char *token = NULL;\
	char *save = NULL;\
	char alias_list[128] = {0};\
	int each;\
	atomic_set(n_udp_elements, 0);\
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
				atomic_inc(n_udp_elements);\
				goto lookup_next;\
			}\
lookup_by_alias_posted_fuzzy:\
			/** lookup alias with Post-Fuzzy match */\
			vec_foreach_element(udp_vector_table, each, v){\
				if (v && !strncmp (v->sc_alias, token, (strlen(token) - 1/** ignore '*'  */))){\
					func (v);\
					atomic_inc(n_udp_elements);\
				}\
			}\
		}\
lookup_next:\
		token = strtok_r (NULL, split, &save);\
	}

#define split_foreach_udp_func1_param1(argv_x, func, param0)\
	const char *split = ",";/** split tokens */\
	char *token = NULL;\
	char *save = NULL;\
	char alias_list[128] = {0};\
	int each;\
	atomic_set(n_udp_elements, 0);\
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
				atomic_inc(n_udp_elements);\
				goto lookup_next;\
			}\
lookup_by_alias_posted_fuzzy:\
			/** lookup alias with Post-Fuzzy match */\
			vec_foreach_element(udp_vector_table, each, v){\
				if (v && !strncmp (v->sc_alias, token, (strlen(token) - 1/** ignore '*'  */))){\
					func (v, param0);\
					atomic_inc(n_udp_elements);\
				}\
			}\
		}\
lookup_next:\
		token = strtok_r (NULL, split, &save);\
	}


#define split_foreach_udp_func1_param2(argv_x, func, param0, param_1)\
	const char *split = ",";/** split tokens */\
	char *token = NULL;\
	char *save = NULL;\
	char alias_list[128] = {0};\
	int each;\
	atomic_set(n_udp_elements, 0);\
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
				atomic_inc(n_udp_elements);\
				goto lookup_next;\
			}\
lookup_by_alias_posted_fuzzy:\
			/** lookup alias with Post-Fuzzy match */\
			vec_foreach_element(udp_vector_table, each, v){\
				if (v && !strncmp (v->sc_alias, token, (strlen(token) - 1/** ignore '*'  */))){\
					func (v, param0, param_1);\
					atomic_inc(n_udp_elements);\
				}\
			}\
		}\
lookup_next:\
		token = strtok_r (NULL, split, &save);\
	}


#define split_foreach_udp_func2_param1(argv_x, func1, func1_param0, func2, func2_param0)\
	const char *split = ",";/** split tokens */\
	char *token = NULL;\
	char *save = NULL;\
	char alias_list[128] = {0};\
	int each;\
	atomic_set(n_udp_elements, 0);\
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
				atomic_inc(n_udp_elements);\
				goto lookup_next;\
			}\
lookup_by_alias_posted_fuzzy:\
			/** lookup alias with Post-Fuzzy match */\
			vec_foreach_element(udp_vector_table, each, v){\
				if (v && !strncmp (v->sc_alias, token, (strlen(token) - 1/** ignore '*'  */))){\
					func1(v, func1_param0);\
					func2 (v, func2_param0);\
					atomic_inc(n_udp_elements);\
				}\
			}\
		}\
lookup_next:\
		token = strtok_r (NULL, split, &save);\
	}


#define split_foreach_udp_pattern_func1_param1(argv_x, func, param0)\
	const char *split = ",";/** split tokens */\
	char *token = NULL;\
	char *save = NULL;\
	char alias_list[128] = {0};\
	atomic_set(n_udp_elements, 0);\
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
				atomic_inc(n_udp_elements);\
				goto lookup_next;\
			}\
lookup_by_alias_posted_fuzzy:\
			goto lookup_next;\
		}\
lookup_next:\
		token = strtok_r (NULL, split, &save);\
	}
				

#define foreach_udp_func1_param0(argv_x, func)\
	int each;\
	atomic_set(n_udp_elements, 0);\
	struct udp_t *v;\
	vec_foreach_element(udp_vector_table, each, v){\
		if (v) {\
			func (v);\
			atomic_inc(n_udp_elements);\
		}\
	}

#define foreach_udp_func1_param1(argv_x, func, param0)\
	int each;\
	atomic_set(n_udp_elements, 0);\
	struct udp_t *v;\
	vec_foreach_element(udp_vector_table, each, v){\
		if (v) {\
			func (v, param0);\
			atomic_inc(n_udp_elements);\
			v = NULL;\
		}\
	}

#endif
