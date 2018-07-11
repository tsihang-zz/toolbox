#ifndef __ORYX_SQLCONNECTOR_H__
#define __ORYX_SQLCONNECTOR_H__

#include "sqlconnector_inc.h"
#include "sqlconnector_mysql.h"


extern int sqlee_init (struct oryx_sqlctx_t *sql_ctx);
extern void sqlee_config (struct oryx_sqlctx_t * sql_ctx,
				const char *host,
				const char *user,
				const char *pwd,
				const char *db,
				const uint16_t sql_type);
extern void sqlee_destroy (struct oryx_sqlctx_t *sql_ctx);
extern int sqlee_run (struct oryx_sqlctx_t *sql_ctx, const char *sql_str);
extern int sqlee_result_traversal (struct oryx_sqlctx_t *sql_ctx, 
			void (*handler)(void *container, int argc, void **argv), void *inparam);

extern void sqlee_setup (void);

#endif

