#include "oryx.h"
#include "dpdk.h"
#include "config.h"
#include "iface.h"
#include "iface_enum.h"

#ifndef DIM
/** Number of elements in the array. */
#define	DIM(a)	(sizeof (a) / sizeof ((a)[0]))
#endif

vlib_iface_main_t vlib_iface_main;
int link_trasition_detected = 0;

static
void ht_iface_free
(
	const ht_value_t __oryx_unused__ v
)
{
	/** Never free here! */
}

static
ht_key_t ht_iface_hval 
(
	IN struct oryx_htable_t *ht,
	IN const ht_value_t v,
	IN uint32_t s
) 
{
     uint8_t *d = (uint8_t *)v;
     uint32_t i;
     uint32_t hv = 0;

     for (i = 0; i < s; i++) {
         if (i == 0)      hv += (((uint32_t)*d++));
         else if (i == 1) hv += (((uint32_t)*d++) * s);
         else             hv *= (((uint32_t)*d++) * i) + s + i;
     }

     hv *= s;
     hv %= ht->array_size;
     
     return hv;
}

static
int ht_iface_cmp
(
	IN const ht_value_t v1,
	IN uint32_t s1,
	IN const ht_value_t v2,
	IN uint32_t s2
)
{
	int xret = 0;

	if (!v1 || !v2 ||s1 != s2 ||
		memcmp(v1, v2, s2))
		xret = 1;	/** Compare failed. */

	return xret;
}

static void
iface_alloc
(
	OUT struct iface_t **this
)
{
	int 						id;
	int 						lcore;
	char						lcore_stats_name[1024];
	struct oryx_counter_ctx_t			*per_private_ctx0;
	struct iface_counter_ctx	*if_counter_ctx0;

	(*this) = NULL;
	
	/** create an port */
	(*this) = kmalloc (sizeof (struct iface_t), MPF_CLR, __oryx_unused_val__);
	BUG_ON ((*this) == NULL);

	memcpy(&(*this)->sc_alias[0], "--", strlen("--"));
	(*this)->mtu = 1500;
	(*this)->perf_private_ctx = kmalloc(sizeof(struct oryx_counter_ctx_t), MPF_CLR, __oryx_unused_val__);
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

static __oryx_unused__
int iface_rename
(
	IN vlib_iface_main_t *pm,
	IN struct iface_t *this,
	IN const char *new_name
)
{
	/** Delete old alias from hash table. */
	oryx_htable_del (pm->htable, iface_alias(this), strlen (iface_alias(this)));
	memset (iface_alias(this), 0, strlen (iface_alias(this)));
	memcpy (iface_alias(this), new_name, strlen (new_name));
	/** New alias should be rewrite to hash table. */
	oryx_htable_add (pm->htable, iface_alias(this), strlen (iface_alias(this)));	
	return 0;
}

static __oryx_unused__
int iface_add
(
	IN vlib_iface_main_t *pm,
	IN struct iface_t *this
)
{
	do_mutex_lock (&pm->lock);
	int r = oryx_htable_add(pm->htable, iface_alias(this),
						strlen((const char *)iface_alias(this)));
	if (r == 0) {
		vec_set_index (pm->entry_vec, this->ul_id, this);
		pm->ul_n_ports ++;
	}
	do_mutex_unlock (&pm->lock);
	return r;
}

static __oryx_unused__
int iface_del
(
	IN vlib_iface_main_t *pm,
	IN struct iface_t *this
)
{
	do_mutex_lock (&pm->lock);
	int r = oryx_htable_del(pm->htable, iface_alias(this),
						strlen((const char *)iface_alias(this)));
	if (r == 0) {
		vec_unset (pm->entry_vec, this->ul_id);
		pm->ul_n_ports --;
	}
	do_mutex_unlock (&pm->lock);
	return r;
}

static __oryx_always_inline__
void iface_healthy_tmr_handler
(
	IN struct oryx_timer_t __oryx_unused__*tmr,
	IN int __oryx_unused__ argc,
	IN char __oryx_unused__**argv
)
{
	uint64_t nr_rx_pkts;
	uint64_t nr_rx_bytes;
	uint64_t nr_tx_pkts;
	uint64_t nr_tx_bytes;

	int lcore;
	uint64_t lcore_nr_rx_pkts[MAX_LCORES];
	uint64_t lcore_nr_rx_bytes[MAX_LCORES];
	uint64_t lcore_nr_tx_pkts[MAX_LCORES];
	uint64_t lcore_nr_tx_bytes[MAX_LCORES];

	static FILE *fp;
	const char *healthy_file = "/data/iface_healthy.txt";
	int each;
	oryx_vector vec = vlib_iface_main.entry_vec;
	struct iface_t *iface;	
	char cat_null[128] = "cat /dev/null > ";

	strcat(cat_null, healthy_file);
	do_system(cat_null);

	if(!fp) {
		fp = fopen(healthy_file, "a+");
		if(!fp) fp = stdout;
	}

	vec_foreach_element(vec, each, iface){
		if (!iface)
			continue;
		{
			/* reset counter. */
			nr_rx_bytes = nr_rx_pkts = nr_tx_bytes = nr_tx_pkts = 0;
			
			for (lcore = 0; lcore < MAX_LCORES; lcore ++) {

				lcore_nr_rx_bytes[lcore] = lcore_nr_rx_pkts[lcore] =\
					lcore_nr_tx_bytes[lcore] = lcore_nr_tx_pkts[lcore] = 0;
				
				lcore_nr_rx_pkts[lcore] = oryx_counter_get(iface_perf(iface),
					iface->if_counter_ctx->lcore_counter_pkts[QUA_RX][lcore]);
				lcore_nr_rx_bytes[lcore] = oryx_counter_get(iface_perf(iface),
					iface->if_counter_ctx->lcore_counter_bytes[QUA_RX][lcore]);
				nr_rx_bytes += lcore_nr_rx_bytes[lcore];
				nr_rx_pkts += lcore_nr_rx_pkts[lcore];

				lcore_nr_tx_pkts[lcore] = oryx_counter_get(iface_perf(iface),
					iface->if_counter_ctx->lcore_counter_pkts[QUA_TX][lcore]);
				lcore_nr_tx_bytes[lcore] = oryx_counter_get(iface_perf(iface),
					iface->if_counter_ctx->lcore_counter_bytes[QUA_TX][lcore]);
				nr_tx_bytes += lcore_nr_tx_bytes[lcore];
				nr_tx_pkts += lcore_nr_tx_pkts[lcore];
			}

			fprintf (fp, "=== %s\n", iface_alias(iface));
			fprintf (fp, "%12s%12s%12lu", " ", "Rx pkts:", nr_rx_pkts);
			fprintf (fp, "%12s%12s%12lu", " ", "Rx bytes:", nr_rx_bytes);

			fprintf (fp, "%s", "\n");
			fflush(fp);
		}
	}
}

static __oryx_always_inline__
void iface_activity_prob_tmr_handler
(
	IN struct oryx_timer_t __oryx_unused__*tmr,
	IN int __oryx_unused__ argc,
	IN char __oryx_unused__**argv
)
{
	vlib_iface_main_t *pm = &vlib_iface_main;	
	int each;
	struct iface_t *this;

	vec_foreach_element(pm->entry_vec, each, this) {
		if (likely(this != NULL)) {
			/** check linkstate. */
			if (this->if_poll_state)
				this->if_poll_state(this);

			/** poll a iface up if need. */
			if(this->ul_flags & NETDEV_POLL_UP) {
				/** up this interface right now. */
				if(this->if_poll_up) {
					this->if_poll_up(this);
				} else {
					oryx_loge(-1,
						"ethdev up driver is not registered, this iface will down forever.");
				}
			}
		}
	}
}

static void
register_ports(void)
{
	int i;
	struct iface_t *new;
	struct iface_t *this;
	vlib_iface_main_t *pm = &vlib_iface_main;
	int n_ports_now = vec_count(pm->entry_vec);

	/** SW<->CPU ports, only one. */
	for (i = 0; i < (int)DIM(iface_list); i ++) {

		this = &iface_list[i];
	#if 0	
		/** lan1-lan8 are not vfio-pci drv iface. */
		if (this->type == ETH_GE && (this->ul_flags & NETDEV_PANEL)) {
			if (!netdev_exist(this->sc_alias_fixed)) {
				continue;
			}
		}
	#endif
		
		iface_alloc(&new);
		if (!new) {
			oryx_panic(-1, 
				"Can not alloc memory for a iface");
		} else {
			memcpy(&new->sc_alias[0], this->sc_alias_fixed, strlen(this->sc_alias_fixed));
			new->sc_alias_fixed		= this->sc_alias_fixed;
			new->type				= this->type;
			new->if_poll_state		= this->if_poll_state;
			new->if_poll_up			= this->if_poll_up;
			new->ul_id				= n_ports_now + i;
			new->ul_flags			= this->ul_flags;
			if (iface_add(pm, new))
				oryx_panic(-1, 
					"registering interface %s ... error", iface_alias(new));
		}
	}
}


__oryx_always_extern__
void vlib_iface_init
(
	IN vlib_main_t *vm
)
{
	vlib_iface_main_t *pm = &vlib_iface_main;
	
	pm->link_detect_tmr_interval = 3;
	pm->entry_vec	= vec_init (MAX_PORTS);
	pm->htable		= oryx_htable_init(DEFAULT_HASH_CHAIN_SIZE, 
							ht_iface_hval, ht_iface_cmp, ht_iface_free, 0);

	if (pm->htable == NULL || pm->entry_vec == NULL)
		oryx_panic(-1, 
			"vlib iface main init error!");
	    
	register_ports();

	pm->link_detect_tmr = oryx_tmr_create(1, "iface activity monitoring tmr", 
							(TMR_OPTIONS_PERIODIC | TMR_OPTIONS_ADVANCED),
							iface_activity_prob_tmr_handler,
							0, NULL, pm->link_detect_tmr_interval);
	if(likely(pm->link_detect_tmr != NULL))
		oryx_tmr_start(pm->link_detect_tmr);

	pm->healthy_tmr = oryx_tmr_create(1, "iface healthy monitoring tmr", 
							(TMR_OPTIONS_PERIODIC | TMR_OPTIONS_ADVANCED),
							iface_healthy_tmr_handler,
							0, NULL, 3);
	if(likely(pm->healthy_tmr != NULL))
		oryx_tmr_start(pm->healthy_tmr);

	vm->ul_flags |= VLIB_PORT_INITIALIZED;

}

