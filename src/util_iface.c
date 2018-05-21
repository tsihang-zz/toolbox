#include "oryx.c"
#include "iface_private.h"

void iface_alloc (struct iface_t **this)
{
	int id;

	(*this) = NULL;
	
	/** create an port */
	(*this) = kmalloc (sizeof (struct iface_t), MPF_CLR, __oryx_unused_val__);

	ASSERT ((*this));

	(*this)->us_mtu = 1500;
	(*this)->belong_maps = vec_init(1024);
	(*this)->if_poll_up = netdev_up;
	memcpy(&(*this)->sc_alias[0], "--", strlen("--"));

	/** port counters */
	id = QUA_COUNTER_RX;
	(*this)->counter_bytes[id] = oryx_register_counter("port.rx.bytes", 
			"bytes Rx for this port", &(*this)->perf_private_ctx);
	(*this)->counter_pkts[id] = oryx_register_counter("port.rx.pkts",
			"pkts Rx for this port", &(*this)->perf_private_ctx);

	id = QUA_COUNTER_TX;
	(*this)->counter_bytes[id] = oryx_register_counter("port.tx.bytes", 
			"bytes Tx for this port", &(*this)->perf_private_ctx);
	(*this)->counter_pkts[id] = oryx_register_counter("port.tx.pkts",
			"pkts Tx for this port", &(*this)->perf_private_ctx);

	/**
	 * More Counters here.
	 * TODO ...
	 */
	
	/** last step */
	oryx_counter_get_array_range(COUNTER_RANGE_START(&(*this)->perf_private_ctx), 
			COUNTER_RANGE_END(&(*this)->perf_private_ctx), 
			&(*this)->perf_private_ctx);
}

int iface_rename(vlib_port_main_t *vp, 
				struct iface_t *this, const char *new_name)
{
	/** Delete old alias from hash table. */
	oryx_htable_del (vp->htable, port_alias(this), strlen (port_alias(this)));
	memset (port_alias(this), 0, strlen (port_alias(this)));
	memcpy (port_alias(this), (char *)new_name, 
		strlen ((char *)new_name));
	/** New alias should be rewrite to hash table. */
	oryx_htable_add (vp->htable, port_alias(this), strlen (port_alias(this)));	
	return 0;
}

int iface_lookup_id(vlib_port_main_t *vp,
				u32 id, struct iface_t **this)
{
	(*this) = NULL;
	do_lock (&vp->lock);
	(*this) = (struct iface_t *) vec_lookup_ensure (vp->entry_vec, id);
	do_unlock (&vp->lock);
	return 0;
}

int iface_lookup_alias(vlib_port_main_t *vp,
				const char *alias, struct iface_t **this)
{
	(*this) = NULL;
	void *s = oryx_htable_lookup(vp->htable, alias, strlen(alias));			
	if (likely (s)) {
		/** THIS IS A VERY CRITICAL POINT. */
		do_lock (&vp->lock);
		(*this) = (struct iface_t *) container_of (s, struct iface_t, sc_alias);
		do_unlock (&vp->lock);
		return 0;
	}
	return -1;
}

int iface_add(vlib_port_main_t *vp, struct iface_t *this)
{
	do_lock (&vp->lock);
	int r = oryx_htable_add(vp->htable, this->sc_alias, strlen((const char *)this->sc_alias));
	if (r == 0) {
		vec_set_index (vp->entry_vec, this->ul_id, this);
	}
	do_unlock (&vp->lock);
	return r;
}

int iface_del(vlib_port_main_t *vp, struct iface_t *this)
{
	do_lock (&vp->lock);
	int r = oryx_htable_del(vp->htable, this->sc_alias, strlen((const char *)this->sc_alias));
	if (r == 0) {
		vec_unset (vp->entry_vec, this->ul_id);
	}
	do_unlock (&vp->lock);
	return r;
}

