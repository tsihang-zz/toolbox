#ifndef GEO_CDR_TABLE_H
#define GEO_CDR_TABLE_H

#include "geo.h"

enum {
	cdr_source = 0,	
	cdr_gn_signal,
	cdr_gtpv2_signal,
	cdr_gtp_s3_signal,
	cdr_s1_mme_signal,
	cdr_s1_emm_signal,
	cdr_s1_esm_signal,
	cdr_s1ap_erab_signal,
	cdr_s1ap_handover_signal,
	cdr_s1ap_manage_signal,
	cdr_s6a_diameter_signal,
	cdr_s1_sms_signal,
	cdr_s1_csfb_signal,
	cdr_sgs_mm_signal,
	cdr_sgs_cs_signal,
	cdr_x2_manage_signal,
	cdr_x2_handover_signal,
	cdr_uu_signal,
	cdr_uu_mobility_signal,
	cdr_uu_mr_ue_signal,
	cdr_uu_mr_cell_signal,
	
	cdr_sdr,
	cdr_wap,
	cdr_dns,
	cdr_mms_mo,
	cdr_mms_mt,
	cdr_wechat,
	cdr_wap_conn,
	cdr_ftp,
	cdr_rtsp,
	cdr_email,
	cdr_voip,
	cdr_p2p,
	cdr_flowstat,
	cdr_tcp,

	#if defined(NPAS_CDR_14_TELECOM)||defined(NPAS_CDR_16)
	cdr_online_video,
	cdr_radius_signal,
	#endif

	/*telecom LTE*/
	#if defined(NPAS_CDR_14_TELECOM)
	cdr_wap_real,
	cdr_smtp,
	cdr_imap,
	cdr_icmp,
	cdr_a11_signal,
	cdr_a12_signal,
	cdr_pmip_signal,
	cdr_type_p2p_download,
	cdr_type_p2p_video,
	cdr_type_voip,
	cdr_type_game,
	cdr_type_trade,
	cdr_type_read,
	cdr_type_music,
	cdr_type_app,
	cdr_ms_mms,
	#endif
	
	#if defined(NPAS_CDR_14)||defined(NPAS_CDR_16)
	cdr_irangb_zc_gmm_attach,
	cdr_irangb_zc_gmm_detach,
	cdr_irangb_zc_gmm_rau,
	cdr_irangb_zc_gmm_service_request,
	cdr_irangb_zc_gmm_other,
	cdr_irangb_zc_sm_pdp_active,
	cdr_irangb_zc_sm_pdp_deactive,
	cdr_irangb_zc_sm_pdp_modify,
	cdr_irangb_zc_user_pdp,
	cdr_irangb_zc_ranap_rab,
	cdr_irangb_zc_ranap_relocation,
	cdr_irangb_zc_ranap_other,
	cdr_irangb_zc_bssgp,
	cdr_irangb_zc_ns,
	cdr_irangb_zc_mms,
	cdr_ns_nsvc_traffic,
	cdr_bssgp_bvc_traffic,
	#endif
	cdr_num
};

#define GN_ID_START 0
#define VIRTUAL_ID_START 50000
#define SOURCE_ID_START 60000

enum{
	tableID_source = SOURCE_ID_START + 0,
	/*GN Signal*/
	tableID_gn_signal = GN_ID_START + 3300,
	/*LTE Signal*/
	tableID_gtpv2_signal = GN_ID_START + 5200,
	tableID_gtp_s3_signal = GN_ID_START + 5201,
	tableID_s1_mme_signal = GN_ID_START + 3102,
	tableID_s1_emm_signal = GN_ID_START + 3106,
	tableID_s1_esm_signal = GN_ID_START + 3107,
	tableID_s1ap_erab_signal = GN_ID_START + 3104,
	tableID_s1ap_handover_signal = GN_ID_START + 3103,
	tableID_s1ap_manage_signal = GN_ID_START + 3105,
	tableID_s6a_diameter_signal = GN_ID_START + 3301,
	tableID_s1_sms_signal = GN_ID_START + 3108,
	tableID_s1_csfb_signal = GN_ID_START + 3109,
	tableID_sGs_mm_signal = GN_ID_START + 3302,
	tableID_sGs_cs_signal = GN_ID_START + 3303,
	tableID_x2_manage_signal = GN_ID_START + 3112,
	tableID_x2_handover_signal = GN_ID_START + 3113,
	tableID_uu_signal = GN_ID_START + 3114,
	tableID_uu_mobility_signal = GN_ID_START + 3115,
	tableID_uu_mr_ue_signal = GN_ID_START + 3117,
	tableID_uu_mr_cell_signal = GN_ID_START + 3116,
	/*VoLTE Signal*/
	tableID_volte_sdr = GN_ID_START + 6018,
	tableID_volte_sdr_list_bdr = GN_ID_START + 3305,
	tableID_sv_bdr = GN_ID_START + 5140,
	tableID_s1_emm_bdr = GN_ID_START + 5106,
	tableID_s1_esm_bdr = GN_ID_START + 5107,
	tableID_s1_handover_bdr = GN_ID_START + 5103,
	tableID_sip_cc_bdr = GN_ID_START + 6002,
	tableID_sip_reg_bdr = GN_ID_START + 6013,
	tableID_ims_diameter_bdr = GN_ID_START + 6012,
	tableID_sip_other_message_bdr = GN_ID_START + 6016,
	tableID_sip_content_volte_bdr = GN_ID_START + 6017,
	tableID_rtp_rtcp_bdr = GN_ID_START + 6010,
	tableID_dhcp_bdr = GN_ID_START + 6015,
	/*Telecom LTE*/
	tableID_icmp = GN_ID_START + 3355,
	tableID_a11_signal = GN_ID_START + 3124,
	tableID_radius_signal = GN_ID_START + 3125,
	tableID_pmip_signal = GN_ID_START + 3126,
	
	/*GN & LTE USER-DATA*/
	tableID_sdr = GN_ID_START + 6000,
	tableID_wap = GN_ID_START + 3005,
	tableID_dns	= GN_ID_START + 3080,
	tableID_mms_mo = GN_ID_START + 3001,
	tableID_mms_mt = GN_ID_START + 3003,
	tableID_wechat = GN_ID_START + 3082,
	tableID_wap_conn = GN_ID_START + 3024,
	tableID_ftp = GN_ID_START + 3329,
	tableID_rtsp = GN_ID_START + 3330,
	tableID_email = GN_ID_START + 3047,
	tableID_voip = GN_ID_START + 3046,
	tableID_p2p = GN_ID_START + 3350,
	
	tableID_flowstat = GN_ID_START + 3354,
	tableID_online_video = GN_ID_START + 3006,

	/*telecom*/
	tableID_ms_mms = GN_ID_START + 3331,

	/*Iran gb CDR*/
	tableID_irangb_zc_gmm_attach = GN_ID_START + 8501,
	tableID_irangb_zc_gmm_detach = GN_ID_START + 8502,
	tableID_irangb_zc_gmm_rau = GN_ID_START + 8503,
	tableID_irangb_zc_gmm_service_request = GN_ID_START + 8504,
	tableID_irangb_zc_gmm_other = GN_ID_START + 8508,
	tableID_irangb_zc_sm_pdp_active = GN_ID_START + 8511,
	tableID_irangb_zc_sm_pdp_deactive = GN_ID_START + 8512,
	tableID_irangb_zc_sm_pdp_modify = GN_ID_START + 8513,
	tableID_irangb_zc_sm_other = GN_ID_START + 8518,
	tableID_irangb_zc_user_pdp = GN_ID_START + 8519,
	tableID_irangb_zc_ranap_rab = GN_ID_START + 8521,
	tableID_irangb_zc_ranap_relocation = GN_ID_START + 8522,
	tableID_irangb_zc_ranap_other = GN_ID_START + 8523,
	tableID_irangb_zc_bssgp = GN_ID_START + 8530,
	tableID_irangb_zc_ns = GN_ID_START + 8540,
	tableID_irangb_zc_mms = GN_ID_START + 3004,
	tableID_bssgp_bvc_traffic = GN_ID_START + 8538,
	tableID_ns_nsvc_traffic = GN_ID_START + 8548,
	/*Virutal CDR*/
	tableID_tcp = VIRTUAL_ID_START + 0,
	tableID_a12_signal = VIRTUAL_ID_START + 1,
	tableID_imap = VIRTUAL_ID_START + 2,
	tableID_smtp = VIRTUAL_ID_START + 3
};


static __oryx_always_inline__
int _GetCDRIndex(uint32_t cdr_table_id)
{
	int cdr_index;
	
	switch(cdr_table_id)
	{
		case tableID_source:
			cdr_index = cdr_source;
			break;		
		#if defined(NPAS_CDR_12)||defined(NPAS_CDR_13)||defined(NPAS_CDR_14)||defined(NPAS_CDR_14_TELECOM)||defined(NPAS_CDR_16)
		/*GN Signal*/
		case tableID_gn_signal:
			cdr_index = cdr_gn_signal;
			break;
		/*LTE Signal*/
		case tableID_gtpv2_signal:
			cdr_index = cdr_gtpv2_signal;
			break;
		case tableID_gtp_s3_signal:
			cdr_index = cdr_gtp_s3_signal;
			break;
		case tableID_s1_mme_signal:
			cdr_index = cdr_s1_mme_signal;
			break;
		case tableID_s1_emm_signal:
			cdr_index = cdr_s1_emm_signal;
			break;
		case tableID_s1_esm_signal:
			cdr_index = cdr_s1_esm_signal;
			break;
		case tableID_s1ap_erab_signal:
			cdr_index = cdr_s1ap_erab_signal;
			break;
		case tableID_s1ap_handover_signal:
			cdr_index = cdr_s1ap_handover_signal;
			break;
		case tableID_s1ap_manage_signal:
			cdr_index = cdr_s1ap_manage_signal;
			break;
		case tableID_s6a_diameter_signal:
			cdr_index = cdr_s6a_diameter_signal;
			break;
		case tableID_s1_sms_signal:
			cdr_index = cdr_s1_sms_signal;
			break;
		case tableID_s1_csfb_signal:
			cdr_index = cdr_s1_csfb_signal;
			break;
		case tableID_sGs_mm_signal:
			cdr_index = cdr_sgs_mm_signal;
			break;
		case tableID_sGs_cs_signal:
			cdr_index = cdr_sgs_cs_signal;
			break;
		case tableID_x2_manage_signal:
			cdr_index = cdr_x2_manage_signal;
			break;
		case tableID_x2_handover_signal:
			cdr_index = cdr_x2_handover_signal;
			break;
		case tableID_uu_signal:
			cdr_index = cdr_uu_signal;
			break;
		case tableID_uu_mobility_signal:
			cdr_index = cdr_uu_mobility_signal;
			break;
		case tableID_uu_mr_ue_signal:
			cdr_index = cdr_uu_mr_ue_signal;
			break;
		case tableID_uu_mr_cell_signal:
			cdr_index = cdr_uu_mr_cell_signal;
			break;

		/*GN & LTE USER-DATA*/
		case tableID_sdr:
			cdr_index = cdr_sdr;
			break;
		case tableID_wap:
			cdr_index = cdr_wap;
			break;
		case tableID_dns:
			cdr_index = cdr_dns;
			break;
		case tableID_mms_mo:
			cdr_index = cdr_mms_mo;
			break;
		case tableID_mms_mt:
			cdr_index = cdr_mms_mt;
			break;
		case tableID_wechat:
			cdr_index = cdr_wechat;
			break;
		case tableID_wap_conn:
			cdr_index = cdr_wap_conn;
			break;
		case tableID_ftp:
			cdr_index = cdr_ftp;
			break;
		case tableID_rtsp:
			cdr_index = cdr_rtsp;
			break;
		case tableID_email:
			cdr_index = cdr_email;
			break;
		case tableID_voip :
			cdr_index = cdr_voip;
			break;
		case tableID_p2p:
			cdr_index = cdr_p2p;
			break;		
		case tableID_flowstat:
			cdr_index = cdr_flowstat;
			break;
		case tableID_tcp:
			cdr_index = cdr_tcp;
			break;
		#endif
		#if defined(NPAS_CDR_14_TELECOM)||defined(NPAS_CDR_16)
		case tableID_online_video:
			cdr_index = cdr_online_video;
			break;
		case tableID_radius_signal:
			cdr_index = cdr_radius_signal;
			break;
		#endif
	/*VoLTE Signal*/
	#if 0		
		case tableID_volte_sdr:
			cdr_index = cdr_volte_sdr;
			break;
		case tableID_volte_sdr_list_bdr:
			cdr_index = cdr_volte_sdr_list_bdr;
			break;
		case tableID_sv_bdr:
			cdr_index = cdr_sv_bdr;
			break;
		case tableID_s1_emm_bdr:
			cdr_index = cdr_s1_emm_bdr;
			break;
		case tableID_s1_esm_bdr:
			cdr_index = cdr_s1_esm_bdr;
			break;
		case tableID_s1_handover_bdr:
			cdr_index = cdr_s1_handover_bdr;
			break;
		case tableID_sip_cc_bdr:
			cdr_index = cdr_sip_cc_bdr;
			break;
		case tableID_sip_reg_bdr:
			cdr_index = cdr_sip_reg_bdr;
			break;
		case tableID_ims_diameter_bdr:
			cdr_index = cdr_ims_diameter_bdr;
			break;
		case tableID_sip_other_message_bdr:
			cdr_index = cdr_sip_other_message_bdr;
			break;	
		case tableID_sip_content_volte_bdr:
			cdr_index = cdr_sip_content_volte_bdr;
			break;
		case tableID_rtp_rtcp_bdr:
			cdr_index = cdr_rtp_rtcp_bdr;
			break;
		case tableID_dhcp_bdr:
			cdr_index = cdr_dhcp_bdr;
			break;
	#endif
	#if defined(NPAS_CDR_14_TELECOM)
		case tableID_a11_signal:
			cdr_index = cdr_a11_signal;
			break;
		case tableID_pmip_signal:
			cdr_index = cdr_pmip_signal;
			break;
		case tableID_icmp:
			cdr_index = cdr_icmp;
			break; 
		case tableID_ms_mms:
			cdr_index = cdr_ms_mms;
			break; 
	#endif
		#if defined(NPAS_CDR_14)||defined(NPAS_CDR_16)
		case tableID_irangb_zc_gmm_attach:
			cdr_index = cdr_irangb_zc_gmm_attach;
			break;
		case tableID_irangb_zc_gmm_detach:
			cdr_index = cdr_irangb_zc_gmm_detach;
			break;
		case tableID_irangb_zc_gmm_rau:
			cdr_index = cdr_irangb_zc_gmm_rau;
			break;
		case tableID_irangb_zc_gmm_service_request:
			cdr_index = cdr_irangb_zc_gmm_service_request;
			break;
		case tableID_irangb_zc_gmm_other:
			cdr_index = cdr_irangb_zc_gmm_other;
			break;
		case tableID_irangb_zc_sm_pdp_active:
			cdr_index = cdr_irangb_zc_sm_pdp_active;
			break;
		case tableID_irangb_zc_sm_pdp_deactive:
			cdr_index = cdr_irangb_zc_sm_pdp_deactive;
			break;
		case tableID_irangb_zc_sm_pdp_modify:
			cdr_index = cdr_irangb_zc_sm_pdp_modify;
			break;
		//case tableID_irangb_zc_sm_other:
			//cdr_index = cdr_irangb_zc_sm_other;
			//break;
		case tableID_irangb_zc_user_pdp:
			cdr_index = cdr_irangb_zc_user_pdp;
			break;
		case tableID_irangb_zc_ranap_rab:
			cdr_index = cdr_irangb_zc_ranap_rab;
			break;
		case tableID_irangb_zc_ranap_relocation:
			cdr_index = cdr_irangb_zc_ranap_relocation;
			break;
		case tableID_irangb_zc_ranap_other:
			cdr_index = cdr_irangb_zc_ranap_other;
			break;
		case tableID_irangb_zc_bssgp:
			cdr_index = cdr_irangb_zc_bssgp;
			break;
		case tableID_irangb_zc_ns:
			cdr_index = cdr_irangb_zc_ns;
			break;
		case tableID_irangb_zc_mms:
			cdr_index = cdr_irangb_zc_mms;
			break;
		case tableID_ns_nsvc_traffic:
			cdr_index = cdr_ns_nsvc_traffic;
			break;
		case tableID_bssgp_bvc_traffic:
			cdr_index = cdr_bssgp_bvc_traffic;
			break;
		#endif
		default:
			cdr_index = cdr_num;
			break;
	}

	return cdr_index;
}

static __oryx_always_inline__
int GetCDRIndex(void * pkg , int * cdr_index)
{
	(*cdr_index) = cdr_source;
	full_record_cdr_head_t *cdr_head = (full_record_cdr_head_t*)pkg;

	if(cdr_head->data_type == 0)
		*cdr_index = _GetCDRIndex(ntohs(cdr_head->table_id));

	return 0;
}

static __oryx_always_inline__
int _Is_AllNumber(char *num_str)
{
	int i = 0;
	
	while(num_str[i]) {
		if(num_str[i] != '+' && num_str[i] != '-' 
			&& (num_str[i] < '0' || num_str[i] > '9')
			&& (num_str[i] < 'A' || num_str[i] > 'F')
			&& (num_str[i] < 'a' || num_str[i] > 'f'))
			return 0;
		
		i++;
	}
	
	if(i == 0)
		return 0;
	
	return 1;
}

static __oryx_always_inline__
int geo_get_key_info(char *cdr_pkt, struct geo_key_info_t *gk, int cdr_index)
{	
	switch(cdr_index)
	{
		case cdr_s1_emm_signal:
		{
			full_record_s1_emm_signal_t *v = (full_record_s1_emm_signal_t *)cdr_pkt;
			gk->mme_code		= v->mme_code;
			gk->m_tmsi			= v->m_tmsi;
			gk->mme_ue_s1ap_id	= v->mme_ue_s1ap_id;
			if(v->imsi[0] != 0) {
				strncpy(gk->imsi, v->imsi, 18);
				gk->ul_flags |= GEO_CDR_KEY_INFO_APPEAR_IMSI;
			}
			break;
		}
		case cdr_s1ap_handover_signal:
		{
			full_record_s1ap_handover_signal_t *v = (full_record_s1ap_handover_signal_t *)cdr_pkt;
			gk->mme_code		= v->mme_code;
			gk->m_tmsi			= v->m_tmsi;
			gk->mme_ue_s1ap_id	= v->mme_ue_s1apid;
			if(v->imsi[0] != 0) {
				strncpy(gk->imsi, v->imsi, 18);
				gk->ul_flags |= GEO_CDR_KEY_INFO_APPEAR_IMSI;
			}
			break;
		}
		case cdr_s1_mme_signal:
		{
			full_record_s1_mme_signal_t * v = (full_record_s1_mme_signal_t *)cdr_pkt;
			gk->mme_code		= v->mme_code;
			gk->m_tmsi			= v->m_tmsi;
			gk->mme_ue_s1ap_id	= v->mme_ue_s1ap_id;
			if(v->imsi[0] != 0) {
				strncpy(gk->imsi, v->imsi, 18);
				gk->ul_flags |= GEO_CDR_KEY_INFO_APPEAR_IMSI;
			}
			break;
		}
		default:
			gk->ul_flags |= GEO_CDR_KEY_INFO_BYPASS;
			return -1;
	}

	if(gk->m_tmsi != (uint32_t)-1)
		gk->ul_flags |= GEO_CDR_KEY_INFO_APPEAR_MTMSI;
	if(gk->mme_ue_s1ap_id != (uint32_t)-1)
		gk->ul_flags |= GEO_CDR_KEY_INFO_APPEAR_S1APID;

	/* no mtmsi and s1ap */
#if defined(GEO_CDR_HASH_MODE_M_TMSI)
	if (!(gk->ul_flags & GEO_CDR_KEY_INFO_APPEAR_MTMSI))
		gk->ul_flags |= GEO_CDR_KEY_INFO_BYPASS;
#else
	if(!(gk->ul_flags & GEO_CDR_KEY_INFO_APPEAR_S1APID))
		gk->ul_flags |= GEO_CDR_KEY_INFO_BYPASS;
#endif

	gk->cdr_index = cdr_index;
	
	return 0;
}

struct geo_cdr_table_t {
	uint8_t			cdr_index;
	uint32_t		table_id;
	const char		*cdr_name;
	size_t			length;
}*geo_cdr_tables[cdr_num];


#endif
