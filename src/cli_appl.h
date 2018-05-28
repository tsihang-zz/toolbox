#ifndef CLI_APPL_H
#define CLI_APPL_H

#include "appl_private.h"

extern atomic_t n_application_elements;
extern oryx_vector appl_vector_table;


#define split_foreach_application_func1_param0(argv_x, func)\
		const char *split = ",";/** split tokens */\
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
					appl_table_entry_lookup_i (atoi(token), &v);\
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
				vec_foreach_element(appl_vector_table, foreach_element, v){\
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
		const char *split = ",";/** split tokens */\
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
					appl_table_entry_lookup_i (atoi(token), &v);\
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
				vec_foreach_element(appl_vector_table, foreach_element, v){\
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
		const char *split = ",";/** split tokens */\
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
					appl_table_entry_lookup_i (atoi(token), &v);\
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
				vec_foreach_element(appl_vector_table, foreach_element, v){\
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
		vec_foreach_element(appl_vector_table, foreach_element, v){\
			if (v) {\
				func (v);\
				atomic_inc(&n_application_elements);\
			}\
		}
	
#define foreach_application_func1_param1(argv_x, func, param0)\
		int foreach_element;\
		atomic_set(&n_application_elements, 0);\
		struct appl_t *v;\
		vec_foreach_element(appl_vector_table, foreach_element, v){\
			if (v) {\
				func (v, param0);\
				atomic_inc(&n_application_elements);\
			}\
		}

#endif

