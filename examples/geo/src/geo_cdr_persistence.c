#include "oryx.h"
#include "geo_cdr_table.h"
#include "geo_htable.h"
#include "geo_cdr_persistence.h"

#include "sqlconnector.h"

struct geo_log_file_t cdr_stats_out_fp = {
	.fp_path	=	"/tmp/cdr_stats_out.txt",
	.fp_comment	=	"cdr stats out logging.",
	.fp			=	NULL,
};

struct geo_log_file_t cdr_record_error_fp = {
	.fp_path	=	"/tmp/cdr_error.txt",
	.fp_comment	=	"invalid cdr record and error message out logging.",
	.fp			=	NULL,
};

struct geo_log_file_t cdr_record_refill_result_fp = {
	.fp_path	=	"/tmp/cdr_refill_result.txt",
	.fp_comment	=	"cdr refill result logging.",
	.fp			=	NULL,
};

struct sql_load_data_t s1mme_buf;
struct sql_load_data_t s1emm_buf;
struct sql_load_data_t s1ap_buf;

struct oryx_sqlctx_t *sqlctx;


void geo_db_delete_table(const char *cdr_table_name)
{
	char sql[1024] = {0};

	sprintf(sql, "DELETE from %s",
		cdr_table_name);
	sqlctx->sql_cmd_type = SQLCMD_DELETE;
	sqlee_run (sqlctx, sql);
}

void geo_db_load_table(const char *filepath, const char *cdr_table_name)
{
	char sql[1024] = {0};

	sprintf(sql, "LOAD DATA LOCAL INFILE '%s' into table %s", filepath, cdr_table_name);
	sqlctx->sql_cmd_type = SQLCMD_LOAD_INFILE;
	sqlee_run (sqlctx, sql);
}

void geo_db_clear(void)
{
	geo_db_delete_table(geo_cdr_tables[cdr_s1_mme_signal]->cdr_name);
	geo_db_delete_table(geo_cdr_tables[cdr_s1_emm_signal]->cdr_name);
	geo_db_delete_table(geo_cdr_tables[cdr_s1ap_handover_signal]->cdr_name);
}

void log_refill_result(struct geo_htable_key_t *hk, int cdr_index, oryx_file_t *fp)
{
	int i;
	
	fprintf(fp, "[%32s]%10s%10x%16s%16x%10s",
		geo_cdr_tables[cdr_index]->cdr_name,
		"M_TMSI: ",		hk->v,
		"MME_CODE: ",	hk->mme_code,
		"IMSI: ");
	for (i = 0; i < 18; i ++) 
		fprintf(fp, ":%02x", hk->imsi[i]);
	fprintf(fp, "%s", "\n");
	
	fflush(fp);
}
void geo_cdr_persistence_init(void)
{

	sqlee_setup ();
	sqlctx = (struct oryx_sqlctx_t *)kmalloc(sizeof(struct oryx_sqlctx_t), MPF_CLR, __oryx_unused_val__);
	sqlee_config (sqlctx, "192.168.0.148", "root", "123456", "geocdr", SQLTYPE_MYSQL);
	sqlee_init (sqlctx);
	geo_db_clear();

	if (oryx_path_exsit (cdr_stats_out_fp.fp_path))
		oryx_path_remove(cdr_stats_out_fp.fp_path);
	cdr_stats_out_fp.fp = fopen(cdr_stats_out_fp.fp_path, "wa+");
	BUG_ON(cdr_stats_out_fp.fp == NULL);

	if (oryx_path_exsit (cdr_record_error_fp.fp_path))
		oryx_path_remove(cdr_record_error_fp.fp_path);
	cdr_record_error_fp.fp = fopen(cdr_record_error_fp.fp_path, "wa+");
	BUG_ON(cdr_record_error_fp.fp == NULL);

	if (oryx_path_exsit (cdr_record_refill_result_fp.fp_path))
		oryx_path_remove(cdr_record_refill_result_fp.fp_path);
	cdr_record_refill_result_fp.fp = fopen(cdr_record_refill_result_fp.fp_path, "wa+");
	BUG_ON(cdr_record_refill_result_fp.fp == NULL);

	
}

