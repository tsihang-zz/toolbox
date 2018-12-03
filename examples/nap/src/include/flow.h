#ifndef FLOW_H
#define FLOW_H

#if defined(HAVE_FLOW_MGR)

#include "common_private.h"

#define FLOWLOCK_MUTEX
#define FBLOCK_MUTEX

#define BIT_U8(n)  ((uint8_t)(1 << (n)))
#define BIT_U16(n) ((uint16_t)(1 << (n)))
#define BIT_U32(n) (1UL  << (n))
#define BIT_U64(n) (1ULL << (n))

#define FLOW_QUIET      TRUE
#define FLOW_VERBOSE    FALSE

#define TOSERVER 0
#define TOCLIENT 1

/* per flow flags */

/** At least on packet from the source address was seen */
#define FLOW_TO_SRC_SEEN                BIT_U32(0)
/** At least on packet from the destination address was seen */
#define FLOW_TO_DST_SEEN                BIT_U32(1)
/** Don't return this from the flow hash. It has been replaced. */
#define FLOW_TCP_REUSED                 BIT_U32(2)

/** Flow was inspected against IP-Only sigs in the toserver direction */
#define FLOW_TOSERVER_IPONLY_SET        BIT_U32(3)
/** Flow was inspected against IP-Only sigs in the toclient direction */
#define FLOW_TOCLIENT_IPONLY_SET        BIT_U32(4)

/** packet_t belonging to this flow should not be inspected at all */
#define FLOW_NOPACKET_INSPECTION        BIT_U32(5)
/** packet_t payloads belonging to this flow should not be inspected */
#define FLOW_NOPAYLOAD_INSPECTION       BIT_U32(6)

/** All packets in this flow should be dropped */
#define FLOW_ACTION_DROP                BIT_U32(7)

/** Sgh for toserver direction set (even if it's NULL) */
#define FLOW_SGH_TOSERVER               BIT_U32(8)
/** Sgh for toclient direction set (even if it's NULL) */
#define FLOW_SGH_TOCLIENT               BIT_U32(9)

/** packet to server direction has been logged in drop file (only in IPS mode) */
#define FLOW_TOSERVER_DROP_LOGGED       BIT_U32(10)
/** packet to client direction has been logged in drop file (only in IPS mode) */
#define FLOW_TOCLIENT_DROP_LOGGED       BIT_U32(11)

/** flow has alerts */
#define FLOW_HAS_ALERTS                 BIT_U32(12)

/** Pattern matcher alproto detection done */
#define FLOW_TS_PM_ALPROTO_DETECT_DONE  BIT_U32(13)
/** Probing parser alproto detection done */
#define FLOW_TS_PP_ALPROTO_DETECT_DONE  BIT_U32(14)
/** Pattern matcher alproto detection done */
#define FLOW_TC_PM_ALPROTO_DETECT_DONE  BIT_U32(15)
/** Probing parser alproto detection done */
#define FLOW_TC_PP_ALPROTO_DETECT_DONE  BIT_U32(16)
#define FLOW_TIMEOUT_REASSEMBLY_DONE    BIT_U32(17)

/** flow is ipv4 */
#define FLOW_IPv4                       BIT_U32(18)
/** flow is ipv6 */
#define FLOW_IPv6                       BIT_U32(19)

#define FLOW_PROTO_DETECT_TS_DONE       BIT_U32(20)
#define FLOW_PROTO_DETECT_TC_DONE       BIT_U32(21)

/** Indicate that alproto detection for flow should be done again */
#define FLOW_CHANGE_PROTO               BIT_U32(22)

/* File flags */

/** no magic on files in this flow */
#define FLOWFILE_NO_MAGIC_TS            BIT_U16(0)
#define FLOWFILE_NO_MAGIC_TC            BIT_U16(1)

/** even if the flow has files, don't store 'm */
#define FLOWFILE_NO_STORE_TS            BIT_U16(2)
#define FLOWFILE_NO_STORE_TC            BIT_U16(3)
/** no md5 on files in this flow */
#define FLOWFILE_NO_MD5_TS              BIT_U16(4)
#define FLOWFILE_NO_MD5_TC              BIT_U16(5)

/** no sha1 on files in this flow */
#define FLOWFILE_NO_SHA1_TS             BIT_U16(6)
#define FLOWFILE_NO_SHA1_TC             BIT_U16(7)

/** no sha256 on files in this flow */
#define FLOWFILE_NO_SHA256_TS           BIT_U16(8)
#define FLOWFILE_NO_SHA256_TC           BIT_U16(9)

/** no size tracking of files in this flow */
#define FLOWFILE_NO_SIZE_TS             BIT_U16(10)
#define FLOWFILE_NO_SIZE_TC             BIT_U16(11)

enum FlowState {
    FLOW_STATE_NEW = 0,
    FLOW_STATE_ESTABLISHED,
    FLOW_STATE_CLOSED,
    FLOW_STATE_LOCAL_BYPASSED,
    FLOW_STATE_CAPTURE_BYPASSED,
};
	
typedef unsigned short FlowStateType;
typedef unsigned short FlowRefCount;

/* Hash key for the flow hash */
typedef struct FlowKey_
{
    Address src, dst;
    Port sp, dp;
    uint8_t proto;
    uint8_t recursion_level;

} FlowKey;

typedef struct FlowAddress_ {
    union {
        uint32_t       address_un_data32[4]; /* type-specific field */
        uint16_t       address_un_data16[8]; /* type-specific field */
        uint8_t        address_un_data8[16]; /* type-specific field */
    } address;
} FlowAddress;

#define addr_data32 address.address_un_data32
#define addr_data16 address.address_un_data16
#define addr_data8  address.address_un_data8

/**
 *  \brief Flow data structure.
 *
 *  The flow is a global data structure that is created for new packets of a
 *  flow and then looked up for the following packets of a flow.
 *
 *  Locking
 *
 *  The flow is updated/used by multiple packets at the same time. This is why
 *  there is a flow-mutex. It's a mutex and not a spinlock because some
 *  operations on the flow can be quite expensive, thus spinning would be
 *  too expensive.
 *
 *  The flow "header" (addresses, ports, proto, recursion level) are static
 *  after the initialization and remain read-only throughout the entire live
 *  of a flow. This is why we can access those without protection of the lock.
 */

typedef struct Flow_
{
    /* flow "header", used for hashing and flow lookup. Static after init,
     * so safe to look at without lock */
    FlowAddress src, dst;
    union {
        Port sp;        /**< tcp/udp source port */
        uint8_t type;   /**< icmp type */
    };
    union {
        Port dp;        /**< tcp/udp destination port */
        uint8_t code;   /**< icmp code */
    };
    uint8_t proto;
    uint8_t recursion_level;
    uint16_t vlan_id[2];

    /** flow hash - the flow hash before hash table size mod. */
    uint32_t flow_hash;

    /* time stamp of last update (last packet). Set/updated under the
     * flow and flow hash row locks, safe to read under either the
     * flow lock or flow hash row lock. */
    struct timeval lastts;

    /* end of flow "header" */

    ATOMIC_DECLARE(FlowStateType, flow_state);

    /** how many pkts and stream msgs are using the flow *right now*. This
     *  variable is atomic so not protected by the Flow mutex "m".
     *
     *  On receiving a packet the counter is incremented while the flow
     *  bucked is locked, which is also the case on timeout pruning.
     */
    ATOMIC_DECLARE(FlowRefCount, use_cnt);

    /** flow tenant id, used to setup flow timeout and stream pseudo
     *  packets with the correct tenant id set */
    uint32_t tenant_id;

    uint32_t probing_parser_toserver_alproto_masks;
    uint32_t probing_parser_toclient_alproto_masks;

    uint32_t flags;         /**< generic flags */

    uint16_t file_flags;    /**< file tracking/extraction flags */
    /* coccinelle: Flow:file_flags:FLOWFILE_ */

    /** destination port to be used in protocol detection. This is meant
     *  for use with STARTTLS and HTTP CONNECT detection */
    uint16_t protodetect_dp; /**< 0 if not used */

#ifdef FLOWLOCK_RWLOCK
    os_rwlock_t r;
#elif defined FLOWLOCK_MUTEX
    os_mutex_t m;
#else
    #error Enable FLOWLOCK_RWLOCK or FLOWLOCK_MUTEX
#endif

    /** protocol specific data pointer, e.g. for TcpSession */
    void *protoctx;

    /** mapping to Flow's protocol specific protocols for timeouts
        and state and free functions. */
    uint8_t protomap;

    uint8_t flow_end_flags;
    /* coccinelle: Flow:flow_end_flags:FLOW_END_FLAG_ */

#if 0
    AppProto alproto; /**< \brief application level protocol */
    AppProto alproto_ts;
    AppProto alproto_tc;

    /** original application level protocol. Used to indicate the previous
       protocol when changing to another protocol , e.g. with STARTTLS. */
    AppProto alproto_orig;
    /** expected app protocol: used in protocol change/upgrade like in
     *  STARTTLS. */
    AppProto alproto_expect;
	
    /** Thread ID for the stream/detect portion of this flow */
    FlowThreadId thread_id;
	
    /** application level storage ptrs.
     *
     */
    AppLayerParserState *alparser;     /**< parser internal state */

	/** toclient sgh for this flow. Only use when FLOW_SGH_TOCLIENT flow flag
     *  has been set. */
    const struct SigGroupHead_ *sgh_toclient;
    /** toserver sgh for this flow. Only use when FLOW_SGH_TOSERVER flow flag
     *  has been set. */
    const struct SigGroupHead_ *sgh_toserver;

    /* pointer to the var list */
    GenericVar *flowvar;

    /** detection engine ctx version used to inspect this flow. Set at initial
     *  inspection. If it doesn't match the currently in use de_ctx, the
     *  stored sgh ptrs are reset. */
    uint32_t de_ctx_version;

    void *alstate;      /**< application layer state */

    uint32_t todstpktcnt;
    uint32_t tosrcpktcnt;
    uint64_t todstbytecnt;
    uint64_t tosrcbytecnt;

#endif

    /** hash list pointers, protected by fb->s */
    struct Flow_ *hnext; /* hash list */
    struct Flow_ *hprev;
    struct FlowBucket_ *fb;

    /** queue list pointers, protected by queue mutex */
    struct Flow_ *lnext; /* list */
    struct Flow_ *lprev;
    struct timeval startts;


} Flow;

#define FLOW_IS_IPv4(f) \
    (((f)->flags & FLOW_IPv4) == FLOW_IPv4)
#define FLOW_IS_IPv6(f) \
    (((f)->flags & FLOW_IPv6) == FLOW_IPv6)

#define RESET_COUNTERS(f) do { \
    } while (0)

#define FLOW_INITIALIZE(f) do { \
        (f)->sp = 0; \
        (f)->dp = 0; \
        (f)->proto = 0; \
        ATOMIC_INIT((f)->flow_state); \
        ATOMIC_INIT((f)->use_cnt); \
        (f)->flags = 0; \
        (f)->lastts.tv_sec = 0; \
        (f)->lastts.tv_usec = 0; \
        FLOWLOCK_INIT((f)); \
        (f)->protoctx = NULL; \
        (f)->flow_end_flags = 0; \
        (f)->hnext = NULL; \
        (f)->hprev = NULL; \
        (f)->lnext = NULL; \
        (f)->lprev = NULL; \
        RESET_COUNTERS((f)); \
    } while (0)

/* flow hash bucket -- the hash is basically an array of these buckets.
 * Each bucket contains a flow or list of flows. All these flows have
 * the same hashkey (the hash is a chained hash). When doing modifications
 * to the list, the entire bucket is locked. */
typedef struct FlowBucket_ {
    Flow *head;
    Flow *tail;
#ifdef FBLOCK_MUTEX
    os_mutex_t m;
#elif defined FBLOCK_SPIN
    os_spinlock_t s;
#else
    #error Enable FBLOCK_SPIN or FBLOCK_MUTEX
#endif
    /** timestamp in seconds of the earliest possible moment a flow
     *  will time out in this row. Set by the flow manager. Cleared
     *  to 0 by workers, either when new flows are added or when a
     *  flow state changes. The flow manager sets this to INT_MAX for
     *  empty buckets. */
    ATOMIC_DECLARE(int32_t, next_ts);
} __attribute__((aligned(64))) FlowBucket;

#ifdef FLOWLOCK_RWLOCK
    #define FLOWLOCK_INIT(fb) do_rwlock_init(&(fb)->r, NULL)
    #define FLOWLOCK_DESTROY(fb) do_rwlock_destroy(&(fb)->r)
    #define FLOWLOCK_RDLOCK(fb) do_rwlock_lock_rd(&(fb)->r)
    #define FLOWLOCK_WRLOCK(fb) do_rwlock_lock_wr(&(fb)->r)
    #define FLOWLOCK_TRYRDLOCK(fb) do_rwlock_trylock_rd(&(fb)->r)
    #define FLOWLOCK_TRYWRLOCK(fb) do_rwlock_trylock_wr(&(fb)->r)
    #define FLOWLOCK_UNLOCK(fb) do_rwlock_unlock(&(fb)->r)
#elif defined FLOWLOCK_MUTEX
    #define FLOWLOCK_INIT(fb) do_mutex_init(&(fb)->m)
    #define FLOWLOCK_DESTROY(fb) do_mutex_destroy(&(fb)->m)
    #define FLOWLOCK_RDLOCK(fb) do_mutex_lock(&(fb)->m)
    #define FLOWLOCK_WRLOCK(fb) do_mutex_lock(&(fb)->m)
    #define FLOWLOCK_TRYRDLOCK(fb) do_mutex_trylock(&(fb)->m)
    #define FLOWLOCK_TRYWRLOCK(fb) do_mutex_trylock(&(fb)->m)
    #define FLOWLOCK_UNLOCK(fb) do_mutex_unlock(&(fb)->m)
#else
    #error Enable FLOWLOCK_RWLOCK or FLOWLOCK_MUTEX
#endif


#ifdef FBLOCK_SPIN
    #define FBLOCK_INIT(fb) do_spin_init(&(fb)->s, 0)
    #define FBLOCK_DESTROY(fb) do_spin_destroy(&(fb)->s)
    #define FBLOCK_LOCK(fb) do_spin_lock(&(fb)->s)
    #define FBLOCK_TRYLOCK(fb) do_spin_trylock(&(fb)->s)
    #define FBLOCK_UNLOCK(fb) do_spin_unlock(&(fb)->s)
#elif defined FBLOCK_MUTEX
    #define FBLOCK_INIT(fb) pthread_mutex_init(&(fb)->m, NULL)
    #define FBLOCK_DESTROY(fb) do_mutex_destroy(&(fb)->m)
    #define FBLOCK_LOCK(fb) do_mutex_lock(&(fb)->m)
    #define FBLOCK_TRYLOCK(fb) do_mutex_trylock(&(fb)->m)
    #define FBLOCK_UNLOCK(fb) do_mutex_unlock(&(fb)->m)
#else
    #error Enable FBLOCK_SPIN or FBLOCK_MUTEX
#endif



/* Since two or more flows can have the same hash key, we need to compare
 * the flow with the current flow key. */
#define CMP_FLOW(f1,f2) \
    (((CMP_ADDR(&(f1)->src, &(f2)->src) && \
       CMP_ADDR(&(f1)->dst, &(f2)->dst) && \
       CMP_PORT((f1)->sp, (f2)->sp) && CMP_PORT((f1)->dp, (f2)->dp)) || \
      (CMP_ADDR(&(f1)->src, &(f2)->dst) && \
       CMP_ADDR(&(f1)->dst, &(f2)->src) && \
       CMP_PORT((f1)->sp, (f2)->dp) && CMP_PORT((f1)->dp, (f2)->sp))) && \
     (f1)->proto == (f2)->proto && \
     (f1)->recursion_level == (f2)->recursion_level && \
     (f1)->vlan_id[0] == (f2)->vlan_id[0] && \
     (f1)->vlan_id[1] == (f2)->vlan_id[1])

enum {
    FLOW_PROTO_TCP = 0,
    FLOW_PROTO_UDP,
    FLOW_PROTO_ICMP,
    FLOW_PROTO_SCTP,
    FLOW_PROTO_DEFAULT,

    /* should be last */
    FLOW_PROTO_MAX,
};
/* max used in app-layer (counters) */
#define FLOW_PROTO_APPLAYER_MAX FLOW_PROTO_UDP + 1

/** flow memuse counter (atomic), for enforcing memcap limit */
ATOMIC_DECLARE(uint64_t, flow_memuse);

#endif
#endif

