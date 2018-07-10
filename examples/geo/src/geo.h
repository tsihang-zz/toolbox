#ifndef GEO_H
#define GEO_H

#include "geo_cdr_table.h"

#define GEO

#if defined(GEO)
#define NPAS_CDR_14
#endif

#if defined(NPAS_CDR_12)
#include "geo12.h"
#endif

#if defined(NPAS_CDR_13)
#include "geo13.h"
#endif

#if defined(NPAS_CDR_14)
#include "geo14.h"
#endif

#if defined(NPAS_CDR_14_TELECOM)
#include "geo14_telecom.h"
#endif

#if defined(NPAS_CDR_16)
#include "geo16.h"
#endif


#define GEO_CDR_HAVE_REFILL_CACHE		/* CDR which without IMSI will be hold by a cache. 
										 * When a new CDR, which with IMSI and has a same
										 * fingerprint, and this FP is recorded by the cache.
										 * When timeout, all CDRs in it's cache will be delivered
										 * out after refill its own FP. */


#define GEO_CDR_HASH_MODE_M_TMSI			0
#define GEO_CDR_HASH_MODE_MME_UE_S1AP_ID	1
#define GEO_CDR_HASH_MODE	GEO_CDR_HASH_MODE_M_TMSI


#define GEO_CDR_KEY_INFO_BYPASS			(1 << 0)
#define GEO_CDR_KEY_INFO_APPEAR_IMSI	(1 << 1)
#define GEO_CDR_KEY_INFO_APPEAR_MTMSI	(1 << 2)
#define GEO_CDR_KEY_INFO_APPEAR_S1APID	(1 << 3)

struct geo_key_info_t {
	uint32_t	cdr_index;
	uint32_t	m_tmsi;
	uint32_t	mme_ue_s1ap_id;
	char		imsi[18];
	uint8_t		mme_code;
	void		*v;			/** a pointer to this.m_tmsi or this.mme_ue_s1ap_id. */

	uint32_t	ul_flags;
};

#define GEO_CDR_KEY_INFO_INIT_VAL {\
			.mme_code			= -1,\
			.cdr_index			= -1,\
			.m_tmsi				= -1,\
			.mme_ue_s1ap_id		= -1,\
			.imsi		= {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},\
			.ul_flags	= 0,\
		}

static __oryx_always_inline__
void geo_key_info_init(struct geo_key_info_t *gk) {
	gk->cdr_index		= -1;
	gk->m_tmsi			= -1;
	gk->mme_code		= -1;
	gk->mme_ue_s1ap_id	= -1;
	gk->ul_flags		= 0;
	memset (&gk->imsi, 0, 18);
}

static __oryx_always_inline__
const char *fmt_imsi(const char *imsi_hex, int l, char * imsi_str)
{
	int i;
	int s = 0;

	for (i = 0; i < l; i ++) {
		s += snprintf(imsi_str + i, 1024 - s, "%02x", imsi_hex[i]);
	}

	return imsi_str;
}

static __oryx_always_inline__
void dump_cdr_s1_mme(char *cdr)
{
	int i;
	full_record_s1_mme_signal_t *v = (full_record_s1_mme_signal_t *)cdr;

	fprintf(stdout, "====== %s\n", "S1_MME");

	fprintf(stdout, "%32s%8lu\n", "SDR_ID: ", v->sdr_id);
	fprintf(stdout, "%32s%8x\n", "mme_ue_s1ap_id: ", v->mme_ue_s1ap_id);
	fprintf(stdout, "%32s%8x\n", "m_tmsi: ", v->m_tmsi);

	
	fprintf(stdout, "%32s", "imsi: ");
	for (i = 0; i < 18; i ++) {
		fprintf(stdout, "%02x-", v->imsi[i]);
	}
	printf ("\n");

	fprintf(stdout, "%32s", "imei: ");
	for (i = 0; i < 18; i ++) {
		fprintf(stdout, "%02x-", v->imei[i]);
	}
	printf ("\n");

	fprintf(stdout, "%32s", "APN: ");
	for (i = 0; i < 32; i ++) {
		fprintf(stdout, "%02x-", v->apn[i]);
	}
	printf ("\n");

	fprintf(stdout, "%32s", "calling: ");
	for (i = 0; i < 24; i ++) {
		fprintf(stdout, "%02x-", v->calling[i]);
	}
	printf ("\n");

	printf ("\n\n");
}

static __oryx_always_inline__
void dump_cdr_s1_emm(char *cdr)
{
	int i;
	full_record_s1_emm_signal_t *v = (full_record_s1_emm_signal_t *)cdr;

	fprintf(stdout, "====== %s\n", "S1_EMM");

	fprintf(stdout, "%32s%8lu\n", "SDR_ID: ", v->sdr_id);
	fprintf(stdout, "%32s%8x\n", "mme_ue_s1ap_id: ", v->mme_ue_s1ap_id);
	fprintf(stdout, "%32s%8x\n", "m_tmsi: ", v->m_tmsi);


	fprintf(stdout, "%32s", "imsi: ");
	for (i = 0; i < 18; i ++) {
		fprintf(stdout, "%02x-", v->imsi[i]);
	}
	printf ("\n");

	fprintf(stdout, "%32s", "imei: ");
	for (i = 0; i < 18; i ++) {
		fprintf(stdout, "%02x-", v->imei[i]);
	}
	printf ("\n");

	fprintf(stdout, "%32s", "APN: ");
	for (i = 0; i < 32; i ++) {
		fprintf(stdout, "%02x-", v->apn[i]);
	}
	printf ("\n");

	fprintf(stdout, "%32s", "calling: ");
	for (i = 0; i < 24; i ++) {
		fprintf(stdout, "%02x-", v->calling[i]);
	}
	printf ("\n");

	printf ("\n\n");
}

static __oryx_always_inline__
void dump_cdr_s1ap_handover(char *cdr)
{
	int i;
	full_record_s1ap_handover_signal_t *v = (full_record_s1ap_handover_signal_t *)cdr;

	fprintf(stdout, "====== %s\n", "S1AP_HANDOVER");

	fprintf(stdout, "%32s%8lu\n", "SDR_ID: ", v->sdr_id);
	fprintf(stdout, "%32s%8x\n", "mme_ue_s1ap_id: ", v->mme_ue_s1apid);
	fprintf(stdout, "%32s%8x\n", "m_tmsi: ", v->m_tmsi);


	fprintf(stdout, "%32s", "imsi: ");
	for (i = 0; i < 18; i ++) {
		fprintf(stdout, "%02x-", v->imsi[i]);
	}
	printf ("\n");

	fprintf(stdout, "%32s", "imei: ");
	for (i = 0; i < 18; i ++) {
		fprintf(stdout, "%02x-", v->imei[i]);
	}
	printf ("\n");

	fprintf(stdout, "%32s", "APN: ");
	for (i = 0; i < 32; i ++) {
		fprintf(stdout, "%02x-", v->apn[i]);
	}
	printf ("\n");

	fprintf(stdout, "%32s", "calling: ");
	for (i = 0; i < 24; i ++) {
		fprintf(stdout, "%02x-", v->calling[i]);
	}
	printf ("\n");

	printf ("\n\n");
}


#endif
