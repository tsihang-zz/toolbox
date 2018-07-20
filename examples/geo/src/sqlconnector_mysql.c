#include "oryx.h"
#include "sqlconnector_inc.h"

/*
 * sudo apt-get install mysql-server 
 * sudo apt-get install libmysqlclient-dev
 */
#include <mysql/mysql.h>


static __oryx_always_inline__
void mysql_dump_result (struct oryx_sqlctx_t *sql_ctx)
{
	unsigned int num_fields;
	unsigned int i;
	MYSQL_ROW row;

	num_fields = mysql_num_fields (sql_ctx->sqlresult);
	while (0 != (row = mysql_fetch_row (sql_ctx->sqlresult))) {
	    for (i=0; i<num_fields; i++)
			printf ("%s\t",row[i]);
	    printf ("\n");
	}
	
	//mysql_free_result (sql_ctx->sqlresult);
}

static int mysql_initialize (struct oryx_sqlctx_t *sql_ctx)
{
	if (unlikely (!sql_ctx) ||
		unlikely (!sql_ctx->host) || unlikely (!sql_ctx->db) ||
		unlikely (!sql_ctx->user) || unlikely (!sql_ctx->passwd)) {
		
		oryx_loge(-1,
			"sql_ctx is invalid.");
		return -1;
	}
	
	sql_ctx->sqldata = (MYSQL *)kmalloc(sizeof(MYSQL), MPF_CLR, __oryx_unused_val__);
	if (unlikely (!sql_ctx->sqldata)) {
		return -1;
	}
	
	mysql_init (sql_ctx->sqldata);

	if (!mysql_real_connect (sql_ctx->sqldata, sql_ctx->host,
		        sql_ctx->user, sql_ctx->passwd, sql_ctx->db, 0, NULL, 0)) {
		oryx_logw(-1,"Mysql Connection failed.");
		if (mysql_error(sql_ctx->sqldata)) {
		    oryx_logw(-1,"Connection error %d: %s\n", mysql_errno(sql_ctx->sqldata), mysql_error(sql_ctx->sqldata));
		}
		return -1;
	}

	oryx_logi ("Mysql Connection Success.");

	return 0;
}

void mysql_destroy (struct oryx_sqlctx_t *sql_ctx)
{
	if (sql_ctx) {
		mysql_close (sql_ctx->sqldata);
	}
	kfree (sql_ctx);
}

static int mysql_result_traversal (struct oryx_sqlctx_t *sql_ctx, void (*handler)(void *container, int argc, void **argv), void *inparam)
{
	unsigned int num_fields;
	MYSQL_ROW row;

	if (unlikely (!sql_ctx->sqlresult)) {
		oryx_loge(-1,
			"Cannot fetch result.");
		return -1;
	}

	num_fields = mysql_num_fields (sql_ctx->sqlresult);
	while (0 != (row = mysql_fetch_row (sql_ctx->sqlresult))) {
		if (handler)
			handler (inparam, num_fields, (void **)row);
	}

	mysql_free_result (sql_ctx->sqlresult);

	return 0;
	
}

static int mysql_run (struct oryx_sqlctx_t *sql_ctx, const char *sql_str)
{
	int res;

	if (unlikely (!sql_str))
		return -1;
	
	res = mysql_query (sql_ctx->sqldata, sql_str);
	if (!res) {

		if (sql_ctx->sql_cmd_type == SQLCMD_CREATAB ||
			sql_ctx->sql_cmd_type == SQLCMD_INSERT) 
			oryx_logn ("\"%s\"  success, affected rows=%lu", 
				sql_str, (unsigned long)mysql_affected_rows(sql_ctx->sqldata));
		
		if (sql_ctx->sql_cmd_type == SQLCMD_QUERY) {
			sql_ctx->sqlresult = mysql_store_result (sql_ctx->sqldata);
			if(!sql_ctx->sqlresult) {
				oryx_loge(-1,
					"\"%s\" error %d: %s\n", sql_str, mysql_errno(sql_ctx->sqldata), mysql_error(sql_ctx->sqldata));
				return -1;
			} else {
				;
			}
		}
	} else {
		oryx_loge(-1,
				"\"%s\" error %d: %s\n", sql_str, mysql_errno(sql_ctx->sqldata), mysql_error(sql_ctx->sqldata));
		return -1;
	}

	return 0;
}

void mysql_register (void)
{
	sql_engine[SQLTYPE_MYSQL].stdsql_init		= mysql_initialize;
	sql_engine[SQLTYPE_MYSQL].stdsql_destroy	= mysql_destroy;
	sql_engine[SQLTYPE_MYSQL].stdsql_run		= mysql_run;
	sql_engine[SQLTYPE_MYSQL].stdsql_traversal_result = mysql_result_traversal;
}

