#ifndef GEO_H
#define GEO_H

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

struct geo_stat_t {
	uint64_t		rx_pkts;			/** all udp pkts */
	uint64_t		rx_cdr_pkts;		/** all cdr pkts. */
	uint64_t		rx_valid_cdr_pkts;	/** all cdr pkts which we care about. */	
	uint64_t		rx_invalid_cdr_pkts;/** all invalid cdr pkts */
	uint64_t		rx_bypass_cdr_pkts;	/** rx_bypass_cdr_pkts + rx_valid_cdr_pkts = rx_cdr_pkts */

	uint64_t		total_actually_refill_pkts;
	uint64_t		total_refill_pkts;
};

#if 0
#define GEO_CDR_HAVE_REFILL_CACHE		/* CDR which without IMSI will be hold by a cache. 
										 * When a new CDR, which with IMSI and has a same
										 * fingerprint, and this FP is recorded by the cache.
										 * When timeout, all CDRs in it's cache will be delivered
										 * out after refill its own FP. */
#endif

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
	uint32_t	ul_flags;

#define GEO_CDR_KEY_INFO_INIT_VAL {\
				.mme_code			= -1,\
				.cdr_index			= -1,\
				.m_tmsi 			= -1,\
				.mme_ue_s1ap_id 	= -1,\
				.imsi		= {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},\
				.ul_flags	= 0,\
			}
};


struct geo_log_file_t {
	const char *fp_path;
	const char	*fp_comment;
	oryx_file_t *fp;
	char	md5[16];	/** file change. */

#define GEO_LOG_FILE_INIT_VAL {\
				.fp_path		= -1,\
				.fp_comment 	= -1,\
				.fp 			= NULL,\
				.md5			= {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},\
			}
};




static __oryx_always_inline__
void geo_key_info_dump(struct geo_key_info_t *gk, FILE *fp)
{
	int i;
	
	fprintf(fp, "%16u\t%16u\t%16u\t",
		gk->m_tmsi,
		gk->mme_code,
		gk->mme_ue_s1ap_id);
	for (i = 0; i < 15; i ++) {
		fprintf(fp, "%02x", gk->imsi[i]);
	}
	fprintf(fp, "%s", "\n");
	
	fflush(fp);
}

static __oryx_always_inline__
void geo_key_info_init(struct geo_key_info_t *dst, struct geo_key_info_t *src) {
	if (!src) {
		dst->cdr_index		= -1;
		dst->m_tmsi			= -1;
		dst->mme_code		= -1;
		dst->mme_ue_s1ap_id	= -1;
		dst->ul_flags		= 0;
		memset (&dst->imsi, 0, 18);
	} else {
		dst->cdr_index		= src->cdr_index;
		dst->m_tmsi			= src->m_tmsi;
		dst->mme_code		= src->mme_code;
		dst->mme_ue_s1ap_id	= src->mme_ue_s1ap_id;
		dst->ul_flags		= src->ul_flags;
		memcpy ((void *)&dst->imsi[0], (void *)&src->imsi[0], 18);
	}
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

	fprintf (stdout, "====== %s\n", "S1_MME");

	fprintf (stdout, "%32s%8lu\n", "SDR_ID: ", v->sdr_id);
	fprintf (stdout, "%32s%8x\n", "mme_ue_s1ap_id: ", v->mme_ue_s1ap_id);
	fprintf (stdout, "%32s%8x\n", "m_tmsi: ", v->m_tmsi);

	
	fprintf (stdout, "%32s", "imsi: ");
	for (i = 0; i < 18; i ++) {
		fprintf (stdout, "%02x-", v->imsi[i]);
	}
	fprintf (stdout, "\n");

	fprintf (stdout, "%32s", "imei: ");
	for (i = 0; i < 18; i ++) {
		fprintf (stdout, "%02x-", v->imei[i]);
	}
	fprintf (stdout, "\n");

	fprintf (stdout, "%32s", "APN: ");
	for (i = 0; i < 32; i ++) {
		fprintf (stdout, "%02x-", v->apn[i]);
	}
	fprintf (stdout, "\n");

	fprintf (stdout, "%32s", "calling: ");
	for (i = 0; i < 24; i ++) {
		fprintf (stdout, "%02x-", v->calling[i]);
	}
	fprintf (stdout, "\n");

	fprintf (stdout, "\n\n");
}

static __oryx_always_inline__
void dump_cdr_s1_emm(char *cdr)
{
	int i;
	full_record_s1_emm_signal_t *v = (full_record_s1_emm_signal_t *)cdr;

	fprintf (stdout, "====== %s\n", "S1_EMM");

	fprintf (stdout, "%32s%8lu\n", "SDR_ID: ", v->sdr_id);
	fprintf (stdout, "%32s%8x\n", "mme_ue_s1ap_id: ", v->mme_ue_s1ap_id);
	fprintf (stdout, "%32s%8x\n", "m_tmsi: ", v->m_tmsi);


	fprintf (stdout, "%32s", "imsi: ");
	for (i = 0; i < 18; i ++) {
		fprintf (stdout, "%02x-", v->imsi[i]);
	}
	fprintf (stdout, "\n");

	fprintf (stdout, "%32s", "imei: ");
	for (i = 0; i < 18; i ++) {
		fprintf (stdout, "%02x-", v->imei[i]);
	}
	fprintf (stdout, "\n");

	fprintf (stdout, "%32s", "APN: ");
	for (i = 0; i < 32; i ++) {
		fprintf (stdout, "%02x-", v->apn[i]);
	}
	fprintf (stdout, "\n");

	fprintf (stdout, "%32s", "calling: ");
	for (i = 0; i < 24; i ++) {
		fprintf (stdout, "%02x-", v->calling[i]);
	}
	fprintf (stdout, "\n");

	fprintf (stdout, "\n\n");
}

static __oryx_always_inline__
void dump_cdr_s1ap_handover(char *cdr)
{
	int i;
	full_record_s1ap_handover_signal_t *v = (full_record_s1ap_handover_signal_t *)cdr;

	fprintf (stdout, "====== %s\n", "S1AP_HANDOVER");

	fprintf (stdout, "%32s%8lu\n", "SDR_ID: ", v->sdr_id);
	fprintf (stdout, "%32s%8x\n", "mme_ue_s1ap_id: ", v->mme_ue_s1apid);
	fprintf (stdout, "%32s%8x\n", "m_tmsi: ", v->m_tmsi);


	fprintf (stdout, "%32s", "imsi: ");
	for (i = 0; i < 18; i ++) {
		fprintf (stdout, "%02x-", v->imsi[i]);
	}
	fprintf (stdout, "\n");

	fprintf (stdout, "%32s", "imei: ");
	for (i = 0; i < 18; i ++) {
		fprintf (stdout, "%02x-", v->imei[i]);
	}
	fprintf (stdout, "\n");

	fprintf (stdout, "%32s", "APN: ");
	for (i = 0; i < 32; i ++) {
		fprintf (stdout, "%02x-", v->apn[i]);
	}
	fprintf (stdout, "\n");

	fprintf (stdout, "%32s", "calling: ");
	for (i = 0; i < 24; i ++) {
		fprintf (stdout, "%02x-", v->calling[i]);
	}
	fprintf (stdout, "\n");

	fprintf (stdout, "\n\n");
}


#endif
