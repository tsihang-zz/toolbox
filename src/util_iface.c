#include "oryx.c"
#include "iface_private.h"

void iface_alloc (struct iface_t **this)
{
	int							id;
	int							lcore;
	char						lcore_stats_name[1024];
	struct CounterCtx			*per_private_ctx0;
	struct iface_counter_ctx	*if_counter_ctx0;

	(*this) = NULL;
	
	/** create an port */
	(*this) = kmalloc (sizeof (struct iface_t), MPF_CLR, __oryx_unused_val__);
	BUG_ON ((*this) == NULL);

	memcpy(&(*this)->sc_alias[0], "--", strlen("--"));
	(*this)->mtu = 1500;
	(*this)->perf_private_ctx = kmalloc(sizeof(struct CounterCtx), MPF_CLR, __oryx_unused_val__);
	(*this)->if_counter_ctx = kmalloc(sizeof(struct iface_counter_ctx), MPF_CLR, __oryx_unused_val__);

	per_private_ctx0 = (*this)->perf_private_ctx;
	if_counter_ctx0 = (*this)->if_counter_ctx;
	
	for (id = QUA_RX; id < QUA_RXTX; id ++) {
		for (lcore = 0; lcore < MAX_LCORES; lcore ++) {		
			sprintf(lcore_stats_name, "port.%s.bytes.lcore%d", id == QUA_RX ? "rx" : "tx", lcore);
			if_counter_ctx0->lcore_counter_bytes[id][lcore] = oryx_register_counter(strdup(lcore_stats_name), 
					"NULL", per_private_ctx0);
		
			sprintf(lcore_stats_name, "port.%s.pkts.lcore%d", id == QUA_RX ? "rx" : "tx", lcore);
			if_counter_ctx0->lcore_counter_pkts[id][lcore] = oryx_register_counter(strdup(lcore_stats_name), 
					"NULL", per_private_ctx0);
		}
	}


	/**
	 * More Counters here.
	 * TODO ...
	 */
	
	/** last step */
	oryx_counter_get_array_range(COUNTER_RANGE_START(per_private_ctx0), 
			COUNTER_RANGE_END(per_private_ctx0), 
			per_private_ctx0);
}

int iface_rename(vlib_port_main_t *pm, 
				struct iface_t *this, const char *new_name)
{
	/** Delete old alias from hash table. */
	oryx_htable_del (pm->htable, iface_alias(this), strlen (iface_alias(this)));
	memset (iface_alias(this), 0, strlen (iface_alias(this)));
	memcpy (iface_alias(this), (char *)new_name, 
		strlen ((char *)new_name));
	/** New alias should be rewrite to hash table. */
	oryx_htable_add (pm->htable, iface_alias(this), strlen (iface_alias(this)));	
	return 0;
}



int iface_add(vlib_port_main_t *pm, struct iface_t *this)
{
	do_lock (&pm->lock);
	int r = oryx_htable_add(pm->htable, iface_alias(this),
						strlen((const char *)iface_alias(this)));
	if (r == 0) {
		vec_set_index (pm->entry_vec, this->ul_id, this);
		pm->ul_n_ports ++;
	}
	do_unlock (&pm->lock);
	return r;
}

int iface_del(vlib_port_main_t *pm, struct iface_t *this)
{
	do_lock (&pm->lock);
	int r = oryx_htable_del(pm->htable, iface_alias(this),
						strlen((const char *)iface_alias(this)));
	if (r == 0) {
		vec_unset (pm->entry_vec, this->ul_id);
		pm->ul_n_ports --;
	}
	do_unlock (&pm->lock);
	return r;
}

