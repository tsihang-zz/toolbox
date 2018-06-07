#ifndef UTIL_MAP_H
#define UTIL_MAP_H

#include "map_private.h"
#include "udp_private.h"
#include "appl_private.h"

int map_entry_new (struct map_t **map, char *alias, char *from, char *to);

int map_table_entry_deep_lookup(const char *argv, struct map_t **map);

void map_entry_add_udp_to_drop_quat (struct udp_t *udp, struct map_t *map);
void map_entry_add_udp_to_pass_quat (struct udp_t *udp, struct map_t *map);

int map_entry_add_appl (struct appl_t __oryx_unused__*appl, struct map_t __oryx_unused__*map , 
	void (*download)(struct map_t *, struct appl_t *));
int map_entry_add_appl0 (struct appl_t __oryx_unused__*appl, struct map_t __oryx_unused__*map , 
	const char *hit_action, void (*dfn)(struct map_t *, struct appl_t *));

int map_entry_remove_appl (struct appl_t *appl, struct map_t *map);

void map_table_entry_lookup (struct prefix_t *lp, 
	struct map_t **m);

void em_download_appl(struct map_t *map, struct appl_t *appl);
void acl_download_appl(struct map_t *map, struct appl_t *appl);

	

#endif

