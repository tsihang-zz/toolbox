#ifndef CLI_APPL_H
#define CLI_APPL_H

#include "appl_private.h"

extern atomic_t n_application_elements;

#define split_foreach_application_func1_param0(argv_x, func)\
		const char *split = ",";/** split tokens */\
		char *token = NULL;\
		char *save = NULL;\
		char alias_list[128] = {0};\
		int each;\
		oryx_vector vec = vlib_appl_main.entry_vec;\
		struct appl_t *v = NULL;\
		atomic_set(&n_application_elements, 0);\
		memcpy (alias_list, (char *)argv_x, strlen ((char *)argv_x));\
		token = strtok_r (alias_list, split, &save);\
		while (token) {\
			if (token) {\
				struct appl_t *v = NULL;\
				if (isalldigit (token)) {\
					u32 id = atoi(token);\
					struct prefix_t lp = {\
						.cmd = LOOKUP_ID,\
						.v = (void *)&id,\
						.s = strlen (token),\
					};\
					appl_table_entry_lookup(&lp, &v);\
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
						appl_table_entry_lookup (&lp, &v);\
						if (unlikely(!v)) {\
							goto lookup_next;\
						}\
					}\
				}\
				if (v && (v->ul_flags & APPL_VALID)) {\
					func (v);\
					atomic_inc(&n_application_elements);\
					goto lookup_next;\
				}\
	lookup_by_alias_posted_fuzzy:\
				/** lookup alias with Post-Fuzzy match */\
				vec_foreach_element(vec, each, v){\
					if (v && (v->ul_flags & APPL_VALID) &&\
						!strncmp (v->sc_alias, token, (strlen(token) - 1/** ignore '*'  */))){\
						func (v);\
						atomic_inc(&n_application_elements);\
					}\
				}\
			}\
	lookup_next:\
			token = strtok_r (NULL, split, &save);\
		}
	
	
#define split_foreach_application_func1_param1(argv_x, func, param0)\
		const char *split = ",";/** split tokens */\
		char *token = NULL;\
		char *save = NULL;\
		char alias_list[128] = {0};\
		int each;\
		oryx_vector vec = vlib_appl_main.entry_vec;\
		struct appl_t *v = NULL;\
		atomic_set(&n_application_elements, 0);\
		memcpy (alias_list, (char *)argv_x, strlen ((char *)argv_x));\
		token = strtok_r (alias_list, split, &save);\
		while (token) {\
			if (token) {\
				struct appl_t *v = NULL;\
				if (isalldigit (token)) {\
					/** Lookup by ID. */\
					u32 id = atoi(token);\
					struct prefix_t lp = {\
						.cmd = LOOKUP_ID,\
						.v = (void *)&id,\
						.s = strlen (token),\
					};\
					appl_table_entry_lookup(&lp, &v);\
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
						appl_table_entry_lookup(&lp, &v);\
						if (unlikely(!v)) {\
							goto lookup_next;\
						}\
					}\
				}\
				if (v && (v->ul_flags & APPL_VALID)) {\
					func (v, param0);\
					atomic_inc(&n_application_elements);\
					goto lookup_next;\
				}\
	lookup_by_alias_posted_fuzzy:\
				/** lookup alias with Post-Fuzzy match */\
				vec_foreach_element(vec, each, v){\
					if (v && (v->ul_flags & APPL_VALID) &&\
						!strncmp (v->sc_alias, token, (strlen(token) - 1/** ignore '*'  */))){\
						func (v, param0);\
						atomic_inc(&n_application_elements);\
					}\
				}\
			}\
	lookup_next:\
			token = strtok_r (NULL, split, &save);\
		}
	
	
#define split_foreach_application_func1_param2(argv_x, func, param0, param_1)\
		const char *split = ",";/** split tokens */\
		char *token = NULL;\
		char *save = NULL;\
		char alias_list[128] = {0};\
		int each;\
		oryx_vector vec = vlib_appl_main.entry_vec;\
		struct appl_t *v = NULL;\
		atomic_set(&n_application_elements, 0);\
		memcpy (alias_list, (char *)argv_x, strlen ((char *)argv_x));\
		token = strtok_r (alias_list, split, &save);\
		while (token) {\
			if (token) {\
				struct appl_t *v = NULL;\
				if (isalldigit (token)) {\
					/** Lookup by ID. */\
					u32 id = atoi(token);\
					struct prefix_t lp = {\
						.cmd = LOOKUP_ID,\
						.v = (void *)&id,\
						.s = strlen (token),\
					};\
					appl_table_entry_lookup(&lp, &v);\
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
						appl_table_entry_lookup(&lp, &v);\
						if (unlikely(!v)) {\
							goto lookup_next;\
						}\
					}\
				}\
				if (v && (v->ul_flags & APPL_VALID)) {\
						func (v, param0, param_1);\
						atomic_inc(&n_application_elements);\
					goto lookup_next;\
				}\
	lookup_by_alias_posted_fuzzy:\
				/** lookup alias with Post-Fuzzy match */\
				vec_foreach_element(vec, each, v){\
					if (v && (v->ul_flags & APPL_VALID) &&\
						!strncmp (v->sc_alias, token, (strlen(token) - 1/** ignore '*'  */))){\
						func (v, param0, param_1);\
						atomic_inc(&n_application_elements);\
					}\
				}\
			}\
	lookup_next:\
			token = strtok_r (NULL, split, &save);\
		}
	
			
			
#define split_foreach_application_func1_param3(argv_x, func, param0, param_1, param_2)\
				const char *split = ",";/** split tokens */\
				char *token = NULL;\
				char *save = NULL;\
				char alias_list[128] = {0};\
				int each;\
				oryx_vector vec = vlib_appl_main.entry_vec;\
				struct appl_t *v = NULL;\
				atomic_set(&n_application_elements, 0);\
				memcpy (alias_list, (char *)argv_x, strlen ((char *)argv_x));\
				token = strtok_r (alias_list, split, &save);\
				while (token) {\
					if (token) {\
						struct appl_t *v = NULL;\
						if (isalldigit (token)) {\
							/** Lookup by ID. */\
							u32 id = atoi(token);\
							struct prefix_t lp = {\
								.cmd = LOOKUP_ID,\
								.v = (void *)&id,\
								.s = strlen (token),\
							};\
							appl_table_entry_lookup(&lp, &v);\
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
								appl_table_entry_lookup(&lp, &v);\
								if (unlikely(!v)) {\
									goto lookup_next;\
								}\
							}\
						}\
						if (v && (v->ul_flags & APPL_VALID)) {\
							func (v, param0, param_1, param_2);\
							atomic_inc(&n_application_elements);\
							goto lookup_next;\
						}\
			lookup_by_alias_posted_fuzzy:\
						/** lookup alias with Post-Fuzzy match */\
						vec_foreach_element(vec, each, v){\
							if (v && (v->ul_flags & APPL_VALID) &&\
								!strncmp (v->sc_alias, token, (strlen(token) - 1/** ignore '*'  */))){\
								func (v, param0, param_1, param_2);\
								atomic_inc(&n_application_elements);\
							}\
						}\
					}\
			lookup_next:\
					token = strtok_r (NULL, split, &save);\
				}


#define foreach_application_func1_param0(unused_argv, func)\
		int each;\
		oryx_vector vec = vlib_appl_main.entry_vec;\
		atomic_set(&n_application_elements, 0);\
		struct appl_t *v;\
		vec_foreach_element(am->entry_vec, each, v){\
			if (v && (v->ul_flags & APPL_VALID)) {\
				func (v);\
				atomic_inc(&n_application_elements);\
			}\
		}
	
#define foreach_application_func1_param1(unused_argv, func, param0)\
		int each;\
		oryx_vector vec = vlib_appl_main.entry_vec;\
		atomic_set(&n_application_elements, 0);\
		struct appl_t *v;\
		vec_foreach_element(am->entry_vec, each, v){\
			if (v && (v->ul_flags & APPL_VALID)) {\
				func (v, param0);\
				atomic_inc(&n_application_elements);\
			}\
		}

#endif

