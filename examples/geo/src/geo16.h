#ifndef GEO16_H
#define GEO16_H


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
	uint64_t sdr_id;
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

#endif

