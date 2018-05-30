#include "oryx.h"
#include "debug.h"

#ifndef ORYX_LOG_LEVEL
#define ORYX_LOG_LEVEL	ORYX_LOG_DEBUG
#endif

u32 oryx_log_global_log_level = ORYX_LOG_DEBUG;

/** Get the Current Thread Id */
#ifdef OS_FREEBSD
#include <pthread_np.h>

#define SCGetThreadIdLong(...) ({ \
    long tmpthid; \
    thr_self(&tmpthid); \
    u_long _scgetthread_tid = (u_long)tmpthid; \
    _scgetthread_tid; \
})
#elif defined(__OpenBSD__)
#define SCGetThreadIdLong(...) ({ \
    pid_t tpid; \
    tpid = getpid(); \
    u_long _scgetthread_tid = (u_long)tpid; \
    _scgetthread_tid; \
})
#elif defined(__CYGWIN__)
#define SCGetThreadIdLong(...) ({ \
    u_long _scgetthread_tid = (u_long)GetCurrentThreadId(); \
	_scgetthread_tid; \
})
#elif defined(OS_WIN32)
#define SCGetThreadIdLong(...) ({ \
    u_long _scgetthread_tid = (u_long)GetCurrentThreadId(); \
	_scgetthread_tid; \
})
#elif defined(OS_DARWIN)
#define SCGetThreadIdLong(...) ({ \
    thread_port_t tpid; \
    tpid = mach_thread_self(); \
    u_long _scgetthread_tid = (u_long)tpid; \
    _scgetthread_tid; \
})
#elif defined(sun)
#include <thread.h>
#define SCGetThreadIdLong(...) ({ \
    thread_t tmpthid = thr_self(); \
    u_long _scgetthread_tid = (u_long)tmpthid; \
    _scgetthread_tid; \
})

#else
#define SCGetThreadIdLong(...) ({ \
   pid_t tmpthid; \
   tmpthid = syscall(SYS_gettid); \
   u_long _scgetthread_tid = (u_long)tmpthid; \
   _scgetthread_tid; \
})
#endif /* OS FREEBSD */

/** Number of elements in the array. */
#define	ORYX_DIM(a)	(sizeof (a) / sizeof ((a)[0]))

/* global log structure */
struct oryx_logs_t oryx_logs = {
	.type = ~0,
	.level = ORYX_LOG_DEBUG,
	.file = NULL,
};

/* Stream to use for logging if oryx_logs.file is NULL */
static FILE *default_log_stream;

/**
 * This global structure stores some informations about the message
 * that is currently being processed by one lcore
 */
struct log_cur_msg {
	uint32_t loglevel; /**< log level - see oryx_log.h */
	uint32_t logtype;  /**< log type  - see oryx_log.h */
};

/* default logs */

/* Change the stream that will be used by logging system */
int
oryx_openlog_stream(FILE *f)
{
	oryx_logs.file = f;
	return 0;
}

/* Set global log level */
void
oryx_log_set_global_level(uint32_t level)
{
	oryx_logs.level = (uint32_t)level;
}

/* Set global log level */
/* replaced by oryx_log_set_global_level */
void
oryx_set_log_level(uint32_t level)
{
	oryx_log_set_global_level(level);
}

/* Get global log level */
uint32_t
oryx_log_get_global_level(void)
{
	return oryx_logs.level;
}

/* Get global log level */
/* replaced by oryx_log_get_global_level */
uint32_t
oryx_get_log_level(void)
{
	return oryx_log_get_global_level();
}

/* Set global log type */
void
oryx_set_log_type(uint32_t type, int enable)
{
	if (type < ORYX_LOGTYPE_FIRST_EXT_ID) {
		if (enable)
			oryx_logs.type |= 1 << type;
		else
			oryx_logs.type &= ~(1 << type);
	}

	if (enable)
		oryx_log_set_level(type, 0);
	else
		oryx_log_set_level(type, ORYX_LOG_DEBUG);
}

/* Get global log type */
uint32_t
oryx_get_log_type(void)
{
	return oryx_logs.type;
}

int
oryx_log_set_level(uint32_t type, uint32_t level)
{
	if (type >= oryx_logs.dynamic_types_len)
		return -1;
	if (level > ORYX_LOG_DEBUG)
		return -1;

	oryx_logs.dynamic_types[type].loglevel = level;

	return 0;
}

/* set level */
int
oryx_log_set_level_regexp(const char *pattern, uint32_t level)
{
	regex_t r;
	size_t i;

	if (level > ORYX_LOG_DEBUG)
		return -1;

	if (regcomp(&r, pattern, 0) != 0)
		return -1;

	for (i = 0; i < oryx_logs.dynamic_types_len; i++) {
		if (oryx_logs.dynamic_types[i].name == NULL)
			continue;
		if (regexec(&r, oryx_logs.dynamic_types[i].name, 0,
				NULL, 0) == 0)
			oryx_logs.dynamic_types[i].loglevel = level;
	}

	return 0;
}

static int
oryx_log_lookup(const char *name)
{
	size_t i;

	for (i = 0; i < oryx_logs.dynamic_types_len; i++) {
		if (oryx_logs.dynamic_types[i].name == NULL)
			continue;
		if (strcmp(name, oryx_logs.dynamic_types[i].name) == 0)
			return i;
	}

	return -1;
}

/* register an extended log type, assuming table is large enough, and id
 * is not yet registered.
 */
static int
__oryx_log_register(const char *name, int id)
{
	char *dup_name = strdup(name);

	if (dup_name == NULL)
		return -ENOMEM;

	oryx_logs.dynamic_types[id].name = dup_name;
	oryx_logs.dynamic_types[id].loglevel = ORYX_LOG_DEBUG;

	return id;
}

/* register an extended log type */
int
oryx_log_register(const char *name)
{
	struct oryx_log_dynamic_type *new_dynamic_types;
	int id, ret;

	id = oryx_log_lookup(name);
	if (id >= 0)
		return id;

	new_dynamic_types = realloc(oryx_logs.dynamic_types,
		sizeof(struct oryx_log_dynamic_type) *
		(oryx_logs.dynamic_types_len + 1));
	if (new_dynamic_types == NULL)
		return -ENOMEM;
	oryx_logs.dynamic_types = new_dynamic_types;

	ret = __oryx_log_register(name, oryx_logs.dynamic_types_len);
	if (ret < 0)
		return ret;

	oryx_logs.dynamic_types_len++;

	return ret;
}

struct logtype {
	uint32_t log_id;
	const char *logtype;
};

static const struct logtype logtype_strings[] = {
	{ORYX_LOGTYPE_EAL,        "eal"},
	{ORYX_LOGTYPE_MALLOC,     "malloc"},
	{ORYX_LOGTYPE_RING,       "ring"},
	{ORYX_LOGTYPE_MEMPOOL,    "mempool"},
	{ORYX_LOGTYPE_TIMER,      "timer"},
	{ORYX_LOGTYPE_PMD,        "pmd"},
	{ORYX_LOGTYPE_HASH,       "hash"},
	{ORYX_LOGTYPE_LPM,        "lpm"},
	{ORYX_LOGTYPE_KNI,        "kni"},
	{ORYX_LOGTYPE_ACL,        "acl"},
	{ORYX_LOGTYPE_POWER,      "power"},
	{ORYX_LOGTYPE_METER,      "meter"},
	{ORYX_LOGTYPE_SCHED,      "sched"},
	{ORYX_LOGTYPE_PORT,       "port"},
	{ORYX_LOGTYPE_TABLE,      "table"},
	{ORYX_LOGTYPE_PIPELINE,   "pipeline"},
	{ORYX_LOGTYPE_MBUF,       "mbuf"},
	{ORYX_LOGTYPE_CRYPTODEV,  "cryptodev"},
	{ORYX_LOGTYPE_EFD,        "efd"},
	{ORYX_LOGTYPE_EVENTDEV,   "eventdev"},
	{ORYX_LOGTYPE_USER1,      "user1"},
	{ORYX_LOGTYPE_USER2,      "user2"},
	{ORYX_LOGTYPE_USER3,      "user3"},
	{ORYX_LOGTYPE_USER4,      "user4"},
	{ORYX_LOGTYPE_USER5,      "user5"},
	{ORYX_LOGTYPE_USER6,      "user6"},
	{ORYX_LOGTYPE_USER7,      "user7"},
	{ORYX_LOGTYPE_USER8,      "user8"}
};

void
oryx_log_init(void)
{
	uint32_t i;

#if ORYX_LOG_LEVEL >= ORYX_LOG_DEBUG
	oryx_log_set_global_level(ORYX_LOG_INFO);
#else
	oryx_log_set_global_level(ORYX_LOG_LEVEL);
#endif

	oryx_logs.dynamic_types = calloc(ORYX_LOGTYPE_FIRST_EXT_ID,
		sizeof(struct oryx_log_dynamic_type));
	if (oryx_logs.dynamic_types == NULL)
		return;

	/* register legacy log types */
	for (i = 0; i < ORYX_DIM(logtype_strings); i++)
		__oryx_log_register(logtype_strings[i].logtype,
				logtype_strings[i].log_id);

	oryx_logs.dynamic_types_len = ORYX_LOGTYPE_FIRST_EXT_ID;
}

const char *
loglevel_format(uint32_t level)
{
	switch (level) {
	case 0: return "disabled";
	case ORYX_LOG_EMERGENCY: return "emerg";
	case ORYX_LOG_ALERT: return "alert";
	case ORYX_LOG_CRITICAL: return "critical";
	case ORYX_LOG_ERROR: return "error";
	case ORYX_LOG_WARNING: return "warning";
	case ORYX_LOG_NOTICE: return "notice";
	case ORYX_LOG_INFO: return "info";
	case ORYX_LOG_DEBUG: return "debug";
	default: return "unknown";
	}
}

int
loglevel_unformat(const char *level_str)
{
	if (!strcmp(level_str, "debug"))
		return ORYX_LOG_DEBUG;
	if (!strcmp(level_str, "info"))
		return ORYX_LOG_INFO;
	if (!strcmp(level_str, "notice"))
		return ORYX_LOG_NOTICE;
	if (!strcmp(level_str, "warning"))
		return ORYX_LOG_WARNING;
	if (!strcmp(level_str, "error"))
		return ORYX_LOG_ERROR;
	if (!strcmp(level_str, "critical"))
		return ORYX_LOG_CRITICAL;
	if (!strcmp(level_str, "alert"))
		return ORYX_LOG_ALERT;
	if (!strcmp(level_str, "emerg"))
		return ORYX_LOG_EMERGENCY;

	return UINT_MAX;
}


/* dump global level and registered log types */
void
oryx_log_dump(FILE *f)
{
	size_t i;
	struct oryx_log_dynamic_type *dt;
	
	fprintf(f, "global log level is %s\n",
		loglevel_format(oryx_log_get_global_level()));

	for (i = 0; i < oryx_logs.dynamic_types_len; i++) {
		dt = &oryx_logs.dynamic_types[i];
		if (dt->name == NULL)
			continue;
		fprintf(f, "id %zu: %s, level is %s\n",
			i, dt->name,
			loglevel_format(dt->loglevel));
	}
}

/*
 * Generates a log message The message will be sent in the stream
 * defined by the previous call to oryx_openlog_stream().
 */
int
oryx_vlog(uint32_t level, uint32_t logtype, const char *format, va_list ap)
{
	int ret;
	FILE *f = oryx_logs.file;
	if (f == NULL) {
		f = default_log_stream;
		if (f == NULL) {
			/*
			 * Grab the current value of stderr here, rather than
			 * just initializing default_log_stream to stderr. This
			 * ensures that we will always use the current value
			 * of stderr, even if the application closes and
			 * reopens it.
			 */
			f = stderr;
		}
	}

	if (level > oryx_logs.level)
		return 0;
	if (logtype >= oryx_logs.dynamic_types_len)
		return -1;
	if (level > oryx_logs.dynamic_types[logtype].loglevel)
		return 0;

	ret = vfprintf(f, format, ap);
	fflush(f);
	return ret;
}

/*
 * Generates a log message The message will be sent in the stream
 * defined by the previous call to oryx_openlog_stream().
 * No need to check level here, done by oryx_vlog().
 */
int
oryx_log(uint32_t level, uint32_t logtype, const char *format, ...)
{
	va_list ap;
	int ret;

	va_start(ap, format);
	ret = oryx_vlog(level, logtype, format, ap);
	va_end(ap);
	return ret;
}

/*
 * Called by environment-specific initialization functions.
 */
void
oryx_log_set_default(FILE *default_log)
{
	default_log_stream = default_log;
}

/**
 * \brief Output function that logs a character string out to a file descriptor
 *
 * \param fd  Pointer to the file descriptor
 * \param msg Pointer to the character string that should be logged
 */
static __oryx_always_inline__
void oryx_log2_stream(FILE *fd, char *msg)
{
    /* Would only happen if the log file failed to re-open during rotation. */
    if (fd == NULL) {
        return;
    }

    if (fprintf(fd, "%s\n", msg) < 0)
        printf("Error writing to stream using fprintf\n");

    fflush(fd);

    return;
}

static const
char *errno2_string(int __oryx_unused__ err)
{
	return str(err);
}

/**
 * \brief Adds the global log_format to the outgoing buffer
 *
 * \param log_level log_level of the message that has to be logged
 * \param msg       Buffer containing the outgoing message
 * \param file      File_name from where the message originated
 * \param function  Function_name from where the message originated
 * \param line      Line_no from where the messaged originated
 *
 * \retval 0 on success; else an error code
 */
static int oryx_log2_buffer(
        struct timeval *tval, int color, oryx_logopt_type __oryx_unused__ type,
                     char *buffer, size_t __oryx_unused__ buffer_size,
                     const char *log_format,
                     const uint32_t log_level, const char *file,
                     const unsigned int line, const char *function,
                     const int error_code, const char *message)
{
#ifdef HAVE_LIBJANSSON
    if (type == SC_LOG_OP_TYPE_JSON)
        return SCLogMessageJSON(tval, buffer, buffer_size, log_level, file, line, function, error_code, message);
#endif

    char *temp = buffer;
    const char *s = NULL;
    struct tm *tms = NULL;

    const char *redb = "";
    const char *red = "";
    const char *yellowb = "";
    const char *yellow = "";
    const char *green = "";
    const char *blue = "";
    const char *reset = "";
    if (color) {
        redb = "\x1b[1;31m";
        red = "\x1b[31m";
        yellowb = "\x1b[1;33m";
        yellow = "\x1b[33m";
        green = "\x1b[32m";
        blue = "\x1b[34m";
        reset = "\x1b[0m";
    }
    /* no of characters_written(cw) by snprintf */
    int cw = 0;

    /* make a copy of the format string as it will be modified below */
    char local_format[strlen(log_format) + 1];
    strlcpy(local_format, log_format, sizeof(local_format));
    char *temp_fmt = local_format;
    char *substr = temp_fmt;

	while ( (temp_fmt = index(temp_fmt, SC_LOG_FMT_PREFIX)) ) {
        if ((temp - buffer) > ORYX_LOG_MAX_LOG_MSG_LEN) {
            return 0;
        }
        switch(temp_fmt[1]) {
            case SC_LOG_FMT_TIME:
                temp_fmt[0] = '\0';

                struct tm local_tm;
                tms = oryx_localtime(tval->tv_sec, &local_tm);

                cw = snprintf(temp, ORYX_LOG_MAX_LOG_MSG_LEN - (temp - buffer),
                              "%s%s%d/%d/%04d -- %02d:%02d:%02d%s",
                              substr, green, tms->tm_mday, tms->tm_mon + 1,
                              tms->tm_year + 1900, tms->tm_hour, tms->tm_min,
                              tms->tm_sec, reset);
                if (cw < 0)
                    return -1;
                temp += cw;
                temp_fmt++;
                substr = temp_fmt;
                substr++;
                break;

            case SC_LOG_FMT_PID:
                temp_fmt[0] = '\0';
                cw = snprintf(temp, ORYX_LOG_MAX_LOG_MSG_LEN - (temp - buffer),
                              "%s%s%u%s", substr, yellow, getpid(), reset);
                if (cw < 0)
                    return -1;
                temp += cw;
                temp_fmt++;
                substr = temp_fmt;
                substr++;
                break;

            case SC_LOG_FMT_TID:
                temp_fmt[0] = '\0';
                cw = snprintf(temp, ORYX_LOG_MAX_LOG_MSG_LEN - (temp - buffer),
                              "%s%s%lu%s", substr, yellow, SCGetThreadIdLong(), reset);
                if (cw < 0)
                    return -1;
                temp += cw;
                temp_fmt++;
                substr = temp_fmt;
                substr++;
                break;

            case SC_LOG_FMT_TM:
                temp_fmt[0] = '\0';
/* disabled to prevent dead lock:
 * log or alloc (which calls log on error) can call TmThreadsGetCallingThread
 * which will lock tv_root_lock. This can happen while we already hold this
 * lock. */
#if 0
                ThreadVars *tv = TmThreadsGetCallingThread();
                cw = snprintf(temp, ORYX_LOG_MAX_LOG_MSG_LEN - (temp - *msg),
                              "%s%s", substr, ((tv != NULL)? tv->name: "UNKNOWN TM"));
#endif
                cw = snprintf(temp, ORYX_LOG_MAX_LOG_MSG_LEN - (temp - buffer),
                              "%s%s", substr, "N/A");
                if (cw < 0)
                    return -1;
                temp += cw;
                temp_fmt++;
                substr = temp_fmt;
                substr++;
                break;

            case SC_LOG_FMT_LOG_LEVEL:
                temp_fmt[0] = '\0';
                s = loglevel_format(log_level);
                if (s != NULL) {
                    if (log_level <= ORYX_LOG_ERROR)
                        cw = snprintf(temp, ORYX_LOG_MAX_LOG_MSG_LEN - (temp - buffer),
                                  "%s%s%s%s", substr, redb, s, reset);
                    else if (log_level == ORYX_LOG_WARNING)
                        cw = snprintf(temp, ORYX_LOG_MAX_LOG_MSG_LEN - (temp - buffer),
                                  "%s%s%s%s", substr, red, s, reset);
                    else if (log_level == ORYX_LOG_NOTICE)
                        cw = snprintf(temp, ORYX_LOG_MAX_LOG_MSG_LEN - (temp - buffer),
                                  "%s%s%s%s", substr, yellowb, s, reset);
                    else
                        cw = snprintf(temp, ORYX_LOG_MAX_LOG_MSG_LEN - (temp - buffer),
                                  "%s%s%s%s", substr, yellow, s, reset);
                } else {
                    cw = snprintf(temp, ORYX_LOG_MAX_LOG_MSG_LEN - (temp - buffer),
                                  "%s%s", substr, "INVALID");
                }
                if (cw < 0)
                    return -1;
                temp += cw;
                temp_fmt++;
                substr = temp_fmt;
                substr++;
                break;

            case SC_LOG_FMT_FILE_NAME:
                temp_fmt[0] = '\0';
                cw = snprintf(temp, ORYX_LOG_MAX_LOG_MSG_LEN - (temp - buffer),
                              "%s%s%s%s", substr, blue, file, reset);
                if (cw < 0)
                    return -1;
                temp += cw;
                temp_fmt++;
                substr = temp_fmt;
                substr++;
                break;

            case SC_LOG_FMT_LINE:
                temp_fmt[0] = '\0';
                cw = snprintf(temp, ORYX_LOG_MAX_LOG_MSG_LEN - (temp - buffer),
                              "%s%s%u%s", substr, green, line, reset);
                if (cw < 0)
                    return -1;
                temp += cw;
                temp_fmt++;
                substr = temp_fmt;
                substr++;
                break;

            case SC_LOG_FMT_FUNCTION:
                temp_fmt[0] = '\0';
                cw = snprintf(temp, ORYX_LOG_MAX_LOG_MSG_LEN - (temp - buffer),
                              "%s%s%s%s", substr, green, function, reset);
                if (cw < 0)
                    return -1;
                temp += cw;
                temp_fmt++;
                substr = temp_fmt;
                substr++;
                break;

        }
        temp_fmt++;
	}
    if ((temp - buffer) > ORYX_LOG_MAX_LOG_MSG_LEN) {
        return 0;
    }
    cw = snprintf(temp, ORYX_LOG_MAX_LOG_MSG_LEN - (temp - buffer), "%s", substr);
    if (cw < 0) {
        return -1;
    }
    temp += cw;
    if ((temp - buffer) > ORYX_LOG_MAX_LOG_MSG_LEN) {
        return 0;
    }
	
    if (error_code != 0) {
        cw = snprintf(temp, ORYX_LOG_MAX_LOG_MSG_LEN - (temp - buffer),
                "[%sERRCODE%s: %s%s%s(%s%d%s)] - ", yellow, reset, red, errno2_string(error_code), reset, yellow, error_code, reset);
        if (cw < 0) {
            return -1;
        }
        temp += cw;
        if ((temp - buffer) > ORYX_LOG_MAX_LOG_MSG_LEN) {
            return 0;
        }
    }

    const char *hi = "";
    if (error_code > 0)
        hi = red;
    else if (log_level <= ORYX_LOG_NOTICE)
        hi = yellow;
    cw = snprintf(temp, ORYX_LOG_MAX_LOG_MSG_LEN - (temp - buffer), "%s%s%s", hi, message, reset);
    if (cw < 0) {
        return -1;
    }
    temp += cw;
    if ((temp - buffer) > ORYX_LOG_MAX_LOG_MSG_LEN) {
        return 0;
    }
#if 0
    if (sc_log_config->op_filter_regex != NULL) {
#define MAX_SUBSTRINGS 30
        int ov[MAX_SUBSTRINGS];

        if (pcre_exec(sc_log_config->op_filter_regex,
                      sc_log_config->op_filter_regex_study,
                      buffer, strlen(buffer), 0, 0, ov, MAX_SUBSTRINGS) < 0)
        {
            return SC_ERR_LOG_FG_FILTER_MATCH; // bit hacky, but just return !0
        }
#undef MAX_SUBSTRINGS
    }
#endif
    return 0;
}
					 
void oryx_logging_out(const int log_level, const char *file,
                     const unsigned int line, const char *function,
                     const int error_code, const char *message)
{
	char buffer[ORYX_LOG_MAX_LOG_MSG_LEN] = "";
    /* get ts here so we log the same ts to each output */
    struct timeval tval;
    gettimeofday(&tval, NULL);

	if (oryx_log2_buffer(&tval, 1, SC_LOG_OP_TYPE_REGULAR,
							  buffer, sizeof(buffer),
							  SC_LOG_DEF_LOG_FORMAT,
							  log_level, file, line, function,
							  error_code, message) == 0)
	{
		oryx_log2_stream((log_level == ORYX_LOG_ERROR)? stderr: stdout, buffer);
	}

}

#define ORYX_BACKTRACE_SIZE 256
					 
/* dump the stack of the calling core */
void oryx_dump_stack(void)
{
#ifdef HAVE_BACKTRACE
	void *func[BACKTRACE_SIZE];
	char **symb = NULL;
	int size;

	size = backtrace(func, BACKTRACE_SIZE);
	symb = backtrace_symbols(func, size);

	if (symb == NULL)
		return;

	while (size > 0) {
		oryx_loge(-1,
		 "%d: [%s]", size, symb[size - 1]);
		size --;
	}

	free(symb);
#endif /* RTE_BACKTRACE */
}

/* not implemented in this environment */
void oryx_dump_registers(void)
{
	return;
}


/*
 * Like rte_panic this terminates the application. However, no traceback is
 * provided and no core-dump is generated.
 */
void
oryx_panic(int exit_code, const char *format, ...)
{
	va_list ap;
	FILE *f = stderr;

	if (exit_code != 0)
		oryx_logn("Error - exiting with code: %d\n"
				"  Cause: ", exit_code);

	va_start(ap, format);
	vfprintf(f, format, ap);
	fflush(f);
	va_end(ap);

	oryx_dump_stack();
	oryx_dump_registers();
	abort();
}

