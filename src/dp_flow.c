#include "oryx.h"
#include "dp_decode.h"
#include "dp_flow.h"
#include "dp_flow_queue.h"

#if defined(HAVE_FLOW_MGR)

FlowBucket *flow_hash;
FlowConfig flow_config;

typedef struct FlowHashKey4_ {
    union {
        struct {
            uint32_t addrs[2];
            uint16_t ports[2];
            uint16_t proto; /**< u16 so proto and recur add up to u32 */
            uint16_t recur; /**< u16 so proto and recur add up to u32 */
            uint16_t vlan_id[2];
        };
        const uint32_t u32[5];
    };
} FlowHashKey4;

typedef struct FlowHashKey6_ {
    union {
        struct {
            uint32_t src[4], dst[4];
            uint16_t ports[2];
            uint16_t proto; /**< u16 so proto and recur add up to u32 */
            uint16_t recur; /**< u16 so proto and recur add up to u32 */
            uint16_t vlan_id[2];
        };
        const uint32_t u32[11];
    };
} FlowHashKey6;

#if 0
unsigned int FlowStorageSize(void)
{
    return StorageGetSize(STORAGE_FLOW);
}

void *FlowGetStorageById(Flow *f, int id)
{
    return StorageGetById((Storage *)((void *)f + sizeof(Flow)), STORAGE_FLOW, id);
}

int FlowSetStorageById(Flow *f, int id, void *ptr)
{
    return StorageSetById((Storage *)((void *)f + sizeof(Flow)), STORAGE_FLOW, id, ptr);
}

void *FlowAllocStorageById(Flow *f, int id)
{
    return StorageAllocByIdPrealloc((Storage *)((void *)f + sizeof(Flow)), STORAGE_FLOW, id);
}

void FlowFreeStorageById(Flow *f, int id)
{
    StorageFreeById((Storage *)((void *)f + sizeof(Flow)), STORAGE_FLOW, id);
}

void FlowFreeStorage(Flow *f)
{
    if (FlowStorageSize() > 0)
        StorageFreeAll((Storage *)((void *)f + sizeof(Flow)), STORAGE_FLOW);
}

int FlowStorageRegister(const char *name, const unsigned int size, void *(*Alloc)(unsigned int), void (*Free)(void *)) {
    return StorageRegister(STORAGE_FLOW, name, size, Alloc, Free);
}
#endif

/*
--------------------------------------------------------------------
 This works on all machines.  To be useful, it requires
 -- that the key be an array of uint32_t's, and
 -- that the length be the number of uint32_t's in the key

 The function hashword() is identical to hashlittle() on little-endian
 machines, and identical to hashbig() on big-endian machines,
 except that the length has to be measured in uint32_ts rather than in
 bytes.  hashlittle() is more complicated than hashword() only because
 hashlittle() has to dance around fitting the key bytes into registers.
--------------------------------------------------------------------
*/
static inline uint32_t hashword(
const uint32_t *k,                   /* the key, an array of uint32_t values */
size_t          length,               /* the length of the key, in uint32_ts */
uint32_t        initval)         /* the previous hash, or an arbitrary value */
{
  uint32_t a,b,c;

  /* Set up the internal state */
  a = b = c = 0xdeadbeef + (((uint32_t)length)<<2) + initval;

  /*------------------------------------------------- handle most of the key */
  while (length > 3)
  {
    a += k[0];
    b += k[1];
    c += k[2];
    mix(a,b,c);
    length -= 3;
    k += 3;
  }

  /*------------------------------------------- handle the last 3 uint32_t's */
  switch(length)                     /* all the case statements fall through */
  { 
  case 3 : c+=k[2]; /* fall through */
  case 2 : b+=k[1]; /* fall through */
  case 1 : a+=k[0];
    final(a,b,c);   /* fall through */
  case 0:     /* case 0: nothing left to add */
    break;
  }
  /*------------------------------------------------------ report the result */
  return c;
}

/** \brief compare two raw ipv6 addrs
 *
 *  \note we don't care about the real ipv6 ip's, this is just
 *        to consistently fill the FlowHashKey6 struct, without all
 *        the ntohl calls.
 *
 *  \warning do not use elsewhere unless you know what you're doing.
 *           detect-engine-address-ipv6.c's AddressIPv6GtU32 is likely
 *           what you are looking for.
 */
static inline int FlowHashRawAddressIPv6GtU32(const uint32_t *a, const uint32_t *b)
{
    int i;

    for (i = 0; i < 4; i++) {
        if (a[i] > b[i])
            return 1;
        if (a[i] < b[i])
            break;
    }

    return 0;
}

/** \brief allocate a flow
 *
 *  We check against the memuse counter. If it passes that check we increment
 *  the counter first, then we try to alloc.
 *
 *  \retval f the flow or NULL on out of memory
 */
Flow *FlowAlloc(void)
{
    Flow *f;
    size_t size = sizeof(Flow) + FlowStorageSize();

    if (!(FLOW_CHECK_MEMCAP(size))) {
        return NULL;
    }

    (void) SC_ATOMIC_ADD(flow_memuse, size);

    f = malloc(size);
    if (unlikely(f == NULL)) {
        (void)SC_ATOMIC_SUB(flow_memuse, size);
        return NULL;
    }
    memset(f, 0, size);

    /* coverity[missing_lock] */
    FLOW_INITIALIZE(f);
    return f;
}

/* calculate the hash key for this packet
 *
 * we're using:
 *  hash_rand -- set at init time
 *  source port
 *  destination port
 *  source address
 *  destination address
 *  recursion level -- for tunnels, make sure different tunnel layers can
 *                     never get mixed up.
 *
 *  For ICMP we only consider UNREACHABLE errors atm.
 */
static inline uint32_t FlowGetHash(const Packet *p)
{
    uint32_t hash = 0;

    if (p->ip4h != NULL) {
        if (p->tcph != NULL || p->udph != NULL) {
            FlowHashKey4 fhk;

            int ai = (p->src.addr_data32[0] > p->dst.addr_data32[0]);
            fhk.addrs[1-ai] = p->src.addr_data32[0];
            fhk.addrs[ai] = p->dst.addr_data32[0];

            const int pi = (p->sp > p->dp);
            fhk.ports[1-pi] = p->sp;
            fhk.ports[pi] = p->dp;

            fhk.proto = (uint16_t)p->proto;
            fhk.recur = (uint16_t)p->recursion_level;
            fhk.vlan_id[0] = p->vlan_id[0];
            fhk.vlan_id[1] = p->vlan_id[1];

            hash = hashword(fhk.u32, 5, flow_config.hash_rand);

        } else if (ICMPV4_DEST_UNREACH_IS_VALID(p)) {
            uint32_t psrc = IPV4_GET_RAW_IPSRC_U32(ICMPV4_GET_EMB_IPV4(p));
            uint32_t pdst = IPV4_GET_RAW_IPDST_U32(ICMPV4_GET_EMB_IPV4(p));
            FlowHashKey4 fhk;

            const int ai = (psrc > pdst);
            fhk.addrs[1-ai] = psrc;
            fhk.addrs[ai] = pdst;

            const int pi = (p->icmpv4vars.emb_sport > p->icmpv4vars.emb_dport);
            fhk.ports[1-pi] = p->icmpv4vars.emb_sport;
            fhk.ports[pi] = p->icmpv4vars.emb_dport;

            fhk.proto = (uint16_t)ICMPV4_GET_EMB_PROTO(p);
            fhk.recur = (uint16_t)p->recursion_level;
            fhk.vlan_id[0] = p->vlan_id[0];
            fhk.vlan_id[1] = p->vlan_id[1];

            hash = hashword(fhk.u32, 5, flow_config.hash_rand);

        } else {
            FlowHashKey4 fhk;
            const int ai = (p->src.addr_data32[0] > p->dst.addr_data32[0]);
            fhk.addrs[1-ai] = p->src.addr_data32[0];
            fhk.addrs[ai] = p->dst.addr_data32[0];
            fhk.ports[0] = 0xfeed;
            fhk.ports[1] = 0xbeef;
            fhk.proto = (uint16_t)p->proto;
            fhk.recur = (uint16_t)p->recursion_level;
            fhk.vlan_id[0] = p->vlan_id[0];
            fhk.vlan_id[1] = p->vlan_id[1];

            hash = hashword(fhk.u32, 5, flow_config.hash_rand);
        }
    } else if (p->ip6h != NULL) {
        FlowHashKey6 fhk;
        if (FlowHashRawAddressIPv6GtU32(p->src.addr_data32, p->dst.addr_data32)) {
            fhk.src[0] = p->src.addr_data32[0];
            fhk.src[1] = p->src.addr_data32[1];
            fhk.src[2] = p->src.addr_data32[2];
            fhk.src[3] = p->src.addr_data32[3];
            fhk.dst[0] = p->dst.addr_data32[0];
            fhk.dst[1] = p->dst.addr_data32[1];
            fhk.dst[2] = p->dst.addr_data32[2];
            fhk.dst[3] = p->dst.addr_data32[3];
        } else {
            fhk.src[0] = p->dst.addr_data32[0];
            fhk.src[1] = p->dst.addr_data32[1];
            fhk.src[2] = p->dst.addr_data32[2];
            fhk.src[3] = p->dst.addr_data32[3];
            fhk.dst[0] = p->src.addr_data32[0];
            fhk.dst[1] = p->src.addr_data32[1];
            fhk.dst[2] = p->src.addr_data32[2];
            fhk.dst[3] = p->src.addr_data32[3];
        }

        const int pi = (p->sp > p->dp);
        fhk.ports[1-pi] = p->sp;
        fhk.ports[pi] = p->dp;
        fhk.proto = (uint16_t)p->proto;
        fhk.recur = (uint16_t)p->recursion_level;
        fhk.vlan_id[0] = p->vlan_id[0];
        fhk.vlan_id[1] = p->vlan_id[1];

        hash = hashword(fhk.u32, 11, flow_config.hash_rand);
    }

    return hash;
}

/**
 *  \brief Check if we should create a flow based on a packet
 *
 *  We use this check to filter out flow creation based on:
 *  - ICMP error messages
 *
 *  \param p packet
 *  \retval 1 true
 *  \retval 0 false
 */
static inline int FlowCreateCheck(const Packet *p)
{
    if (PKT_IS_ICMPV4(p)) {
        if (ICMPV4_IS_ERROR_MSG(p)) {
            return 0;
        }
    }

    return 1;
}

static inline void FlowUpdateCounter(ThreadVars *tv, DecodeThreadVars *dtv,
        uint8_t proto)
{
#ifdef UNITTESTS
    if (tv && dtv) {
#endif
        switch (proto){
            case IPPROTO_UDP:
                StatsIncr(tv, dtv->counter_flow_udp);
                break;
            case IPPROTO_TCP:
                StatsIncr(tv, dtv->counter_flow_tcp);
                break;
            case IPPROTO_ICMP:
                StatsIncr(tv, dtv->counter_flow_icmp4);
                break;
            case IPPROTO_ICMPV6:
                StatsIncr(tv, dtv->counter_flow_icmp6);
                break;
        }
#ifdef UNITTESTS
    }
#endif
}


/**
 *  \brief Get a new flow
 *
 *  Get a new flow. We're checking memcap first and will try to make room
 *  if the memcap is reached.
 *
 *  \param tv thread vars
 *  \param dtv decode thread vars (for flow log api thread data)
 *
 *  \retval f *LOCKED* flow on succes, NULL on error.
 */
static Flow *FlowGetNew(ThreadVars *tv, DecodeThreadVars *dtv, const Packet *p)
{
    Flow *f = NULL;

    if (FlowCreateCheck(p) == 0) {
        return NULL;
    }

    /* get a flow from the spare queue */
    f = FlowDequeue(&flow_spare_q);
    if (f == NULL) {
	#if 0
        /* If we reached the max memcap, we get a used flow */
        if (!(FLOW_CHECK_MEMCAP(sizeof(Flow) + FlowStorageSize()))) {
            /* declare state of emergency */
            if (!(SC_ATOMIC_GET(flow_flags) & FLOW_EMERGENCY)) {
                SC_ATOMIC_OR(flow_flags, FLOW_EMERGENCY);

                FlowTimeoutsEmergency();

                /* under high load, waking up the flow mgr each time leads
                 * to high cpu usage. Flows are not timed out much faster if
                 * we check a 1000 times a second. */
                FlowWakeupFlowManagerThread();
            }

            f = FlowGetUsedFlow(tv, dtv);
            if (f == NULL) {
                /* max memcap reached, so increments the counter */
                if (tv != NULL && dtv != NULL) {
                    StatsIncr(tv, dtv->counter_flow_memcap);
                }

                /* very rare, but we can fail. Just giving up */
                return NULL;
            }

            /* freed a flow, but it's unlocked */
        } else
        #endif
		{
            /* now see if we can alloc a new flow */
            f = FlowAlloc();
            if (f == NULL) {
                if (tv != NULL && dtv != NULL) {
                    StatsIncr(tv, dtv->counter_flow_memcap);
                }
                return NULL;
            }

            /* flow is initialized but *unlocked* */
        }
    } else {
        /* flow has been recycled before it went into the spare queue */

        /* flow is initialized (recylced) but *unlocked* */
    }

    FLOWLOCK_WRLOCK(f);
    FlowUpdateCounter(tv, dtv, p->proto);
    return f;
}

static inline int FlowCompare(Flow *f, const Packet *p)
{
    if (p->proto == IPPROTO_ICMP) {
        return FlowCompareICMPv4(f, p);
    } else if (p->proto == IPPROTO_TCP) {
        if (CMP_FLOW(f, p) == 0)
            return 0;

        /* if this session is 'reused', we don't return it anymore,
         * so return false on the compare */
        if (f->flags & FLOW_TCP_REUSED)
            return 0;

        return 1;
    } else {
        return CMP_FLOW(f, p);
    }
}

/**
 *  \brief   Function to map the protocol to the defined FLOW_PROTO_* enumeration.
 *
 *  \param   proto  protocol which is needed to be mapped
 */

uint8_t FlowGetProtoMapping(uint8_t proto)
{
    switch (proto) {
        case IPPROTO_TCP:
            return FLOW_PROTO_TCP;
        case IPPROTO_UDP:
            return FLOW_PROTO_UDP;
        case IPPROTO_ICMP:
            return FLOW_PROTO_ICMP;
        case IPPROTO_SCTP:
            return FLOW_PROTO_SCTP;
        default:
            return FLOW_PROTO_DEFAULT;
    }
}

void FlowUpdateState(Flow *f, enum FlowState s)
{
    /* set the state */
    SC_ATOMIC_SET(f->flow_state, s);

    if (f->fb) {
        /* and reset the flow buckup next_ts value so that the flow manager
         * has to revisit this row */
        SC_ATOMIC_SET(f->fb->next_ts, 0);
    }
}

/* initialize the flow from the first packet
 * we see from it. */
void FlowInit(Flow *f, const Packet *p)
{
    oryx_logd("flow %p", f);

    f->proto = p->proto;
    f->recursion_level = p->recursion_level;
    f->vlan_id[0] = p->vlan_id[0];
    f->vlan_id[1] = p->vlan_id[1];

    if (PKT_IS_IPV4(p)) {
        FLOW_SET_IPV4_SRC_ADDR_FROM_PACKET(p, &f->src);
        FLOW_SET_IPV4_DST_ADDR_FROM_PACKET(p, &f->dst);
        f->flags |= FLOW_IPV4;
    } else if (PKT_IS_IPV6(p)) {
        FLOW_SET_IPV6_SRC_ADDR_FROM_PACKET(p, &f->src);
        FLOW_SET_IPV6_DST_ADDR_FROM_PACKET(p, &f->dst);
        f->flags |= FLOW_IPV6;
    }
#ifdef DEBUG
    /* XXX handle default */
    else {
        printf("FIXME: %s:%s:%" PRId32 "\n", __FILE__, __FUNCTION__, __LINE__);
    }
#endif

    if (p->tcph != NULL) { /* XXX MACRO */
        SET_TCP_SRC_PORT(p,&f->sp);
        SET_TCP_DST_PORT(p,&f->dp);
    } else if (p->udph != NULL) { /* XXX MACRO */
        SET_UDP_SRC_PORT(p,&f->sp);
        SET_UDP_DST_PORT(p,&f->dp);
    } else if (p->icmpv4h != NULL) {
        f->type = p->type;
        f->code = p->code;
    } else if (p->icmpv6h != NULL) {
        f->type = p->type;
        f->code = p->code;
    } else if (p->sctph != NULL) { /* XXX MACRO */
        SET_SCTP_SRC_PORT(p,&f->sp);
        SET_SCTP_DST_PORT(p,&f->dp);
    } /* XXX handle default */
#ifdef DEBUG
    else {
        printf("FIXME: %s:%s:%" PRId32 "\n", __FILE__, __FUNCTION__, __LINE__);
    }
#endif
    COPY_TIMESTAMP(&p->ts, &f->startts);

    f->protomap = FlowGetProtoMapping(f->proto);

}

/**
 *  \brief increase the use count of a flow
 *
 *  \param f flow to decrease use count for
 */
static inline void FlowIncrUsecnt(Flow *f)
{
    if (f == NULL)
        return;

    (void) SC_ATOMIC_ADD(f->use_cnt, 1);
}

/**
 *  \brief decrease the use count of a flow
 *
 *  \param f flow to decrease use count for
 */
static inline void FlowDecrUsecnt(Flow *f)
{
    if (f == NULL)
        return;

    (void) SC_ATOMIC_SUB(f->use_cnt, 1);
}

/** \brief Reference the flow, bumping the flows use_cnt
 *  \note This should only be called once for a destination
 *        pointer */
static inline void FlowReference(Flow **d, Flow *f)
{
    if (likely(f != NULL)) {
#ifdef DEBUG_VALIDATION
        BUG_ON(*d == f);
#else
        if (*d == f)
            return;
#endif
        FlowIncrUsecnt(f);
        *d = f;
    }
}


/** \brief Get Flow for packet
 *
 * Hash retrieval function for flows. Looks up the hash bucket containing the
 * flow pointer. Then compares the packet with the found flow to see if it is
 * the flow we need. If it isn't, walk the list until the right flow is found.
 *
 * If the flow is not found or the bucket was emtpy, a new flow is taken from
 * the queue. FlowDequeue() will alloc new flows as long as we stay within our
 * memcap limit.
 *
 * The p->flow pointer is updated to point to the flow.
 *
 *  \param tv thread vars
 *  \param dtv decode thread vars (for flow log api thread data)
 *
 *  \retval f *LOCKED* flow or NULL
 */
Flow *FlowGetFlowFromHash(ThreadVars *tv, DecodeThreadVars *dtv, const Packet *p, Flow **dest)
{
    Flow *f = NULL;

    /* get our hash bucket and lock it */
    const uint32_t hash = p->flow_hash;
    FlowBucket *fb = &flow_hash[hash % flow_config.hash_size];
    FBLOCK_LOCK(fb);

    oryx_logd("fb %p fb->head %p", fb, fb->head);

    /* see if the bucket already has a flow */
    if (fb->head == NULL) {
        f = FlowGetNew(tv, dtv, p);
        if (f == NULL) {
            FBLOCK_UNLOCK(fb);
            return NULL;
        }

        /* flow is locked */
        fb->head = f;
        fb->tail = f;

        /* got one, now lock, initialize and return */
        FlowInit(f, p);
        f->flow_hash = hash;
        f->fb = fb;
        FlowUpdateState(f, FLOW_STATE_NEW);

        FlowReference(dest, f);

        FBLOCK_UNLOCK(fb);
        return f;
    }

    /* ok, we have a flow in the bucket. Let's find out if it is our flow */
    f = fb->head;

    /* see if this is the flow we are looking for */
    if (FlowCompare(f, p) == 0) {
        Flow *pf = NULL; /* previous flow */

        while (f) {
            pf = f;
            f = f->hnext;

            if (f == NULL) {
                f = pf->hnext = FlowGetNew(tv, dtv, p);
                if (f == NULL) {
                    FBLOCK_UNLOCK(fb);
                    return NULL;
                }
                fb->tail = f;

                /* flow is locked */

                f->hprev = pf;

                /* initialize and return */
                FlowInit(f, p);
                f->flow_hash = hash;
                f->fb = fb;
                FlowUpdateState(f, FLOW_STATE_NEW);

                FlowReference(dest, f);

                FBLOCK_UNLOCK(fb);
                return f;
            }

            if (FlowCompare(f, p) != 0) {
                /* we found our flow, lets put it on top of the
                 * hash list -- this rewards active flows */
                if (f->hnext) {
                    f->hnext->hprev = f->hprev;
                }
                if (f->hprev) {
                    f->hprev->hnext = f->hnext;
                }
                if (f == fb->tail) {
                    fb->tail = f->hprev;
                }

                f->hnext = fb->head;
                f->hprev = NULL;
                fb->head->hprev = f;
                fb->head = f;

                /* found our flow, lock & return */
                FLOWLOCK_WRLOCK(f);
				#if 0
                if (unlikely(TcpSessionPacketSsnReuse(p, f, f->protoctx) == 1)) {
                    f = TcpReuseReplace(tv, dtv, fb, f, hash, p);
                    if (f == NULL) {
                        FBLOCK_UNLOCK(fb);
                        return NULL;
                    }
                }
				#endif

                FlowReference(dest, f);

                FBLOCK_UNLOCK(fb);
                return f;
            }
        }
    }

    /* lock & return */
    FLOWLOCK_WRLOCK(f);
#if 0
    if (unlikely(TcpSessionPacketSsnReuse(p, f, f->protoctx) == 1)) {
        f = TcpReuseReplace(tv, dtv, fb, f, hash, p);
        if (f == NULL) {
            FBLOCK_UNLOCK(fb);
            return NULL;
        }
    }
#endif
    FlowReference(dest, f);

    FBLOCK_UNLOCK(fb);
    return f;
}

/** \brief Entry point for packet flow handling
 *
 * This is called for every packet.
 *
 *  \param tv threadvars
 *  \param dtv decode thread vars (for flow output api thread data)
 *  \param p packet to handle flow for
 */
void FlowHandlePacket(ThreadVars *tv, DecodeThreadVars *dtv, Packet *p)
{
    /* Get this packet's flow from the hash. FlowHandlePacket() will setup
     * a new flow if nescesary. If we get NULL, we're out of flow memory.
     * The returned flow is locked. */
    Flow *f = FlowGetFlowFromHash(tv, dtv, p, &p->flow);
    if (f == NULL)
        return;

    /* set the flow in the packet */
    p->flags |= PKT_HAS_FLOW;
    return;
}
#endif

void FlowSetupPacket(Packet *p)
{
#if defined(HAVE_FLOW_MGR)
    p->flags |= PKT_WANTS_FLOW;
    p->flow_hash = FlowGetHash(p);
#endif
}

