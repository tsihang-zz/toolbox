#include "oryx.h"
#include "geo_cdr_table.h"

struct geo_cdr_table_t cdr_s1_mme = {
	.lf = {
		.fp_path	=	"/home/tsihang/data/cdr_s1mme.txt",
		.fp_comment =	"valid cdr record out logging.",
		.fp 		=	NULL,
	},
	.cdr_index = cdr_s1_mme_signal,
	.table_id = 0,
	.cdr_name = "s1_mme_signal",
	.length	= sizeof(full_record_s1_mme_signal_t),
};

struct geo_cdr_table_t cdr_s1_emm = {
	.lf = {
		.fp_path	=	"/home/tsihang/data/cdr_s1emm.txt",
		.fp_comment =	"valid cdr record out logging.",
		.fp 		=	NULL,
	},
	.cdr_index = cdr_s1_emm_signal,
	.table_id = 0,
	.cdr_name = "s1_emm_signal",
	.length	= sizeof(full_record_s1_emm_signal_t),
};

struct geo_cdr_table_t cdr_s1ap_handover = {
	.lf = {
		.fp_path	=	"/home/tsihang/data/cdr_s1ap_handover.txt",
		.fp_comment =	"valid cdr record out logging.",
		.fp 		=	NULL,
	},
	.cdr_index = cdr_s1ap_handover_signal,
	.table_id = 0,
	.cdr_name = "s1ap_handover_signal",
	.length	= sizeof(full_record_s1ap_handover_signal_t),
};

