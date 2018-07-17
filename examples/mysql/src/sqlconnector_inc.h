#ifndef __ORYX_SQLCONNECTOR_INC_H__
#define __ORYX_SQLCONNECTOR_INC_H__


#define CONNECTOR_ENA_MYSQL

enum {
	SQLTYPE_NOTSET = 0,
	SQLTYPE_MYSQL,
	SQLTYPE_ORACLE,
	/* table size */
	SQLTYPE_TABLE_SIZE,
};

enum {
	SQLCMD_NOTSET = 0,
	SQLCMD_CREATAB,
	SQLCMD_QUERY,
	SQLCMD_INSERT,
	SQLCMD_DELETE,
	SQLCMD_LOAD_INFILE,
	SQLCMD_COMMIT,

};

struct oryx_sqlctx_t {

	const char	*host;
	const char	*user;
	const char	*passwd;
	const char	*db;
	
	int flags;

	void *sqldata;
	void *sqlresult;
	
	uint16_t sql_db_type;
	uint16_t sql_cmd_type;
};

struct oryx_sqlengine_t {
	void *sqlctx;

	int (*stdsql_init)(struct oryx_sqlctx_t * sql_ctx);
	void (*stdsql_destroy)(struct oryx_sqlctx_t * sql_ctx);
	int (*stdsql_run)(struct oryx_sqlctx_t * sql_ctx, const char *sql_str);
	int (*stdsql_traversal_result) (struct oryx_sqlctx_t *sql_ctx, void (*handler)(void *container, int argc, void **argv), void *inparam);
};

extern struct oryx_sqlengine_t sql_engine[];

#endif

