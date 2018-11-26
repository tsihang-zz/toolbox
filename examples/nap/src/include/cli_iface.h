#ifndef CLI_IFACE_H
#define CLI_IFACE_H

#include "iface_private.h"

atomic_extern(uint32_t, nb_ifaces);
extern void vlib_iface_init(vlib_main_t *vm);

#define split_foreach_iface_func1(argv_x, func){\
	const char *split = ",";/** split tokens */\
	char *token = NULL;\
	char *save = NULL;\
	char alias_list[128] = {0};\
	int each;\
	oryx_vector vec = vlib_iface_main.entry_vec;\
	struct iface_t *v = NULL;\
	atomic_set(nb_ifaces, 0);\
	if (!strcmp (alias_list, "*")) {\
		/** lookup alias with Post-Fuzzy match */\
		vec_foreach_element(vec, each, v){\
			if (v){\
				func (v);\
				atomic_inc(nb_ifaces);\
			}\
		}\
	} else {\
		memcpy (alias_list, (const char *)argv_x, strlen ((const char *)argv_x));\
		token = strtok_r (alias_list, split, &save);\
		while (token) {\
			if (token) {\
				if (isalldigit (token)) {\
					/** Lookup by ID. */\
					uint32_t id = atoi(token);\
					struct prefix_t lp = {\
						.cmd = LOOKUP_ID,\
						.v = (void *)&id,\
						.s = strlen (token),\
					};\
					iface_table_entry_lookup (&lp, &v);\
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
						iface_table_entry_lookup (&lp, &v);\
						if (!v) {\
							goto lookup_next;\
						}\
					}\
				}\
				if (v) {\
					func (v);\
					atomic_inc(nb_ifaces);\
					goto lookup_next;\
				}\
	lookup_by_alias_posted_fuzzy:\
				/** lookup alias with Post-Fuzzy match */\
				vec_foreach_element(vec, each, v){\
					if (v && !strncmp (v->sc_alias, token, (strlen(token) - 1/** ignore '*'  */))){\
						func (v);\
						atomic_inc(nb_ifaces);\
					}\
				}\
				goto lookup_next;\
			}\
	lookup_next:\
			token = strtok_r (NULL, split, &save);\
		}\
	}\
}

#define split_foreach_iface_func1_param1(argv_x, func, param0){\
	const char *split = ",";/** split tokens */\
	char *token = NULL;\
	char *save = NULL;\
	char alias_list[128] = {0};\
	int each;\
	oryx_vector vec = vlib_iface_main.entry_vec;\
	struct iface_t *v = NULL;\
	atomic_set(nb_ifaces, 0);\
	if (!strcmp (alias_list, "*")) {\
		/** lookup alias with Post-Fuzzy match */\
		vec_foreach_element(vec, each, v){\
			if (v){\
				func (v, param0);\
				atomic_inc(nb_ifaces);\
			}\
		}\
	} else {\
		memcpy (alias_list, (const char *)argv_x, strlen ((const char *)argv_x));\
		token = strtok_r (alias_list, split, &save);\
		while (token) {\
			if (token) {\
				if (isalldigit (token)) {\
					/** Lookup by ID. */\
					uint32_t id = atoi(token);\
					struct prefix_t lp = {\
						.cmd = LOOKUP_ID,\
						.s = strlen (token),\
						.v = (void *)&id,\
					};\
					iface_table_entry_lookup (&lp, &v);\
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
						iface_table_entry_lookup (&lp, &v);\
						if (!v) {\
							goto lookup_next;\
						}\
					}\
				}\
				if (v) {\
					func (v, param0);\
					atomic_inc(nb_ifaces);\
					goto lookup_next;\
				}\
	lookup_by_alias_posted_fuzzy:\
				/** lookup alias with Post-Fuzzy match */\
				vec_foreach_element(vec, each, v){\
					if (v && !strncmp (v->sc_alias, token, (strlen(token) - 1/** ignore '*'  */))){\
						func (v, param0);\
						atomic_inc(nb_ifaces);\
					}\
				}\
				goto lookup_next;\
			}\
	lookup_next:\
			token = strtok_r (NULL, split, &save);\
		}\
	}\
}


#define split_foreach_iface_func1_param2(argv_x, func, param0, param_1) {\
	const char *split = ",";/** split tokens */\
	char *token = NULL;\
	char *save = NULL;\
	char alias_list[128] = {0};\
	int each;\
	oryx_vector vec = vlib_iface_main.entry_vec;\
	struct iface_t *v = NULL;\
	atomic_set(nb_ifaces, 0);\
	if (!strcmp (alias_list, "*")) {\
		/** lookup alias with Post-Fuzzy match */\
		vec_foreach_element(vec, each, v){\
			if (v){\
				func (v, param0, param_1);\
				atomic_inc(nb_ifaces);\
			}\
		}\
	} else {\
		memcpy (alias_list, (const char *)argv_x, strlen ((const char *)argv_x));\
		token = strtok_r (alias_list, split, &save);\
		while (token) {\
			if (token) {\
				if (isalldigit (token)) {\
					/** Lookup by ID. */\
					uint32_t id = atoi(token);\
					struct prefix_t lp = {\
						.cmd = LOOKUP_ID,\
						.s = strlen (token),\
						.v = (void *)&id,\
					};\
					iface_table_entry_lookup (&lp, &v);\
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
						iface_table_entry_lookup (&lp, &v);\
						if (!v) {\
							goto lookup_next;\
						}\
					}\
				}\
				if (v) {\
					func (v, param0, param_1);\
					atomic_inc(nb_ifaces);\
					goto lookup_next;\
				}\
	lookup_by_alias_posted_fuzzy:\
				/** lookup alias with Post-Fuzzy match */\
				vec_foreach_element(vec, each, v){\
					if (v && !strncmp (v->sc_alias, token, (strlen(token) - 1/** ignore '*'  */))){\
						func (v, param0, param_1);\
						atomic_inc(nb_ifaces);\
					}\
				}\
				goto lookup_next;\
			}\
	lookup_next:\
			token = strtok_r (NULL, split, &save);\
		}\
	}\
}

#define foreach_iface_func1_param0(argv_x, func)\
	int each;\
	oryx_vector vec = vlib_iface_main.entry_vec;\
	atomic_set(nb_ifaces, 0);\
	struct iface_t *v;\
	vec_foreach_element(vec, each, v){\
		if (v) {\
			func (v);\
			atomic_inc(nb_ifaces);\
		}\
	}

#define foreach_iface_func1_param1(argv_x, func, param0)\
	int each;\
	oryx_vector vec = vlib_iface_main.entry_vec;\
	atomic_set(nb_ifaces, 0);\
	struct iface_t *v;\
	vec_foreach_element(vec, each, v){\
		if (v) {\
			func (v, param0);\
			atomic_inc(nb_ifaces);\
		}\
	}

#define foreach_iface_func1_param2(argv_x, func, param0, param1)\
		int each;\
		oryx_vector vec = vlib_iface_main.entry_vec;\
		atomic_set(nb_ifaces, 0);\
		struct iface_t *v;\
		vec_foreach_element(vec, each, v){\
			if (v) {\
				func (v, param0, param1);\
				atomic_inc(nb_ifaces);\
			}\
		}
		

#define foreach_iface_func0_param0_func1_param1(argv_x, func0, param0, func1, param1)\
	int each;\
	oryx_vector vec = vlib_iface_main.entry_vec;\
	atomic_set(nb_ifaces, 0);\
	struct iface_t *v;\
	vec_foreach_element(vec, each, v){\
		if (v) {\
			func (v, param0);\
			atomic_inc(nb_ifaces);\
		}\
	}

#endif

