#ifndef UTIL_IFACE_H
#define UTIL_IFACE_H

#include "iface_private.h"

void iface_alloc (struct iface_t **this);
int iface_rename(vlib_port_main_t *vp, 
				struct iface_t *this, const char *new_name);
int iface_lookup_alias(vlib_port_main_t *vp,
				const char *alias, struct iface_t **this);
int iface_add(vlib_port_main_t *vp, struct iface_t *this);
int iface_del(vlib_port_main_t *vp, struct iface_t *this);

#endif
