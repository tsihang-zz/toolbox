#ifndef ORYX_DEBUG_H
#define ORYX_DEBUG_H

struct oryx_log_dynamic_type {
	const char *name;
	uint32_t loglevel;
};

/** The rte_log structure. */
struct oryx_logs {
	uint32_t type;  /**< Bitfield with enabled logs. */
	uint32_t level; /**< Log level. */
	FILE *file;     /**< Output file set by rte_openlog_stream, or NULL. */
	size_t dynamic_types_len;
	struct oryx_log_dynamic_type *dynamic_types;
};


#endif
