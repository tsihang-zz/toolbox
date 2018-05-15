#ifndef __ERRORS_H__
#define __ERRORS_H__

#include <errno.h>

/** errno(s), return (-ERRNO_XX_XXX) if the api is not declared with rt_errno */
typedef enum {

	/**
	    Standard errno(s)
	*/
	/** Successfully */
	ORYX_SUCCESS,
	
	/** No such element */
	ERRNO_NO_ELEMENT,
	/** Invalid argument */
	ERRNO_INVALID_ARGU,
	/** Invalid value */
	ERRNO_INVALID_VAL,
	/** Permission denied */
	ERRNO_ACCESS_DENIED,
	/** Out of range */
	ERRNO_RANGE,
	/** Timer expired */
	ERRNO_TMDOUT,
	/** Fatal error */
	ERRNO_FATAL,
	ERRNO_WARNING,


	/** ERRNO + MODULE + REASON */

	/**
	    errno(s) for MEMORY
	*/
	ERRNO_MEM_ALLOC,
	ERRNO_MEM_FREE,
	ERRNO_MEM_INVALID, /** NULL pointer */
	ERRNO_MEMBLK_LOOKUP,
	ERRNO_MEMBLK_ALLOC,


	/**
	    errno(s) for SEMPHORE
	*/
	ERRNO_SEMPHORE_POST,
	ERRNO_SEMPHORE_WAIT,

	/**
	    errno(s) for MUTEX
	*/
	ERRNO_MUTEX_INIT,
	ERRNO_MUTEX_LOCK,
	ERRNO_MUTEX_UNLOCK,


	/**
	    errno(s) for COND
	*/
	ERRNO_COND_INIT,

	/**
	    errno(s) for PTHREAD
	*/
	ERRNO_THRD_CREATE,
	ERRNO_THRD_DETACH,


	/**
	    errno(s) for LOGGING
	*/
	ERRNO_LOGGING_INIT,
	ERRNO_LOGGING_FG_FILTER_MATCH,
	ERRNO_LOGGING_FD_FILTER_MATCH,

	/**
	    errno(s) for MQ
	*/
	ERRNO_MQ_INVALID,
	ERRNO_MQ_NO_RESOURCE,
	ERRNO_MQ_NO_MEMORY,


	/**
	    errno(s) for CAPTURE
	*/
	ERRNO_PCAP,
	ERRNO_PCAP_ERROR,
	ERRNO_PCAP_OPEN,
	ERRNO_PCAP_COMPILE,

	/**
	    errno(s) for RT_THROUGHPUT
	*/
	ERRNO_THROUGHPUT_NO_FILE,
	ERRNO_THROUGHPUT_INVALID,


	/**
	    errno(s) for RPC
	*/
	ERRNO_RPC_INVALID_ID,
	ERRNO_RPC_SIZE,

	/**
	    errno(s) for YAML
	*/
	ERRNO_YAML,

	/**
	    errno(s) for UI
	*/
	ERRNO_UI_YAML_ARGU,
	ERRNO_UI_A29,
	ERRNO_UI_LOCAL,

	/**
	    errno(s) for MA
	*/
	ERRNO_MA_YAML_NO_FILE,
	ERRNO_MA_REGISTRY,
	ERRNO_MA_KEEPALIVE,
	ERRNO_MA_INTERACTIVE,

	/**
	    errno(s) for DECODER
	*/
	ERRNO_DECODER_YAML_ARGU,

	/** errno(s) for SESSION */
	ERRNO_SESSION_CB_ALLOC,

	/** errno(s) for TM */
	ERRNO_TM,
	ERRNO_TM_INVALID,
	ERRNO_TM_NO_IDLE,
	ERRNO_TM_MULTIPLE_REG,
	ERRNO_TM_EXSIT,

	/** errno(s) for SOCKET */
	ERRNO_SOCK,

	/** errno(s) for DECODER CLUSTER */
	ERRNO_DC_PEER_CLOSE,
	ERRNO_DC_PEER_ERROR,
	ERRNO_DC_AGENT_SPAWN,

	/** errno(s) for TASK */
	ERRNO_TASK_EXSIT,
	ERRNO_TASK_CREATE,
	ERRNO_TASK_MULTIPLE_REG,

	/** errno(2) for PROBE */
	ERRNO_PROBE_PEER_CLOSE,
	ERRNO_PROBE_PEER_ERROR,

	/** errno(s) for inotify */
	ERRNO_INOTIFY_WATCH,
	ERRNO_INOTIFY_COMMAND,

	ERRNO_SG,

	ERRNO_POOL_INIT,

	ERRNO_FAX_DECODE,

} oryx_error_t;

#define STDIO_BLOCKING_STD_ERROR(rv)   \
    {\
        int sv = rv; \
        if (sv)\
        {   \
            return sv; \
        }\
    }


#define STDIO_RENDERING_STD_ERROR(rv)   \
    {\
        int sv = rv; \
        if (sv)\
        {   \
            printf ("%% Error rv=%d %s", sv,"\n");\
            return sv; \
        }\
    }

#define STDIO_RENDERING_STD_ERROR_AND_CONTINUE(rv)   \
    {\
        int sv = rv; \
        if (sv)\
        {   \
            printf ("%% Error rv=%d %s", sv,"\n");\
            continue; \
        }\
    }

#define STDIO_RENDERING_STD_ERROR_AND_BREAK(rv)   \
    {\
        int sv = rv; \
        if (sv)\
        {   \
            printf ("%% Error rv=%d %s", sv,"\n");\
            break; \
        }\
    }


#define STDIO_BLOCKING_STD_ERROR(rv)   \
    {\
        int sv = rv; \
        if (sv)\
        {   \
            return sv; \
        }\
    }

#define STDIO_BLOCKING_STD_ERROR_AND_CONTINUE(rv)   \
    {\
        int sv = rv; \
        if (sv)\
        {   \
            continue; \
        }\
    }

#define STDIO_BLOCKING_STD_ERROR_AND_BREAK(rv)   \
    {\
        int sv = rv; \
        if (sv)\
        {   \
            break; \
        }\
    }

#endif
