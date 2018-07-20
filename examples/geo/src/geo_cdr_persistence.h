#ifndef GEO_CDR_PERSISTENCE
#define GEO_CDR_PERSISTENCE

struct sql_load_data_t {
	int length;
	int step;
	int elem;
	char *buf;
	const char *md5;
	char *file;
};

extern struct geo_log_file_t cdr_stats_out_fp;
extern struct geo_log_file_t cdr_record_error_fp;
extern struct geo_log_file_t cdr_record_refill_result_fp;
extern struct oryx_sqlctx_t *sqlctx;;

extern void geo_cdr_persistence_init(void);
extern void log_refill_result(struct geo_htable_key_t *hk, int cdr_index, oryx_file_t *fp);
extern void geo_db_delete_table(const char *cdr_table_name);
extern void geo_db_load_table(const char *filepath, const char *cdr_table_name);

#endif

