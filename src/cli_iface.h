#ifndef CLI_IFACE_H
#define CLI_IFACE_H

#include "iface_private.h"

extern atomic_t n_intf_elements;

#define split_foreach_port_func1(argv_x, func){\
	const char *split = ",";/** split tokens */\
	char *token = NULL;\
	char *save = NULL;\
	char alias_list[128] = {0};\
	int foreach_element;\
	oryx_vector vec = vlib_port_main.entry_vec;\
	struct iface_t *v = NULL;\
	atomic_set(&n_intf_elements, 0);\
	if (!strcmp (alias_list, "*")) {\
		/** lookup alias with Post-Fuzzy match */\
		vec_foreach_element(vec, foreach_element, v){\
			if (v){\
				func (v);\
				atomic_inc(&n_intf_elements);\
			}\
		}\
	} else {\
		memcpy (alias_list, (char *)argv_x, strlen ((char *)argv_x));\
		token = strtok_r (alias_list, split, &save);\
		while (token) {\
			if (token) {\
				if (isalldigit (token)) {\
					/** Lookup by ID. */\
					u32 id = atoi(token);\
					struct prefix_t lp = {\
						.cmd = LOOKUP_ID,\
						.v = (void *)&id,\
						.s = strlen (token),\
					};\
					port_table_entry_lookup (&lp, &v);\
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
						port_table_entry_lookup (&lp, &v);\
						if (!v) {\
							goto lookup_next;\
						}\
					}\
				}\
				if (v) {\
					func (v);\
					atomic_inc(&n_intf_elements);\
					goto lookup_next;\
				}\
	lookup_by_alias_posted_fuzzy:\
				/** lookup alias with Post-Fuzzy match */\
				vec_foreach_element(vec, foreach_element, v){\
					if (v && !strncmp (v->sc_alias, token, (strlen(token) - 1/** ignore '*'  */))){\
						func (v);\
						atomic_inc(&n_intf_elements);\
					}\
				}\
				goto lookup_next;\
			}\
	lookup_next:\
			token = strtok_r (NULL, split, &save);\
		}\
	}\
}

#define split_foreach_port_func1_param1(argv_x, func, param0){\
	const char *split = ",";/** split tokens */\
	char *token = NULL;\
	char *save = NULL;\
	char alias_list[128] = {0};\
	int foreach_element;\
	oryx_vector vec = vlib_port_main.entry_vec;\
	struct iface_t *v = NULL;\
	atomic_set(&n_intf_elements, 0);\
	if (!strcmp (alias_list, "*")) {\
		/** lookup alias with Post-Fuzzy match */\
		vec_foreach_element(vec, foreach_element, v){\
			if (v){\
				func (v, param0);\
				atomic_inc(&n_intf_elements);\
			}\
		}\
	} else {\
		memcpy (alias_list, (char *)argv_x, strlen ((char *)argv_x));\
		token = strtok_r (alias_list, split, &save);\
		while (token) {\
			if (token) {\
				if (isalldigit (token)) {\
					/** Lookup by ID. */\
					u32 id = atoi(token);\
					struct prefix_t lp = {\
						.cmd = LOOKUP_ID,\
						.s = strlen (token),\
						.v = (void *)&id,\
					};\
					port_table_entry_lookup (&lp, &v);\
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
						port_table_entry_lookup (&lp, &v);\
						if (!v) {\
							goto lookup_next;\
						}\
					}\
				}\
				if (v) {\
					func (v, param0);\
					atomic_inc(&n_intf_elements);\
					goto lookup_next;\
				}\
	lookup_by_alias_posted_fuzzy:\
				/** lookup alias with Post-Fuzzy match */\
				vec_foreach_element(vec, foreach_element, v){\
					if (v && !strncmp (v->sc_alias, token, (strlen(token) - 1/** ignore '*'  */))){\
						func (v, param0);\
						atomic_inc(&n_intf_elements);\
					}\
				}\
				goto lookup_next;\
			}\
	lookup_next:\
			token = strtok_r (NULL, split, &save);\
		}\
	}\
}


#define split_foreach_port_func1_param2(argv_x, func, param0, param_1) {\
	const char *split = ",";/** split tokens */\
	char *token = NULL;\
	char *save = NULL;\
	char alias_list[128] = {0};\
	int foreach_element;\
	oryx_vector vec = vlib_port_main.entry_vec;\
	struct iface_t *v = NULL;\
	atomic_set(&n_intf_elements, 0);\
	if (!strcmp (alias_list, "*")) {\
		/** lookup alias with Post-Fuzzy match */\
		vec_foreach_element(vec, foreach_element, v){\
			if (v){\
				func (v, param0, param_1);\
				atomic_inc(&n_intf_elements);\
			}\
		}\
	} else {\
		memcpy (alias_list, (char *)argv_x, strlen ((char *)argv_x));\
		token = strtok_r (alias_list, split, &save);\
		while (token) {\
			if (token) {\
				if (isalldigit (token)) {\
					/** Lookup by ID. */\
					u32 id = atoi(token);\
					struct prefix_t lp = {\
						.cmd = LOOKUP_ID,\
						.s = strlen (token),\
						.v = (void *)&id,\
					};\
					port_table_entry_lookup (&lp, &v);\
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
						port_table_entry_lookup (&lp, &v);\
						if (!v) {\
							goto lookup_next;\
						}\
					}\
				}\
				if (v) {\
					func (v, param0, param_1);\
					atomic_inc(&n_intf_elements);\
					goto lookup_next;\
				}\
	lookup_by_alias_posted_fuzzy:\
				/** lookup alias with Post-Fuzzy match */\
				vec_foreach_element(vec, foreach_element, v){\
					if (v && !strncmp (v->sc_alias, token, (strlen(token) - 1/** ignore '*'  */))){\
						func (v, param0, param_1);\
						atomic_inc(&n_intf_elements);\
					}\
				}\
				goto lookup_next;\
			}\
	lookup_next:\
			token = strtok_r (NULL, split, &save);\
		}\
	}\
}

#define foreach_port_func1_param0(argv_x, func)\
	int foreach_element;\
	oryx_vector vec = vlib_port_main.entry_vec;\
	atomic_set(&n_intf_elements, 0);\
	struct iface_t *v;\
	vec_foreach_element(vec, foreach_element, v){\
		if (v) {\
			func (v);\
			atomic_inc(&n_intf_elements);\
		}\
	}

#define foreach_port_func1_param1(argv_x, func, param0)\
	int foreach_element;\
	oryx_vector vec = vlib_port_main.entry_vec;\
	atomic_set(&n_intf_elements, 0);\
	struct iface_t *v;\
	vec_foreach_element(vec, foreach_element, v){\
		if (v) {\
			func (v, param0);\
			atomic_inc(&n_intf_elements);\
		}\
	}

#define foreach_port_func0_param0_func1_param1(argv_x, func0, param0, func1, param1)\
	int foreach_element;\
	oryx_vector vec = vlib_port_main.entry_vec;\
	atomic_set(&n_intf_elements, 0);\
	struct iface_t *v;\
	vec_foreach_element(vec, foreach_element, v){\
		if (v) {\
			func (v, param0);\
			atomic_inc(&n_intf_elements);\
		}\
	}

#endif

