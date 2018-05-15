#ifndef DEBUG_H
#define DEBUG_H

struct oryx_log_dynamic_type {
	const char *name;
	uint32_t loglevel;
};

/** The rte_log structure. */
struct oryx_logs_t {
	uint32_t type;  /**< Bitfield with enabled logs. */
	uint32_t level; /**< Log level. */
	FILE *file;     /**< Output file set by rte_openlog_stream, or NULL. */
	size_t dynamic_types_len;
	struct oryx_log_dynamic_type *dynamic_types;
};

/** Global log informations */
extern struct oryx_logs_t oryx_logs;

/* SDK log type */
#define ORYX_LOGTYPE_EAL        0 /**< Log related to eal. */
#define ORYX_LOGTYPE_MALLOC     1 /**< Log related to malloc. */
#define ORYX_LOGTYPE_RING       2 /**< Log related to ring. */
#define ORYX_LOGTYPE_MEMPOOL    3 /**< Log related to mempool. */
#define ORYX_LOGTYPE_TIMER      4 /**< Log related to timers. */
#define ORYX_LOGTYPE_PMD        5 /**< Log related to poll mode driver. */
#define ORYX_LOGTYPE_HASH       6 /**< Log related to hash table. */
#define ORYX_LOGTYPE_LPM        7 /**< Log related to LPM. */
#define ORYX_LOGTYPE_KNI        8 /**< Log related to KNI. */
#define ORYX_LOGTYPE_ACL        9 /**< Log related to ACL. */
#define ORYX_LOGTYPE_POWER     10 /**< Log related to power. */
#define ORYX_LOGTYPE_METER     11 /**< Log related to QoS meter. */
#define ORYX_LOGTYPE_SCHED     12 /**< Log related to QoS port scheduler. */
#define ORYX_LOGTYPE_PORT      13 /**< Log related to port. */
#define ORYX_LOGTYPE_TABLE     14 /**< Log related to table. */
#define ORYX_LOGTYPE_PIPELINE  15 /**< Log related to pipeline. */
#define ORYX_LOGTYPE_MBUF      16 /**< Log related to mbuf. */
#define ORYX_LOGTYPE_CRYPTODEV 17 /**< Log related to cryptodev. */
#define ORYX_LOGTYPE_EFD       18 /**< Log related to EFD. */
#define ORYX_LOGTYPE_EVENTDEV  19 /**< Log related to eventdev. */

/* these log types can be used in an application */
#define ORYX_LOGTYPE_USER1     24 /**< User-defined log type 1. */
#define ORYX_LOGTYPE_USER2     25 /**< User-defined log type 2. */
#define ORYX_LOGTYPE_USER3     26 /**< User-defined log type 3. */
#define ORYX_LOGTYPE_USER4     27 /**< User-defined log type 4. */
#define ORYX_LOGTYPE_USER5     28 /**< User-defined log type 5. */
#define ORYX_LOGTYPE_USER6     29 /**< User-defined log type 6. */
#define ORYX_LOGTYPE_USER7     30 /**< User-defined log type 7. */
#define ORYX_LOGTYPE_USER8     31 /**< User-defined log type 8. */

/** First identifier for extended logs */
#define ORYX_LOGTYPE_FIRST_EXT_ID 128


/* Can't use 0, as it gives compiler warnings */
#define ORYX_LOG_EMERGENCY 1U  /**< System is unusable.               */
#define ORYX_LOG_ALERT     2U  /**< Action must be taken immediately. */
#define ORYX_LOG_CRITICAL  3U  /**< Critical conditions.              */
#define ORYX_LOG_ERROR     4U  /**< Error conditions.                 */
#define ORYX_LOG_WARNING   5U  /**< Warning conditions.               */
#define ORYX_LOG_NOTICE    6U  /**< Normal but significant condition. */
#define ORYX_LOG_INFO      7U  /**< Informational.                    */
#define ORYX_LOG_DEBUG     8U  /**< Debug-level messages.             */

/* The different log format specifiers supported by the API */
#define SC_LOG_FMT_TIME             't' /* Timestamp in standard format */
#define SC_LOG_FMT_PID              'p' /* PID */
#define SC_LOG_FMT_TID              'i' /* Thread ID */
#define SC_LOG_FMT_TM               'm' /* Thread module name */
#define SC_LOG_FMT_LOG_LEVEL        'd' /* Log level */
#define SC_LOG_FMT_FILE_NAME        'f' /* File name */
#define SC_LOG_FMT_LINE             'l' /* Line number */
#define SC_LOG_FMT_FUNCTION         'n' /* Function */

/* The log format prefix for the format specifiers */
#define SC_LOG_FMT_PREFIX           '%'

/**
 * \brief The various output interfaces supported
 */
typedef enum {
    SC_LOG_OP_IFACE_CONSOLE,
    SC_LOG_OP_IFACE_FILE,
    SC_LOG_OP_IFACE_SYSLOG,
    SC_LOG_OP_IFACE_MAX,
} oryx_logopt_iface;

typedef enum {
    SC_LOG_OP_TYPE_REGULAR = 0,
    SC_LOG_OP_TYPE_JSON,
} oryx_logopt_type;

/* The default log_format, if it is not supplied by the user */
#ifdef RELEASE
#define SC_LOG_DEF_LOG_FORMAT "%t - <%d> - "
#else
#define SC_LOG_DEF_LOG_FORMAT "[%i] %t - (%f:%l) <%d> (%n) -- "
#endif

/* The maximum length of the log message */
#define SC_LOG_MAX_LOG_MSG_LEN 2048

/* The maximum length of the log format */
#define SC_LOG_MAX_LOG_FORMAT_LEN 128

/* The default log level, if it is not supplied by the user */
#define SC_LOG_DEF_LOG_LEVEL ORYX_LOG_INFO

/* The default output interface to be used */
#define SC_LOG_DEF_LOG_OP_IFACE SC_LOG_OP_IFACE_CONSOLE

void oryx_logging_out(const int log_level, const char *file,
                     const unsigned int line, const char *function,
                     const int error_code, const char *message);


#endif
