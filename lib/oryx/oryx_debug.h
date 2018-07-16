#ifndef ORYX_DEBUG_H
#define ORYX_DEBUG_H

#include "debug.h"

void
oryx_log_init(void);

/**
 * Change the stream that will be used by the logging system.
 *
 * This can be done at any time. The f argument represents the stream
 * to be used to send the logs. If f is NULL, the default output is
 * used (stderr).
 *
 * @param f
 *   Pointer to the stream.
 * @return
 *   - 0 on success.
 *   - Negative on error.
 */
int oryx_openlog_stream(FILE *f);

/**
 * Set the global log level.
 *
 * After this call, logs with a level lower or equal than the level
 * passed as argument will be displayed.
 *
 * @param level
 *   Log level. A value between ORYX_LOG_EMERGENCY (1) and ORYX_LOG_DEBUG (8).
 */
void oryx_log_set_global_level(uint32_t level);

/**
 * Deprecated, replaced by oryx_log_set_global_level().
 */
void oryx_set_log_level(uint32_t level);

/**
 * Get the global log level.
 *
 * @return
 *   The current global log level.
 */
uint32_t oryx_log_get_global_level(void);

/**
 * Deprecated, replaced by oryx_log_get_global_level().
 */
uint32_t oryx_get_log_level(void);

/**
 * Enable or disable the log type.
 *
 * @param type
 *   Log type, for example, ORYX_LOGTYPE_EAL.
 * @param enable
 *   True for enable; false for disable.
 */
void oryx_set_log_type(uint32_t type, int enable);

/**
 * Get the global log type.
 */
uint32_t oryx_get_log_type(void);

/**
 * Set the log level for a given type.
 *
 * @param pattern
 *   The regexp identifying the log type.
 * @param level
 *   The level to be set.
 * @return
 *   0 on success, a negative value if level is invalid.
 */
int oryx_log_set_level_regexp(const char *pattern, uint32_t level);

/**
 * Set the log level for a given type.
 *
 * @param logtype
 *   The log type identifier.
 * @param level
 *   The level to be set.
 * @return
 *   0 on success, a negative value if logtype or level is invalid.
 */
int oryx_log_set_level(uint32_t logtype, uint32_t level);

const char *
loglevel_format(uint32_t level);

int
loglevel_unformat(const char *level_str);


/**
 * Get the current loglevel for the message being processed.
 *
 * Before calling the user-defined stream for logging, the log
 * subsystem sets a per-lcore variable containing the loglevel and the
 * logtype of the message being processed. This information can be
 * accessed by the user-defined log output function through this
 * function.
 *
 * @return
 *   The loglevel of the message being processed.
 */
int oryx_log_cur_msg_loglevel(void);

/**
 * Get the current logtype for the message being processed.
 *
 * Before calling the user-defined stream for logging, the log
 * subsystem sets a per-lcore variable containing the loglevel and the
 * logtype of the message being processed. This information can be
 * accessed by the user-defined log output function through this
 * function.
 *
 * @return
 *   The logtype of the message being processed.
 */
int oryx_log_cur_msg_logtype(void);

/**
 * Register a dynamic log type
 *
 * If a log is already registered with the same type, the returned value
 * is the same than the previous one.
 *
 * @param name
 *   The string identifying the log type.
 * @return
 *   - >0: success, the returned value is the log type identifier.
 *   - (-ENONEM): cannot allocate memory.
 */
int oryx_log_register(const char *name);

/**
 * Dump log information.
 *
 * Dump the global level and the registered log types.
 *
 * @param f
 *   The output stream where the dump should be sent.
 */
void oryx_log_dump(FILE *f);

/**
 * Generates a log message.
 *
 * The message will be sent in the stream defined by the previous call
 * to oryx_openlog_stream().
 *
 * The level argument determines if the log should be displayed or
 * not, depending on the global oryx_logs variable.
 *
 * The preferred alternative is the ORYX_LOG() because it adds the
 * level and type in the logged string.
 *
 * @param level
 *   Log level. A value between ORYX_LOG_EMERGENCY (1) and ORYX_LOG_DEBUG (8).
 * @param logtype
 *   The log type, for example, ORYX_LOGTYPE_EAL.
 * @param format
 *   The format string, as in printf(3), followed by the variable arguments
 *   required by the format.
 * @return
 *   - 0: Success.
 *   - Negative on error.
 */
int oryx_log(uint32_t level, uint32_t logtype, const char *format, ...)
#ifdef __GNUC__
#if (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ > 2))
	__attribute__((cold))
#endif
#endif
	__attribute__((format(printf, 3, 4)));

/**
 * Generates a log message.
 *
 * The message will be sent in the stream defined by the previous call
 * to oryx_openlog_stream().
 *
 * The level argument determines if the log should be displayed or
 * not, depending on the global oryx_logs variable. A trailing
 * newline may be added if needed.
 *
 * The preferred alternative is the ORYX_LOG() because it adds the
 * level and type in the logged string.
 *
 * @param level
 *   Log level. A value between ORYX_LOG_EMERGENCY (1) and ORYX_LOG_DEBUG (8).
 * @param logtype
 *   The log type, for example, ORYX_LOGTYPE_EAL.
 * @param format
 *   The format string, as in printf(3), followed by the variable arguments
 *   required by the format.
 * @param ap
 *   The va_list of the variable arguments required by the format.
 * @return
 *   - 0: Success.
 *   - Negative on error.
 */
int oryx_vlog(uint32_t level, uint32_t logtype, const char *format, va_list ap)
	__attribute__((format(printf,3,0)));

/**
 * Generates a log message.
 *
 * The ORYX_LOG() is a helper that prefixes the string with the log level
 * and type, and call oryx_log().
 *
 * @param l
 *   Log level. A value between EMERG (1) and DEBUG (8). The short name is
 *   expanded by the macro, so it cannot be an integer value.
 * @param t
 *   The log type, for example, EAL. The short name is expanded by the
 *   macro, so it cannot be an integer value.
 * @param ...
 *   The fmt string, as in printf(3), followed by the variable arguments
 *   required by the format.
 * @return
 *   - 0: Success.
 *   - Negative on error.
 */
#define ORYX_LOG(l, t, ...)					\
	 oryx_log(ORYX_LOG_ ## l,					\
		 ORYX_LOGTYPE_ ## t, # t ": " __VA_ARGS__)

/**
 * Generates a log message for data path.
 *
 * Similar to ORYX_LOG(), except that it is removed at compilation time
 * if the ORYX_LOG_DP_LEVEL configuration option is lower than the log
 * level argument.
 *
 * @param l
 *   Log level. A value between EMERG (1) and DEBUG (8). The short name is
 *   expanded by the macro, so it cannot be an integer value.
 * @param t
 *   The log type, for example, EAL. The short name is expanded by the
 *   macro, so it cannot be an integer value.
 * @param ...
 *   The fmt string, as in printf(3), followed by the variable arguments
 *   required by the format.
 * @return
 *   - 0: Success.
 *   - Negative on error.
 */
#define ORYX_LOG_DP(l, t, ...)					\
	(void)((ORYX_LOG_ ## l <= ORYX_LOG_DP_LEVEL) ?		\
	 oryx_log(ORYX_LOG_ ## l,					\
		 ORYX_LOGTYPE_ ## t, # t ": " __VA_ARGS__) :	\
	 0)

extern uint32_t oryx_log_global_log_level;

#define __oryx_log__(x, file, func, line, ...)                                          	\
	    do {                                                                        \
	        if (oryx_log_global_log_level >= x)                                 		\
	        {                                                                       \
	            char _oryx_log_msg[ORYX_LOG_MAX_LOG_MSG_LEN];                           \
	                                                                                \
	            int _oryx_log_ret = snprintf(_oryx_log_msg, ORYX_LOG_MAX_LOG_MSG_LEN, __VA_ARGS__);   \
	            if (_oryx_log_ret == ORYX_LOG_MAX_LOG_MSG_LEN)                          \
	                _oryx_log_msg[ORYX_LOG_MAX_LOG_MSG_LEN - 1] = '\0';                 \
	                                                                                \
	            oryx_logging_out(x, file, line, func, 0, _oryx_log_msg);              \
	        }                                                                       \
	    } while(0)

#define __oryx_loge__(x, file, func, line, err, ...)                                     \
		do {																		\
			if (oryx_log_global_log_level >= x)										\
			{																		\
				char _oryx_log_msg[ORYX_LOG_MAX_LOG_MSG_LEN];							\
																					\
				int _oryx_log_ret = snprintf(_oryx_log_msg, ORYX_LOG_MAX_LOG_MSG_LEN, __VA_ARGS__);	\
				if (_oryx_log_ret == ORYX_LOG_MAX_LOG_MSG_LEN)							\
					_oryx_log_msg[ORYX_LOG_MAX_LOG_MSG_LEN - 1] = '\0'; 				\
																					\
				oryx_logging_out(x, file, line, func, err, _oryx_log_msg);			\
			}																		\
		} while(0)

#if defined(BUILD_DEBUG)
/**
 * \brief Macro used to log DEBUG messages. Comes under the debugging subsystem,
 *		  and hence will be enabled only in the presence of the DEBUG macro.
 *
 * \retval ... Takes as argument(s), a printf style format message
 */
#define oryx_logd(...)       __oryx_log__(ORYX_LOG_DEBUG, \
		__FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)

/**
 * \brief Macro used to log INFORMATIONAL messages.
 *
 * \retval ... Takes as argument(s), a printf style format message
 */
#define oryx_logi(...) __oryx_log__(ORYX_LOG_INFO, \
		__FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)
					
/**
 * \brief Macro used to log NOTICE messages.
 *
 * \retval ... Takes as argument(s), a printf style format message
 */
#define oryx_logn(...) __oryx_log__(ORYX_LOG_NOTICE, \
        __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)

/**
 * \brief Macro used to log WARNING messages.
 *
 * \retval err_code Error code that has to be logged along with the
 *                  warning message
 * \retval ...      Takes as argument(s), a printf style format message
 */
#define oryx_logw(err_code, ...) __oryx_loge__(ORYX_LOG_WARNING, \
        __FILE__, __FUNCTION__, __LINE__, \
        err_code, __VA_ARGS__)

/**
 * \brief Macro used to log ERROR messages.
 *
 * \retval err_code Error code that has to be logged along with the
 *                  error message
 * \retval ...      Takes as argument(s), a printf style format message
 */
#define oryx_loge(err_code, ...) __oryx_loge__(ORYX_LOG_ERROR, \
        __FILE__, __FUNCTION__, __LINE__, \
        err_code, __VA_ARGS__)

/**
 * \brief Macro used to log CRITICAL messages.
 *
 * \retval err_code Error code that has to be logged along with the
 *                  critical message
 * \retval ...      Takes as argument(s), a printf style format message
 */
#define oryx_logc(err_code, ...) __oryx_loge__(ORYX_LOG_CRITICAL, \
        __FILE__, __FUNCTION__, __LINE__, \
        err_code, __VA_ARGS__)

/**
 * \brief Macro used to log ALERT messages.
 *
 * \retval err_code Error code that has to be logged along with the
 *                  alert message
 * \retval ...      Takes as argument(s), a printf style format message
 */
#define oryx_loga(err_code, ...) __oryx_loge__(ORYX_LOG_ALERT, \
        __FILE__, __FUNCTION__, __LINE__, \
        err_code, __VA_ARGS__)

/**
 * \brief Macro used to log EMERGENCY messages.
 *
 * \retval err_code Error code that has to be logged along with the
 *                  emergency message
 * \retval ...      Takes as argument(s), a printf style format message
 */
#define oryx_logE(err_code, ...) __oryx_loge__(ORYX_LOG_EMERGENCY, \
        __FILE__, __FUNCTION__, __LINE__, \
        err_code, __VA_ARGS__)
#else
/**
 * \brief Macro used to log DEBUG messages. Comes under the debugging subsystem,
 *		  and hence will be enabled only in the presence of the DEBUG macro.
 *
 * \retval ... Takes as argument(s), a printf style format message
 */
#define oryx_logd(...)

/**
 * \brief Macro used to log INFORMATIONAL messages.
 *
 * \retval ... Takes as argument(s), a printf style format message
 */
#define oryx_logi(...)
					
/**
 * \brief Macro used to log NOTICE messages.
 *
 * \retval ... Takes as argument(s), a printf style format message
 */
#define oryx_logn(...) __oryx_log__(ORYX_LOG_NOTICE, \
        __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)

/**
 * \brief Macro used to log WARNING messages.
 *
 * \retval err_code Error code that has to be logged along with the
 *                  warning message
 * \retval ...      Takes as argument(s), a printf style format message
 */
#define oryx_logw(err_code, ...) __oryx_loge__(ORYX_LOG_WARNING, \
        __FILE__, __FUNCTION__, __LINE__, \
        err_code, __VA_ARGS__)

/**
 * \brief Macro used to log ERROR messages.
 *
 * \retval err_code Error code that has to be logged along with the
 *                  error message
 * \retval ...      Takes as argument(s), a printf style format message
 */
#define oryx_loge(err_code, ...) __oryx_loge__(ORYX_LOG_ERROR, \
        __FILE__, __FUNCTION__, __LINE__, \
        err_code, __VA_ARGS__)

/**
 * \brief Macro used to log CRITICAL messages.
 *
 * \retval err_code Error code that has to be logged along with the
 *                  critical message
 * \retval ...      Takes as argument(s), a printf style format message
 */
#define oryx_logc(err_code, ...) __oryx_loge__(ORYX_LOG_CRITICAL, \
        __FILE__, __FUNCTION__, __LINE__, \
        err_code, __VA_ARGS__)

/**
 * \brief Macro used to log ALERT messages.
 *
 * \retval err_code Error code that has to be logged along with the
 *                  alert message
 * \retval ...      Takes as argument(s), a printf style format message
 */
#define oryx_loga(err_code, ...) __oryx_loge__(ORYX_LOG_ALERT, \
        __FILE__, __FUNCTION__, __LINE__, \
        err_code, __VA_ARGS__)

/**
 * \brief Macro used to log EMERGENCY messages.
 *
 * \retval err_code Error code that has to be logged along with the
 *                  emergency message
 * \retval ...      Takes as argument(s), a printf style format message
 */
#define oryx_logE(err_code, ...) __oryx_loge__(ORYX_LOG_EMERGENCY, \
        __FILE__, __FUNCTION__, __LINE__, \
        err_code, __VA_ARGS__)


#endif
void
oryx_panic(int exit_code, const char *format, ...);

static __oryx_always_inline__
const char * oryx_safe_strerror(int errnum)
{
  const char *s = strerror(errnum);
  return (s != NULL) ? s : "Unknown error";
}

#endif
