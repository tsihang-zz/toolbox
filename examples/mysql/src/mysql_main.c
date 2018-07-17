#include "oryx.h"


#include "sqlconnector.h"

int main (
	int		__oryx_unused_param__	argc,
	char	__oryx_unused_param__	** argv
)
{
	uint32_t val_start, val_end;
	int ret;

	oryx_initialize();
	
	struct oryx_sqlctx_t *sqlctx = (struct oryx_sqlctx_t *)kmalloc(sizeof(struct oryx_sqlctx_t), MPF_CLR, __oryx_unused_val__);

	sqlee_setup ();
	sqlee_config (sqlctx, "192.168.0.148", "root", "123456", "geocdr", SQLTYPE_MYSQL);
	sqlee_init (sqlctx);

#if 0	
	sqlctx->sql_cmd_type = SQLCMD_CREATAB;
	sqlee_run (sqlctx, "CREATE TABLE s1_test_signal (id int not null, m_tmsi int null, mme_code int null, mme_ue_s1ap_id int null, imsi VARCHAR(18) null)");
#endif	
	sqlctx->sql_cmd_type = SQLCMD_INSERT;
	sqlee_run (sqlctx, "INSERT INTO s1_test_signal (id, m_tmsi, mme_code, mme_ue_s1ap_id, imsi) VALUES(1, 24046, 0, 1234567, '3236323')");
	
	sqlctx->sql_cmd_type = SQLCMD_QUERY;
	sqlee_run (sqlctx, "SELECT * FROM s1_test_signal");
	
	sqlee_destroy(sqlctx);


	oryx_task_launch();
	
	FOREVER;
	
	return 0;
}

