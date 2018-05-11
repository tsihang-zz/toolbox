
#ifndef __ORYX_H__
#define __ORYX_H__

#include <sched.h>

#ifdef HAVE_APR
#include "apr.h"
#include "apr_arch_thread_mutex.h"
#include "apr_arch_thread_cond.h"
#include "apr_arch_file_io.h"
#include "apr_arch_networkio.h"

#include "apr_thread_mutex.h"
#include "apr_thread_cond.h"
#include "apr_file_io.h"
#include "apr_network_io.h"
#include "apr_signal.h"
#include "apr_file_io.h"
#include "apr_dbd.h"
#include "apr_md5.h"
#endif

#include <assert.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <mntent.h>
#include <netdb.h>
#include <poll.h>
#include <pwd.h>
#include <regex.h>
#include <syscall.h>
#include <sched.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <semaphore.h>
#include <sys/epoll.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>
#include <getopt.h>
#include <pthread.h>
#include <syslog.h>
#include <signal.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/sockios.h>
#include <net/if.h>
#include <sys/time.h>
#include <stdalign.h>
#include <math.h>

#if !defined(HAVE_DPDK)
#include <net/ethernet.h>
#endif

#if defined(HAVE_PCAP)
#include <pcap.h>
#include <pcap/pcap.h>
#include <pcap/bpf.h>
#endif

/** 
 * Define a common assume_aligned using an appropriate compiler built-in, if
 * it's available. Note that we need to handle C or C++ compilation. */
#ifdef __cplusplus
#  ifdef HAVE_CXX_BUILTIN_ASSUME_ALIGNED
#    define assume_aligned(x, y) __builtin_assume_aligned((x), (y))
#  endif
#else
#  ifdef HAVE_CC_BUILTIN_ASSUME_ALIGNED
#    define assume_aligned(x, y) __builtin_assume_aligned((x), (y))
#  endif
#endif
// Fallback to identity case.
#ifndef assume_aligned
# define assume_aligned(x, y) (x)
#endif

#define ISALIGNED_N(ptr, n) (((uintptr_t)(ptr) & ((n) - 1)) == 0)

#if defined(HAVE_TYPEOF)
# define ISALIGNED(ptr)      ISALIGNED_N((ptr), alignof(__typeof__(*(ptr))))
#else
/* we should probably avoid using this test in C */
# define ISALIGNED(ptr)      (1)
#endif

#define ISALIGNED_16(ptr)   ISALIGNED_N((ptr), 16)
#define ISALIGNED_CL(ptr)   ISALIGNED_N((ptr), 64)

#ifndef __BITS_PER_LONG
#if defined(__x86_64__) || defined(__aarch64__)
#	define __BITS_PER_LONG 64
#else
#	define __BITS_PER_LONG 32
#endif
#endif

#ifndef VALID
#define VALID 1
#define INVALID 0
#endif

#ifndef FOREVER
#define FOREVER for(;;)
#endif

#ifndef THIS_API_IS_ONLY_FOR_TEST
#define THIS_API_IS_ONLY_FOR_TEST
#endif

#ifndef The_interface_has_not_been_tested
#define The_interface_has_not_been_tested
#define THE_INTERFACE_HAS_NOT_BEEN_TESTED
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

/** Alway treated the expr as true */
#ifndef likely
#define likely(expr) __builtin_expect(!!(expr), 1)
#endif

/** Alway treated the expr as false */
#ifndef unlikely
#define unlikely(expr) __builtin_expect(!!(expr), 0)
#endif

#ifndef ERRNO_EQUAL
#define ERRNO_EQUAL(e) (errno == e)
#endif

#define container_of(ptr, type, member) ({ \
        const typeof(((type *)0)->member) *__mptr = (ptr); \
        (type *)((char *)__mptr - offsetof(type,member));})

#ifndef __GNUC__
#  define  __attribute__(x)  /*NOTHING*/
#endif

#define __oryx_unused_val__		0

/* Shorthand for attribute to mark a function as part of our public API.
 * Functions without this attribute will be hidden. */
#if defined(__GNUC__)
#define __oryx_always_extern__		__attribute__((visibility("default")))
#define	__oryx_always_inline__		__attribute__((always_inline)) inline
#define	__oryx_unused__				__attribute__((unused))
#define __oryx_noreturn__(fn)		__attribute__((noreturn))fn
//#define __oryx_format_func__(f,a)	__attribute__((format(__NSString__, f, a)))
#else
// TODO: dllexport defines for windows
#define __oryx_always_extern__
#define	__oryx_always_inline__	
#define	__oryx_unused__
#endif

/* we need this to stringify the defines which are supplied at compiletime see:
   http://gcc.gnu.org/onlinedocs/gcc-3.4.1/cpp/Stringification.html#Stringification */
#define xstr(s) str(s)
#define str(s) #s

#if 0
#ifndef BUG_ON
#define BUG_ON(expr)	assert(!(expr))
#endif

#ifndef WARN_ON
#define WARN_ON(expr)	assert(!(expr))
#endif
#endif

/** std variable. */
typedef unsigned char	oryx_byte_t;
typedef size_t		oryx_size_t;
typedef int16_t		oryx_int16_t;
typedef uint8_t		oryx_uint8_t;
typedef uint16_t		oryx_uint16_t;
typedef uint32_t		oryx_uint32_t;
typedef int32_t		oryx_int32_t;
typedef uint64_t		oryx_uint64_t;
typedef int64_t		oryx_int64_t;
typedef off_t			oryx_off_t;
typedef socklen_t		oryx_socklen_t;
typedef ino_t          	oryx_ino_t;
typedef oryx_uint16_t	oryx_port_t;
typedef oryx_uint32_t		HASH_INDEX;
typedef DIR				oryx_dir_t;			   /**< native dirent */
typedef FILE				oryx_file_t;
typedef struct timeval		oryx_os_imp_time_t;    /**< native timeval */
typedef struct tm			oryx_os_exp_time_t;    /**< native tm */
typedef oryx_int32_t		oryx_handler_t;
typedef signed char i8;
typedef signed short i16;
typedef signed int i32;
typedef signed long long i64;
typedef unsigned char u8;
typedef unsigned char uchar;
typedef unsigned short u16;
typedef unsigned int u32;
/* Floating point types. */
typedef double f64;
typedef float f32;
typedef unsigned long long u64;
typedef unsigned long uword;
typedef void *			oryx_os_shm_t;         /**< native SHM */
typedef oryx_int32_t	oryx_status_t;
typedef pthread_t		oryx_os_thread_t;
typedef void *			oryx_os_dso_handle_t;
typedef u32 			key32_t;
typedef void* 			ht_value_t;
typedef key32_t			ht_key_t;

#define GETL_BYTE2(p)		(((p)[0]<<8)  | (p)[1])
#define GETL_BYTE3(p)		(((p)[0]<<16) | ((p)[1]<<8)  | (p)[2])
#define GETL_BYTE4(p)		(((p)[0]<<24) | ((p)[1]<<16) | ((p)[2]<<8) | (p)[3])

#define ADDR_OFFSETOF(addr, bytes) ((addr>>(bytes*8))&0x000000FF)

typedef uint16_t u16_port_t;

/* Network address indicated for protocol header. */
typedef union {
	uint8_t	 val8[16];
	uint32_t val32[4];
	uint64_t val64[2];
	struct{
		uint32_t    v4;
		uint32_t    r;
	};

	struct {
	    uint64_t    v6u;	/** Upper */
	    uint64_t    v6l;	/** Lower */
	};
} ip_addr_t;

/* Address */
struct inet_addr {
    char family;
    union {
        uint32_t       data32[4]; /* type-specific field */
        uint16_t       data16[8]; /* type-specific field */
        uint8_t        data8[16]; /* type-specific field */
    } address;
};

/**
 * Alignment macros
 */

/* ORYX_ALIGN() is only to be used to align on a power of 2 boundary */
#define ORYX_ALIGN(size, boundary) \
    (((size) + ((boundary) - 1)) & ~((boundary) - 1))

/** Default alignment */
#define ORYX_ALIGN_DEFAULT(size) ORYX_ALIGN(size, 8)



/* temporary defines to handle 64bit compile mismatches */
#define ORYX_INT_TRUNC_CAST    int
#define ORYX_UINT32_TRUNC_CAST oryx_uint32_t

/* Mechanisms to properly type numeric literals */
#define ORYX_INT64_C(val) INT64_C(val)
#define ORYX_UINT64_C(val) UINT64_C(val)


/** @} */

/**
 * @defgroup oryx_ctype ctype functions
 * These macros allow correct support of 8-bit characters on systems which
 * support 8-bit characters.  Pretty dumb how the cast is required, but
 * that's legacy libc for ya.  These new macros do not support EOF like
 * the standard macros do.  Tough.
 * @{
 */
/** @see isalnum */
#define oryx_isalnum(c) (isalnum(((unsigned char)(c))))
/** @see isalpha */
#define oryx_isalpha(c) (isalpha(((unsigned char)(c))))
/** @see iscntrl */
#define oryx_iscntrl(c) (iscntrl(((unsigned char)(c))))
/** @see isdigit */
#define oryx_isdigit(c) (isdigit(((unsigned char)(c))))
/** @see isgraph */
#define oryx_isgraph(c) (isgraph(((unsigned char)(c))))
/** @see islower*/
#define oryx_islower(c) (islower(((unsigned char)(c))))
/** @see isascii */
#ifdef isascii
#define oryx_isascii(c) (isascii(((unsigned char)(c))))
#else
#define oryx_isascii(c) (((c) & ~0x7f)==0)
#endif
/** @see isprint */
#define oryx_isprint(c) (isprint(((unsigned char)(c))))
/** @see ispunct */
#define oryx_ispunct(c) (ispunct(((unsigned char)(c))))
/** @see isspace */
#define oryx_isspace(c) (isspace(((unsigned char)(c))))
/** @see isupper */
#define oryx_isupper(c) (isupper(((unsigned char)(c))))
/** @see isxdigit */
#define oryx_isxdigit(c) (isxdigit(((unsigned char)(c))))
/** @see tolower */
#define oryx_tolower(c) (tolower(((unsigned char)(c))))
/** @see toupper */
#define oryx_toupper(c) (toupper(((unsigned char)(c))))

/** @} */


/* Macroes for conversion between host and network byte order */
#define hton8(x)  (x)
#define ntoh8(x)  (x)
#define hton16(x) htons(x)
#define ntoh16(x) ntohs(x)
#define hton32(x) htonl(x)
#define ntoh32(x) ntohl(x)

#ifndef INT_MAX
#define INT_MAX ((int)(~0U>>1))
#endif

#ifndef INT_MIN
#define INT_MIN (-INT_MAX - 1)
#endif

#ifndef UINT_MAX
#define UINT_MAX (~0U)
#endif

#ifndef LONG_MAX
#define LONG_MAX ((long)(~0UL>>1))
#endif

#ifndef LONG_MIN
#define LONG_MIN (-LONG_MAX - 1)
#endif

#ifndef ULONG_MAX
#define ULONG_MAX (~0UL)
#endif

#ifndef LLONG_MAX
#define LLONG_MAX ((long long)(~0ULL>>1))
#endif

#ifndef LLONG_MIN
#define LLONG_MIN (-LLONG_MAX - 1)
#endif

#ifndef ULLONG_MAX
#define ULLONG_MAX (~0ULL)
#endif


#define ORYX_INT32_MAX		INT_MAX
#define ORYX_INT32_MIN		INT_MIN
#define ORYX_UINT32_MAX		UINT_MAX

#define ORYX_THREAD_FUNC
#define ORYX_DECLARE_DATA
#define ORYX_DECLARE_EXPORT

#if defined(DOXYGEN) || !defined(WIN32)

/**
 * The public ORYX functions are declared with ORYX_DECLARE(), so they may
 * use the most appropriate calling convention.  Public ORYX functions with 
 * variable arguments must use ORYX_DECLARE_NONSTD().
 *
 * @remark Both the declaration and implementations must use the same macro.
 *
 * <PRE>
 * ORYX_DECLARE(rettype) oryx_func(args)
 * </PRE>
 * @see ORYX_DECLARE_NONSTD @see ORYX_DECLARE_DATA
 * @remark Note that when ORYX compiles the library itself, it passes the 
 * symbol -DORYX_DECLARE_EXPORT to the compiler on some platforms (e.g. Win32) 
 * to export public symbols from the dynamic library build.\n
 * The user must define the ORYX_DECLARE_STATIC when compiling to target
 * the static ORYX library on some platforms (e.g. Win32.)  The public symbols 
 * are neither exported nor imported when ORYX_DECLARE_STATIC is defined.\n
 * By default, compiling an application and including the ORYX public
 * headers, without defining ORYX_DECLARE_STATIC, will prepare the code to be
 * linked to the dynamic library.
 */
#define ORYX_DECLARE(type)            type 

/**
 * The public ORYX functions using variable arguments are declared with 
 * ORYX_DECLARE_NONSTD(), as they must follow the C language calling convention.
 * @see ORYX_DECLARE @see ORYX_DECLARE_DATA
 * @remark Both the declaration and implementations must use the same macro.
 * <PRE>
 *
 * ORYX_DECLARE_NONSTD(rettype) oryx_func(args, ...);
 *
 * </PRE>
 */
#define ORYX_DECLARE_NONSTD(type)     type

/**
 * The public ORYX variables are declared with AP_MODULE_DECLARE_DATA.
 * This assures the appropriate indirection is invoked at compile time.
 * @see ORYX_DECLARE @see ORYX_DECLARE_NONSTD
 * @remark Note that the declaration and implementations use different forms,
 * but both must include the macro.
 * 
 * <PRE>
 *
 * extern ORYX_DECLARE_DATA type oryx_variable;\n
 * ORYX_DECLARE_DATA type oryx_variable = value;
 *
 * </PRE>
 */

#elif defined(ORYX_DECLARE_STATIC)
#define ORYX_DECLARE(type)            type __stdcall
#define ORYX_DECLARE_NONSTD(type)     type __cdecl
#define ORYX_DECLARE_DATA
#elif defined(ORYX_DECLARE_EXPORT)
#define ORYX_DECLARE(type)            __declspec(dllexport) type __stdcall
#define ORYX_DECLARE_NONSTD(type)     __declspec(dllexport) type __cdecl
#define ORYX_DECLARE_DATA             __declspec(dllexport)
#else
#define ORYX_DECLARE(type)            __declspec(dllimport) type __stdcall
#define ORYX_DECLARE_NONSTD(type)     __declspec(dllimport) type __cdecl
#define ORYX_DECLARE_DATA             __declspec(dllimport)
#endif

#if defined(_WIN32)
#define ALIGN_ATTR(x) __declspec(align(x))
#else
#define ALIGN_ATTR(x) __attribute__((aligned((x))))
#endif

#define ALIGN_DIRECTIVE ALIGN_ATTR(16)
#define ALIGN_AVX_DIRECTIVE ALIGN_ATTR(32)
#define ALIGN_CL_DIRECTIVE ALIGN_ATTR(64)

#define ARRAY_LENGTH(a) (sizeof(a)/sizeof((a)[0]))


/* Define ORYX_SSIZE_T_FMT.  
 * If ssize_t is an integer we define it to be "d",
 * if ssize_t is a long int we define it to be "ld",
 * if ssize_t is neither we declare an error here.
 * I looked for a better way to define this here, but couldn't find one, so
 * to find the logic for this definition search for "ssize_t_fmt" in
 * configure.in.
 */

#define ORYX_SSIZE_T_FMT "ld"

/* And ORYX_SIZE_T_FMT */
#define ORYX_SIZE_T_FMT "lu"

/* And ORYX_OFF_T_FMT */
#define ORYX_OFF_T_FMT "ld"

/* And ORYX_PID_T_FMT */
#define ORYX_PID_T_FMT "d"

/* And ORYX_INT64_T_FMT */
#define ORYX_INT64_T_FMT "ld"

/* And ORYX_UINT64_T_FMT */
#define ORYX_UINT64_T_FMT "lu"

/* And ORYX_UINT64_T_HEX_FMT */
#define ORYX_UINT64_T_HEX_FMT "lx"


#define STRING_MAX 8096

enum {LOOKUP_ID, LOOKUP_ALIAS};

struct prefix_t {
	u8 cmd;
	void *v;
	u8 s;	/** sizeof(v) */
};


#define BOLD                 "\e[1m"
#define UNDERLINE      "\e[4m"
#define BLINK               "\e[5m"
#define REVERSE           "\e[7m"
#define HIDE                  "\e[8m"
#define CLEAR                "\e[2J"
#define CLRLINE            "\r\e[K" //or "\e[1K\r"

#define foreach_color			\
  _(FIN, "\e[0m", "Finish drawing.")	\
  _(BLACK, "\e[0;30m", "Black color.")	\
  _(RED, "\e[0;31m", "Red color.")\
  _(GREEN, "\e[0;32m", "Green color.")\
  _(YELLOW, "\e[0;33m", "Yellow color.")\
  _(BLUE, "\e[0;34m", "Blue color.")\
  _(PURPLE, "\e[0;35m", "Purple color.")\
  _(CYAN, "\e[0;36m", "Cyan color.")\
  _(WHITE, "\e[0;37m", "White color.")

typedef enum {
#define _(f,s,d) COLOR_##f,
	foreach_color
#undef _
	COLORS,
}color_t;

#define COLOR_TYPES {\
    [COLOR_FIN] = "\e[0m",\
    [COLOR_BLACK] = "\e[0;30m",\
    [COLOR_RED] = "\e[0;31m",\
    [COLOR_GREEN] = "\e[0;32m",\
    [COLOR_YELLOW] = "\e[0;33m",\
    [COLOR_BLUE] = "\e[0;34m",\
    [COLOR_PURPLE] = "\e[0;35m",\
    [COLOR_CYAN] = "\e[0;36m",\
    [COLOR_WHITE] = "\e[0;37m",\
    [COLORS] = "skip-draw-color"\
}

/* Current function name.  Need (char *) cast to silence gcc4 pointer signedness warning. */
#define oryx_error_function ((char *) __FUNCTION__)

#define ASSERT_ENABLE	1

#define ASSERT(expr)					\
do {							\
  if (ASSERT_ENABLE && ! (expr))			\
    {							\
      printf (		\
		   "%s:%lu (%s) assertion `%s' fails\n",	\
		   __FILE__,				\
		   (uword) __LINE__,			\
		   oryx_error_function,			\
		   # expr);				\
    }							\
} while (0)

oryx_status_t oryx_initialize(void);

#include "oryx_error.h"
#include "oryx_ipc.h"
#include "oryx_utils.h"
#include "oryx_memory.h"
#include "oryx_htable.h"
#include "oryx_actq.h"
#include "oryx_task.h"
#include "oryx_tmr.h"
#include "oryx_mpm.h"
#include "oryx_netdev.h"

#endif	/* ORYX_H */

