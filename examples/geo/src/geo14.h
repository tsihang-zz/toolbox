#ifndef GEO14_H
#define GEO14_H

typedef struct full_record_scg_head_s
{
	uint16_t	len;
	uint16_t	msg_type;
	uint16_t	seq_id;
	uint16_t	reserved;
	uint32_t	offset;
}full_record_scg_head_t;

typedef struct full_record_cdr_head_s
{
	full_record_scg_head_t scg_head;
	uint16_t	total_len;
	uint16_t	table_id;
	uint16_t	service_detail;
	uint16_t   policy_id;
	uint64_t	start_time;
	uint64_t	cdr_id;
	uint8_t	device_id;
	uint8_t filter_flag;
	uint8_t	data_type;
	uint8_t cpu_clock_mul;
	uint8_t	reserved1[4];
}full_record_cdr_head_t;

typedef struct full_record_rawdata_head_s
{
	full_record_scg_head_t scg_head;
	uint16_t	total_len;
	uint16_t	table_id;
	uint16_t	service_detail;
	uint16_t   policy_id;
	uint64_t	start_time;
	uint64_t	cdr_id;
	uint8_t	device_id;
	uint8_t filter_flag;
	uint8_t	data_type;
	uint8_t  cpu_clock_mul;
	uint8_t reserved1_u8[4];
	uint64_t  cdr_start_time;
	char	distribute_key[24];
}full_record_rawdata_head_t;

#define CDR_LEN(cdr_head) ntohs(cdr_head->scg_head.len)

typedef struct {
	full_record_cdr_head_t cdr_head;
	
	uint64_t			sdr_id;
	
	uint8_t 			cdr_type;
	uint8_t 			handover_type;
	uint8_t 			trigger_cause_type;
	uint8_t 			trigger_cause;
	uint8_t 			fail_cause_type;
	uint8_t 			fail_cause;
	uint8_t 			cancel_cause_type;
	uint8_t 			cancel_cause;
	
	int32_t 			cancel_req_count;
	uint8_t 			cancel_result;
	uint8_t 			handover_count;
	uint16_t		   cur_tac;
	
	uint32_t		   cur_eci;
	uint32_t		   cur_enbid;
	
	uint32_t		   src_eci;
	uint32_t		   src_enbid;
	
	uint16_t		   src_tac;
	uint16_t		   dest_tac;
	uint32_t		   dest_eci;
	
	uint32_t		   dest_enbid;
	uint32_t		   mme_ip;
	
	uint32_t		   enodeb_ip;
	uint16_t		   mme_port;
	uint16_t		   enodeb_port;
	
	uint32_t		   mme_ue_s1apid;
	uint32_t		   enb_ue_s1apid;
	
	uint16_t		   mme_group_id;
	uint8_t 			mme_code;
	uint8_t 			ip_type;
	uint32_t		   user_ipv4;
	
	uint8_t 		   user_ipv6[16];
	
	int32_t 			ue_maxbitrate_dl;
	int32_t 			ue_maxbitrate_ul;
	
	int64_t 	  	resp_delay;
	int64_t 	 	handover_delay;
	
	int64_t	 		total_time;
	
	uint32_t		   m_tmsi;
	char		  imsi[18];
	char		  calling[24];
	char		  imei[18];
	char		  apn[32];
	uint8_t			cdr_result;
	uint8_t			reserved[7];
}full_record_s1ap_handover_signal_t;

typedef struct{
	full_record_cdr_head_t cdr_head;
	uint64_t sdr_id;
	uint8_t cdr_type;
	uint8_t msg_type;
	uint8_t cause;
	uint8_t ip_type;
	uint32_t user_ipv4;
	uint8_t user_ipv6[16];
	uint32_t m_tmsi;
	uint16_t mme_group_id;
	uint8_t mme_code;
	uint8_t req_count;
	uint32_t mme_ue_s1ap_id;
	uint32_t enb_ue_s1ap_id;
	uint32_t mme_ip;
	uint32_t enodeb_ip;
	uint16_t mme_port;
	uint16_t enodeb_port;
	uint16_t cur_tac;
	uint16_t src_tac;
	uint32_t cur_eci;
	uint32_t src_eci;
	uint16_t mcc;
	uint16_t mnc;
	uint8_t paging_flag;
	uint8_t ca_type;
	uint8_t cdr_result;
	uint8_t new_mme_code;
	int32_t uplink_count;
	int32_t downlink_count;
	int64_t resp_delay;
	uint64_t total_time;
	char imsi[18];
	char calling[24];
	char imei[18];
	char apn[32];
	uint32_t tmsi;
	uint16_t lac;
	uint16_t new_mme_group_id;
	uint32_t new_m_tmsi;
	uint8_t old_guti_type;
	uint8_t msisdn_capability;
	uint8_t paging_type;
	uint8_t s1ap_cause_type;
	uint8_t s1ap_cause;
	uint8_t s_guti_type;
	char reserved0[2];
	uint8_t rand[16];
	uint8_t autn[16];
	uint8_t res[8];
	uint8_t kasme[32];
}full_record_s1_emm_signal_t;

typedef struct{
	full_record_cdr_head_t cdr_head;
	uint64_t sdr_id;
	uint8_t cdr_type;
	uint8_t msg_type;
	uint8_t cause_type;
	uint8_t cause;
	uint32_t m_tmsi;
	uint16_t mme_group_id;
	uint8_t mme_code;
	uint8_t ip_type;
	uint32_t user_ipv4;
	uint8_t user_ipv6[16];
	uint32_t mme_ue_s1ap_id;
	uint32_t enb_ue_s1ap_id;
	uint32_t mme_ip;
	uint32_t enodeb_ip;
	uint16_t mme_port;
	uint16_t enodeb_port;
	uint32_t cur_eci;
	uint16_t cur_tac;
	uint16_t mcc;
	uint16_t mnc;
	uint8_t req_count;
	uint8_t cdr_result;
	int64_t resp_delay;
	uint64_t total_time;
	char imsi[18];
	char calling[24];
	char imei[18];
	char apn[32];
	uint8_t uecontext_release_type;
	uint8_t uecontext_release_cause;
	
	uint8_t bandEUTRA_r10[13];
	uint8_t ca_BandwidthClassDL_r10[13];
	uint8_t bandEUTRA[13];
	uint8_t accessStratumRelease;
	uint8_t ue_Category;
	uint8_t ue_Category_v1020;
}full_record_s1_mme_signal_t;
#endif