#ifndef UTIL_MAP_H
#define UTIL_MAP_H

#include "map_private.h"
#include "appl_private.h"

int map_entry_new (struct map_t **map, char *alias, char *from, char *to);
void map_entry_destroy (struct map_t *map);

int map_table_entry_deep_lookup(const char *argv, struct map_t **map);

int map_entry_add_appl (struct appl_t __oryx_unused__*appl, struct map_t __oryx_unused__*map , 
	const char *hit_action, int (*rule_download_engine)(struct map_t *, struct appl_t *));
int acl_remove_appl(struct map_t *map, struct appl_t *appl);

int map_entry_remove_appl (struct appl_t *appl, struct map_t *map,
	int (*rule_remove_engine)(struct map_t *, struct appl_t *));

void map_table_entry_lookup (struct prefix_t *lp, 
	struct map_t **m);

int em_download_appl(struct map_t *map, struct appl_t *appl);
int acl_download_appl(struct map_t *map, struct appl_t *appl);

void map_entry_add_port (struct iface_t *port, struct map_t *map, u8 from_to);
void map_entry_remove_port (struct iface_t *port, struct map_t *map, u8 from_to);

#endif

