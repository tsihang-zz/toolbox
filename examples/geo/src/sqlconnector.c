
#include "oryx.h"
#include "sqlconnector_inc.h"

struct oryx_sqlengine_t sql_engine[SQLTYPE_TABLE_SIZE];

const char *sqltest_str = "\
		--\
		-- Create the table children\
		--\
		CREATE TABLE children (\
		    childno int(11) NOT NULL auto_increment,\
		    fname varchar(30),\
		    age int(11),\
		    PRIMARY KEY (childno)\
		);\
		--\
		-- Populate the table 'children'\
		--\
		INSERT INTO children(childno, fname, age) VALUES (1, 'Jenny', 21);\
		INSERT INTO children(childno, fname, age) VALUES (2, 'Andrew', 17);\
		INSERT INTO children(childno, fname, age) VALUES (3, 'Gavin', 8);\
		INSERT INTO children(childno, fname, age) VALUES (4, 'Duncan', 6);\
		INSERT INTO children(childno, fname, age) VALUES (5, 'Emma', 4);\
		INSERT INTO children(childno, fname, age) VALUES (6, 'Alex', 15);\
		INSERT INTO children(childno, fname, age) VALUES (7, 'Adrian', 9);\
";

void sqlee_config (struct oryx_sqlctx_t * sql_ctx,
				const char *host,
				const char *user,
				const char *pwd,
				const char *db,
				const uint16_t sql_type)
{	
	sql_ctx->db		=  (db);
	sql_ctx->host	=  (host);
	sql_ctx->user	=  (user);
	sql_ctx->passwd	=  (pwd);
	sql_ctx->sql_db_type	= sql_type;

}

int sqlee_init (struct oryx_sqlctx_t *sql_ctx)
{
	return sql_engine[sql_ctx->sql_db_type].stdsql_init (sql_ctx);
}

void sqlee_destroy (struct oryx_sqlctx_t *sql_ctx)
{
	sql_engine[sql_ctx->sql_db_type].stdsql_destroy (sql_ctx);
}

int sqlee_run (struct oryx_sqlctx_t *sql_ctx, const char *sql_str)
{
	return sql_engine[sql_ctx->sql_db_type].stdsql_run(sql_ctx, sql_str);
}

int sqlee_result_traversal (struct oryx_sqlctx_t *sql_ctx, 
			void (*handler)(void *container, int argc, void **argv), void *inparam)
{
	return sql_engine[sql_ctx->sql_db_type].stdsql_traversal_result(sql_ctx, handler, inparam);
}

void sqlee_setup (void)
{
    memset(sql_engine, 0, sizeof(sql_engine));

    mysql_register();
}

