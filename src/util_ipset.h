#ifndef UTIL_IPSET_H
#define UTIL_IPSET_H

#include "appl_private.h"


int appl_entry_format (struct appl_t *appl, 
	u32 __oryx_unused__*rule_id, const char *unused_var, 
	char __oryx_unused__*vlan, 
	char __oryx_unused__*sip, 
	char __oryx_unused__*dip, 
	char __oryx_unused__*sp, 
	char __oryx_unused__*dp, 
	char __oryx_unused__*proto);

void appl_entry_new (struct appl_t **appl, char *alias, u32 __oryx_unused__ type);
int appl_entry_del (vlib_appl_main_t *am, struct appl_t *appl);
int appl_entry_add (vlib_appl_main_t *am, struct appl_t *appl);
int appl_table_entry_deep_lookup(const char *argv, struct appl_t **appl);


#endif
