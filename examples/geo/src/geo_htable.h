#ifndef GEO_HTABLE_H
#define GEO_HTABLE_H

void ht_geo_cdr_free (const ht_value_t v);
ht_key_t ht_geo_cdr_hval (struct oryx_htable_t *ht,
		const ht_value_t v, uint32_t s);
int ht_geo_cdr_cmp (const ht_value_t v1, 
		uint32_t s1,
		const ht_value_t v2,
		uint32_t s2);


#define GEO_CDR_HASH_KEY_APPEAR_IMSI	(1 << 0)
struct geo_htable_key_t {
	uint8_t		mme_code;
	uint32_t	v;		/* mtmsi or mme_ue_s1ap_id */
	uint8_t		imsi[18];
	uint32_t	ul_flags;
	void		*inner_cdr_queue;		/* A queue for CDRs which has a same hash_key. */
};

struct oryx_htable_t *geo_cdr_hash_table;

#define GEO_CDR_HASH_KEY_INIT_VAL {\
		.mme_code	= -1,\
		.v			= -1,\
		.imsi		= {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},\
		.ul_flags	= 0,\
		.inner_cdr_queue	= NULL,\
	}

static __oryx_always_inline__
struct geo_htable_key_t *alloc_hk(void)
{
	struct geo_htable_key_t *h = malloc(sizeof(struct geo_htable_key_t));
	BUG_ON(h == NULL);
	memset(h, 0, sizeof(struct geo_htable_key_t));
	fq_new("inner_cdr_queue", (struct qctx_t **)&h->inner_cdr_queue);
	return h;
}

#endif
