#include "oryx.h"

#include "mpm-hs.h"

#ifdef HAVE_HYPERSCAN

#include <hs.h>


void SCHSInitCtx(MpmCtx *);
void SCHSInitThreadCtx(MpmCtx *, MpmThreadCtx *);
void SCHSDestroyCtx(MpmCtx *);
void SCHSDestroyThreadCtx(MpmCtx *, MpmThreadCtx *);
int SCHSAddPatternCI(MpmCtx *, uint8_t *, uint16_t, uint16_t, uint16_t,
                     uint32_t, sig_id, uint8_t);
int SCHSAddPatternCS(MpmCtx *, uint8_t *, uint16_t, uint16_t, uint16_t,
                     uint32_t, sig_id, uint8_t);
int SCHSPreparePatterns(MpmCtx *mpm_ctx);
uint32_t SCHSSearch(MpmCtx *mpm_ctx, MpmThreadCtx *mpm_thread_ctx,
                    PrefilterRuleStore *pmq, const uint8_t *buf, const uint16_t buflen);
void SCHSPrintInfo(MpmCtx *mpm_ctx);
void SCHSPrintSearchStats(MpmThreadCtx *mpm_thread_ctx);


/* size of the hash table used to speed up pattern insertions initially */
#define INIT_HASH_SIZE 65536

/* Initial size of the global database hash (used for de-duplication). */
#define INIT_DB_HASH_SIZE 1000

/* Global prototype scratch, built incrementally as Hyperscan databases are
 * built and then cloned for each thread context. Access is serialised via
 * g_scratch_proto_mutex. */
static hs_scratch_t *g_scratch_proto = NULL;
static os_mutex_t g_scratch_proto_mutex = INIT_MUTEX_VAL;

/* Global hash table of Hyperscan databases, used for de-duplication. Access is
 * serialised via g_db_table_mutex. */
static struct oryx_htable_t  *g_db_table = NULL;
static os_mutex_t g_db_table_mutex = INIT_MUTEX_VAL;

#define do_mutex_lock(lock)	do_mutex_lock(lock)
#define do_mutex_unlock(lock)	do_mutex_unlock(lock)
#define oryx_htable_create 		oryx_htable_init


/**
 * \internal
 * \brief Does a memcpy of the input string to lowercase.
 *
 * \param d   Pointer to the target area for memcpy.
 * \param s   Pointer to the src string for memcpy.
 * \param len len of the string sent in s.
 */
__oryx_always_extern__
void memcpy_tolower (u8 *d, u8 *s, u16 len)
{
    uint16_t i;
	
    for (i = 0; i < len; i++)
        d[i] = u8_tolower(s[i]);

    return;
}


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
static __oryx_always_inline__
uint32_t hashword (
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
  case 3 : c+=k[2];
  case 2 : b+=k[1];
  case 1 : a+=k[0];
    final(a,b,c);
  case 0:     /* case 0: nothing left to add */
    break;
  }
  /*------------------------------------------------------ report the result */
  return c;
}


/*
-------------------------------------------------------------------------------
hashlittle_safe() -- hash a variable-length key into a 32-bit value
  k       : the key (the unaligned variable-length array of bytes)
  length  : the length of the key, counting by bytes
  initval : can be any 4-byte value
Returns a 32-bit value.  Every bit of the key affects every bit of
the return value.  Two keys differing by one or two bits will have
totally different hash values.

The best hash table sizes are powers of 2.  There is no need to do
mod a prime (mod is sooo slow!).  If you need less than 32 bits,
use a bitmask.  For example, if you need only 10 bits, do
  h = (h & hashmask(10));
In which case, the hash table should have hashsize(10) elements.

If you are hashing n strings (uint8_t **)k, do it like this:
  for (i=0, h=0; i<n; ++i) h = hashlittle( k[i], len[i], h);

By Bob Jenkins, 2006.  bob_jenkins@burtleburtle.net.  You may use this
code any way you wish, private, educational, or commercial.  It's free.

This version has been modified from hashlittle() above to avoid accesses beyond
the last byte of the key, which causes warnings from Valgrind and Address
Sanitizer.

Use for hash table lookup, or anything where one collision in 2^^32 is
acceptable.  Do NOT use for cryptographic purposes.
-------------------------------------------------------------------------------
*/

static uint32_t hashlittle_safe(const void *key, size_t length, uint32_t initval)
{
  uint32_t a,b,c;                                          /* internal state */
  union { const void *ptr; size_t i; } u;     /* needed for Mac Powerbook G4 */

  /* Set up the internal state */
  a = b = c = 0xdeadbeef + ((uint32_t)length) + initval;

  u.ptr = key;
  if (HASH_LITTLE_ENDIAN && ((u.i & 0x3) == 0)) {
    const uint32_t *k = (const uint32_t *)key;         /* read 32-bit chunks */

    /*------ all but last block: aligned reads and affect 32 bits of (a,b,c) */
    while (length > 12)
    {
      a += k[0];
      b += k[1];
      c += k[2];
      mix(a,b,c);
      length -= 12;
      k += 3;
    }

    /*----------------------------- handle the last (probably partial) block */
    /*
     * Note that unlike hashlittle() above, we use the "safe" version of this
     * block that is #ifdef VALGRIND above, in order to avoid warnings from
     * Valgrind or Address Sanitizer.
     */

    const uint8_t  *k8 = (const uint8_t *)k;

    switch(length)
    {
    case 12: c+=k[2]; b+=k[1]; a+=k[0]; break;
    case 11: c+=((uint32_t)k8[10])<<16;  /* fall through */
    case 10: c+=((uint32_t)k8[9])<<8;    /* fall through */
    case 9 : c+=k8[8];                   /* fall through */
    case 8 : b+=k[1]; a+=k[0]; break;
    case 7 : b+=((uint32_t)k8[6])<<16;   /* fall through */
    case 6 : b+=((uint32_t)k8[5])<<8;    /* fall through */
    case 5 : b+=k8[4];                   /* fall through */
    case 4 : a+=k[0]; break;
    case 3 : a+=((uint32_t)k8[2])<<16;   /* fall through */
    case 2 : a+=((uint32_t)k8[1])<<8;    /* fall through */
    case 1 : a+=k8[0]; break;
    case 0 : return c;
    }
  } else if (HASH_LITTLE_ENDIAN && ((u.i & 0x1) == 0)) {
    const uint16_t *k = (const uint16_t *)key;         /* read 16-bit chunks */
    const uint8_t  *k8;

    /*--------------- all but last block: aligned reads and different mixing */
    while (length > 12)
    {
      a += k[0] + (((uint32_t)k[1])<<16);
      b += k[2] + (((uint32_t)k[3])<<16);
      c += k[4] + (((uint32_t)k[5])<<16);
      mix(a,b,c);
      length -= 12;
      k += 6;
    }

    /*----------------------------- handle the last (probably partial) block */
    k8 = (const uint8_t *)k;
    switch(length)
    {
    case 12: c+=k[4]+(((uint32_t)k[5])<<16);
             b+=k[2]+(((uint32_t)k[3])<<16);
             a+=k[0]+(((uint32_t)k[1])<<16);
             break;
    case 11: c+=((uint32_t)k8[10])<<16;     /* fall through */
    case 10: c+=k[4];
             b+=k[2]+(((uint32_t)k[3])<<16);
             a+=k[0]+(((uint32_t)k[1])<<16);
             break;
    case 9 : c+=k8[8];                      /* fall through */
    case 8 : b+=k[2]+(((uint32_t)k[3])<<16);
             a+=k[0]+(((uint32_t)k[1])<<16);
             break;
    case 7 : b+=((uint32_t)k8[6])<<16;      /* fall through */
    case 6 : b+=k[2];
             a+=k[0]+(((uint32_t)k[1])<<16);
             break;
    case 5 : b+=k8[4];                      /* fall through */
    case 4 : a+=k[0]+(((uint32_t)k[1])<<16);
             break;
    case 3 : a+=((uint32_t)k8[2])<<16;      /* fall through */
    case 2 : a+=k[0];
             break;
    case 1 : a+=k8[0];
             break;
    case 0 : return c;                     /* zero length requires no mixing */
    }

  } else {                        /* need to read the key one byte at a time */
    const uint8_t *k = (const uint8_t *)key;

    /*--------------- all but the last block: affect some 32 bits of (a,b,c) */
    while (length > 12)
    {
      a += k[0];
      a += ((uint32_t)k[1])<<8;
      a += ((uint32_t)k[2])<<16;
      a += ((uint32_t)k[3])<<24;
      b += k[4];
      b += ((uint32_t)k[5])<<8;
      b += ((uint32_t)k[6])<<16;
      b += ((uint32_t)k[7])<<24;
      c += k[8];
      c += ((uint32_t)k[9])<<8;
      c += ((uint32_t)k[10])<<16;
      c += ((uint32_t)k[11])<<24;
      mix(a,b,c);
      length -= 12;
      k += 12;
    }

    /*-------------------------------- last block: affect all 32 bits of (c) */
    switch(length)                   /* all the case statements fall through */
    {
    case 12: c+=((uint32_t)k[11])<<24;  /* fall through */
    case 11: c+=((uint32_t)k[10])<<16;  /* fall through */
    case 10: c+=((uint32_t)k[9])<<8;    /* fall through */
    case 9 : c+=k[8];                   /* fall through */
    case 8 : b+=((uint32_t)k[7])<<24;   /* fall through */
    case 7 : b+=((uint32_t)k[6])<<16;   /* fall through */
    case 6 : b+=((uint32_t)k[5])<<8;    /* fall through */
    case 5 : b+=k[4];                   /* fall through */
    case 4 : a+=((uint32_t)k[3])<<24;   /* fall through */
    case 3 : a+=((uint32_t)k[2])<<16;   /* fall through */
    case 2 : a+=((uint32_t)k[1])<<8;    /* fall through */
    case 1 : a+=k[0];
             break;
    case 0 : return c;
    }
  }

  final(a,b,c);
  return c;
}


/**
 * \internal
 * \brief Convert a pattern into a regex string accepted by the Hyperscan
 * compiler.
 *
 * For simplicity, we just take each byte of the original pattern and render it
 * with a hex escape (i.e. ' ' -> "\x20")/
 */
static char *HSRenderPattern(const uint8_t *pat, uint16_t pat_len)
{
    if (pat == NULL) {
        return NULL;
    }
    const size_t hex_len = (pat_len * 4) + 1;
    char *str = malloc(hex_len);
    if (str == NULL) {
        return NULL;
    }
    memset(str, 0, hex_len);
    char *sp = str;
    for (uint16_t i = 0; i < pat_len; i++) {
        snprintf(sp, 5, "\\x%02x", pat[i]);
        sp += 4;
    }
    *sp = '\0';
    return str;
}

/**
 * \internal
 * \brief Wraps malloc (which is a macro) so that it can be passed to
 * hs_set_allocator() for Hyperscan's use.
 */
static void *SCHSMalloc(size_t size)
{
    return malloc(size);
}

/**
 * \internal
 * \brief Wraps free (which is a macro) so that it can be passed to
 * hs_set_allocator() for Hyperscan's use.
 */
static void SCHSFree(void *ptr)
{
    free(ptr);
}

/** \brief Register Suricata malloc/free with Hyperscan.
 *
 * Requests that Hyperscan use Suricata's allocator for allocation of
 * databases, scratch space, etc.
 */
static void SCHSSetAllocators(void)
{
    hs_error_t err = hs_set_allocator(SCHSMalloc, SCHSFree);
    if (err != HS_SUCCESS) {
        printf("Failed to set Hyperscan allocator.");
        exit(EXIT_FAILURE);
    }
}

/**
 * \internal
 * \brief Creates a hash of the pattern.  We use it for the hashing process
 *        during the initial pattern insertion time, to cull duplicate sigs.
 *
 * \param pat    Pointer to the pattern.
 * \param patlen Pattern length.
 *
 * \retval hash A 32 bit unsigned hash.
 */
static __oryx_always_inline__ uint32_t SCHSInitHashRaw(uint8_t *pat, uint16_t patlen)
{
    uint32_t hash = patlen * pat[0];
    if (patlen > 1)
        hash += pat[1];

    return (hash % INIT_HASH_SIZE);
}

/**
 * \internal
 * \brief Looks up a pattern.  We use it for the hashing process during
 *        the initial pattern insertion time, to cull duplicate sigs.
 *
 * \param ctx    Pointer to the HS ctx.
 * \param pat    Pointer to the pattern.
 * \param patlen Pattern length.
 * \param flags  Flags.  We don't need this.
 *
 * \retval hash A 32 bit unsigned hash.
 */
static __oryx_always_inline__ SCHSPattern *SCHSInitHashLookup(SCHSCtx *ctx, uint8_t *pat,
                                              uint16_t patlen, uint16_t offset,
                                              uint16_t depth, char __oryx_unused_param__ flags,
                                              uint32_t pid)
{
    uint32_t hash = SCHSInitHashRaw(pat, patlen);

    if (ctx->init_hash == NULL) {
        return NULL;
    }

    SCHSPattern *t = ctx->init_hash[hash];
    for (; t != NULL; t = t->next) {
        /* Since Hyperscan uses offset/depth, we must distinguish between
         * patterns with the same ID but different offset/depth here. */
        if (t->id == pid && t->offset == offset && t->depth == depth) {
            BUG_ON(t->len != patlen);
            BUG_ON(memcmp(t->original_pat, pat, patlen) != 0);
            return t;
        }
    }

    return NULL;
}

/**
 * \internal
 * \brief Allocates a new pattern instance.
 *
 * \param mpm_ctx Pointer to the mpm context.
 *
 * \retval p Pointer to the newly created pattern.
 */
static __oryx_always_inline__ SCHSPattern *SCHSAllocPattern(MpmCtx *mpm_ctx)
{
    SCHSPattern *p = malloc(sizeof(SCHSPattern));
    if (unlikely(p == NULL)) {
        exit(EXIT_FAILURE);
    }
    memset(p, 0, sizeof(SCHSPattern));

    mpm_ctx->memory_cnt++;
    mpm_ctx->memory_size += sizeof(SCHSPattern);

    return p;
}

/**
 * \internal
 * \brief Used to free SCHSPattern instances.
 *
 * \param mpm_ctx Pointer to the mpm context.
 * \param p       Pointer to the SCHSPattern instance to be freed.
 * \param free    Free the above pointer or not.
 */
static __oryx_always_inline__ void SCHSFreePattern(MpmCtx *mpm_ctx, SCHSPattern *p)
{
    if (p != NULL && p->original_pat != NULL) {
        free(p->original_pat);
        mpm_ctx->memory_cnt--;
        mpm_ctx->memory_size -= p->len;
    }

    if (p != NULL && p->sids != NULL) {
        free(p->sids);
    }

    if (p != NULL) {
        free(p);
        mpm_ctx->memory_cnt--;
        mpm_ctx->memory_size -= sizeof(SCHSPattern);
    }
}

static __oryx_always_inline__ uint32_t SCHSInitHash(SCHSPattern *p)
{
    uint32_t hash = p->len * p->original_pat[0];
    if (p->len > 1)
        hash += p->original_pat[1];

    return (hash % INIT_HASH_SIZE);
}

static __oryx_always_inline__ int SCHSInitHashAdd(SCHSCtx *ctx, SCHSPattern *p)
{
    uint32_t hash = SCHSInitHash(p);

    if (ctx->init_hash == NULL) {
        return 0;
    }

    if (ctx->init_hash[hash] == NULL) {
        ctx->init_hash[hash] = p;
        return 0;
    }

    SCHSPattern *tt = NULL;
    SCHSPattern *t = ctx->init_hash[hash];

    /* get the list tail */
    do {
        tt = t;
        t = t->next;
    } while (t != NULL);

    tt->next = p;

    return 0;
}

/**
 * \internal
 * \brief Add a pattern to the mpm-hs context.
 *
 * \param mpm_ctx Mpm context.
 * \param pat     Pointer to the pattern.
 * \param patlen  Length of the pattern.
 * \param pid     Pattern id
 * \param sid     Signature id (internal id).
 * \param flags   Pattern's MPM_PATTERN_* flags.
 *
 * \retval  0 On success.
 * \retval -1 On failure.
 */
static int SCHSAddPattern(MpmCtx *mpm_ctx, uint8_t *pat, uint16_t patlen,
                          uint16_t offset, uint16_t depth, uint32_t pid,
                          sig_id sid, uint8_t flags)
{
    SCHSCtx *ctx = (SCHSCtx *)mpm_ctx->ctx;

    if (offset != 0) {
        flags |= MPM_PATTERN_FLAG_OFFSET;
    }
    if (depth != 0) {
        flags |= MPM_PATTERN_FLAG_DEPTH;
    }

    if (patlen == 0) {
        printf("pattern length 0\n");
        return 0;
    }

    /* check if we have already inserted this pattern */
    SCHSPattern *p =
        SCHSInitHashLookup(ctx, pat, patlen, offset, depth, flags, pid);
    if (p == NULL) {
#ifdef MPM_DEBUG
        printf ("Allocing new pattern\n");
#endif

        /* p will never be NULL */
        p = SCHSAllocPattern(mpm_ctx);

        p->len = patlen;
        p->flags = flags;
        p->id = pid;

        p->offset = offset;
        p->depth = depth;

        p->original_pat = malloc(patlen);
        if (p->original_pat == NULL)
            goto error;
        mpm_ctx->memory_cnt++;
        mpm_ctx->memory_size += patlen;
        memcpy(p->original_pat, pat, patlen);

        /* put in the pattern hash */
        SCHSInitHashAdd(ctx, p);

        mpm_ctx->pattern_cnt++;

        if (mpm_ctx->maxlen < patlen)
            mpm_ctx->maxlen = patlen;

        if (mpm_ctx->minlen == 0) {
            mpm_ctx->minlen = patlen;
        } else {
            if (mpm_ctx->minlen > patlen)
                mpm_ctx->minlen = patlen;
        }

        p->sids_size = 1;
        p->sids = malloc(p->sids_size * sizeof(sig_id));
        BUG_ON(p->sids == NULL);
        p->sids[0] = sid;
    } else {
        /* TODO figure out how we can be called multiple times for the same CTX with the same sid */

        int found = 0;
        uint32_t x = 0;
        for (x = 0; x < p->sids_size; x++) {
            if (p->sids[x] == sid) {
                found = 1;
                break;
            }
        }
        if (!found) {
            sig_id *sids = krealloc(p->sids, (sizeof(sig_id) * (p->sids_size + 1)), MPF_NOFLGS, __oryx_unused_val__);
            BUG_ON(sids == NULL);
            p->sids = sids;
            p->sids[p->sids_size] = sid;
            p->sids_size++;
        }
    }

    return 0;

error:
    SCHSFreePattern(mpm_ctx, p);
    return -1;
}

/**
 * \brief Pattern database information used only as input to the Hyperscan
 * compiler.
 */
typedef struct SCHSCompileData_ {
    unsigned int *ids;
    unsigned int *flags;
    char **expressions;
    hs_expr_ext_t **ext;
    unsigned int pattern_cnt;
} SCHSCompileData;

static SCHSCompileData *SCHSAllocCompileData(unsigned int pattern_cnt)
{
    SCHSCompileData *cd = malloc(pattern_cnt * sizeof(SCHSCompileData));
    if (cd == NULL) {
        goto error;
    }
    memset(cd, 0, pattern_cnt * sizeof(SCHSCompileData));

    cd->pattern_cnt = pattern_cnt;

    cd->ids = malloc(pattern_cnt * sizeof(unsigned int));
    if (cd->ids == NULL) {
        goto error;
    }
    memset(cd->ids, 0, pattern_cnt * sizeof(unsigned int));

    cd->flags = malloc(pattern_cnt * sizeof(unsigned int));
    if (cd->flags == NULL) {
        goto error;
    }
    memset(cd->flags, 0, pattern_cnt * sizeof(unsigned int));

    cd->expressions = malloc(pattern_cnt * sizeof(char *));
    if (cd->expressions == NULL) {
        goto error;
    }
    memset(cd->expressions, 0, pattern_cnt * sizeof(char *));

    cd->ext = malloc(pattern_cnt * sizeof(hs_expr_ext_t *));
    if (cd->ext == NULL) {
        goto error;
    }
    memset(cd->ext, 0, pattern_cnt * sizeof(hs_expr_ext_t *));

    return cd;

error:
    printf("SCHSCompileData alloc failed\n");
    if (cd) {
        free(cd->ids);
        free(cd->flags);
        free(cd->expressions);
        free(cd->ext);
        free(cd);
    }
    return NULL;
}

static void SCHSFreeCompileData(SCHSCompileData *cd)
{
    if (cd == NULL) {
        return;
    }

    free(cd->ids);
    free(cd->flags);
    if (cd->expressions) {
        for (unsigned int i = 0; i < cd->pattern_cnt; i++) {
            free(cd->expressions[i]);
        }
        free(cd->expressions);
    }
    if (cd->ext) {
        for (unsigned int i = 0; i < cd->pattern_cnt; i++) {
            free(cd->ext[i]);
        }
        free(cd->ext);
    }
    free(cd);
}

typedef struct PatternDatabase_ {
    SCHSPattern **parray;
    hs_database_t *hs_db;
    uint32_t pattern_cnt;

    /* Reference count: number of MPM contexts using this pattern database. */
    uint32_t ref_cnt;
} PatternDatabase;

static uint32_t SCHSPatternHash(const SCHSPattern *p, uint32_t hash)
{
    BUG_ON(p->original_pat == NULL);
    BUG_ON(p->sids == NULL);

    hash = hashlittle_safe(&p->len, sizeof(p->len), hash);
    hash = hashlittle_safe(&p->flags, sizeof(p->flags), hash);
    hash = hashlittle_safe(p->original_pat, p->len, hash);
    hash = hashlittle_safe(&p->id, sizeof(p->id), hash);
    hash = hashlittle_safe(&p->offset, sizeof(p->offset), hash);
    hash = hashlittle_safe(&p->depth, sizeof(p->depth), hash);
    hash = hashlittle_safe(&p->sids_size, sizeof(p->sids_size), hash);
    hash = hashlittle_safe(p->sids, p->sids_size * sizeof(sig_id), hash);
    return hash;
}

static char SCHSPatternCompare(const SCHSPattern *p1, const SCHSPattern *p2)
{
    if ((p1->len != p2->len) || (p1->flags != p2->flags) ||
        (p1->id != p2->id) || (p1->offset != p2->offset) ||
        (p1->depth != p2->depth) || (p1->sids_size != p2->sids_size)) {
        return 0;
    }

    if (memcmp(p1->original_pat, p2->original_pat, p1->len) != 0) {
        return 0;
    }

    if (memcmp(p1->sids, p2->sids, p1->sids_size * sizeof(p1->sids[0])) !=
        0) {
        return 0;
    }

    return 1;
}

static uint32_t PatternDatabaseHash(struct oryx_htable_t *ht, void *data, uint32_t __oryx_unused_param__ len)
{
    const PatternDatabase *pd = data;
    uint32_t hash = 0;
    hash = hashword(&pd->pattern_cnt, 1, hash);

    for (uint32_t i = 0; i < pd->pattern_cnt; i++) {
        hash = SCHSPatternHash(pd->parray[i], hash);
    }

    hash %= ht->array_size;
    return hash;
}

static int PatternDatabaseCompare(void *data1, uint32_t __oryx_unused_param__ len1, void *data2,
                                   uint32_t __oryx_unused_param__ len2)
{
    const PatternDatabase *pd1 = data1;
    const PatternDatabase *pd2 = data2;

    if (pd1->pattern_cnt != pd2->pattern_cnt) {
        return 0;
    }

    for (uint32_t i = 0; i < pd1->pattern_cnt; i++) {
        if (SCHSPatternCompare(pd1->parray[i], pd2->parray[i]) == 0) {
            return 1;
        }
    }

    return 0;
}

static void PatternDatabaseFree(PatternDatabase *pd)
{
    BUG_ON(pd->ref_cnt != 0);

    if (pd->parray != NULL) {
        for (uint32_t i = 0; i < pd->pattern_cnt; i++) {
            SCHSPattern *p = pd->parray[i];
            if (p != NULL) {
                free(p->original_pat);
                free(p->sids);
                free(p);
            }
        }
        free(pd->parray);
    }

    hs_free_database(pd->hs_db);

    free(pd);
}

static void PatternDatabaseTableFree(void __oryx_unused_param__*data)
{
    /* Stub function handed to hash table; actual freeing of PatternDatabase
     * structures is done in MPM destruction when the ref_cnt drops to zero. */
}

static PatternDatabase *PatternDatabaseAlloc(uint32_t pattern_cnt)
{
    PatternDatabase *pd = malloc(sizeof(PatternDatabase));
    if (pd == NULL) {
        return NULL;
    }
    memset(pd, 0, sizeof(PatternDatabase));
    pd->pattern_cnt = pattern_cnt;
    pd->ref_cnt = 0;
    pd->hs_db = NULL;

    /* alloc the pattern array */
    pd->parray =
        (SCHSPattern **)malloc(pd->pattern_cnt * sizeof(SCHSPattern *));
    if (pd->parray == NULL) {
        free(pd);
        return NULL;
    }
    memset(pd->parray, 0, pd->pattern_cnt * sizeof(SCHSPattern *));

    return pd;
}

/**
 * \brief Process the patterns added to the mpm, and create the internal tables.
 *
 * \param mpm_ctx Pointer to the mpm context.
 */
int SCHSPreparePatterns(MpmCtx *mpm_ctx)
{
    SCHSCtx *ctx = (SCHSCtx *)mpm_ctx->ctx;

    if (mpm_ctx->pattern_cnt == 0 || ctx->init_hash == NULL) {
        printf("no patterns supplied to this mpm_ctx\n");
        return 0;
    }

    hs_error_t err;
    hs_compile_error_t *compile_err = NULL;
    SCHSCompileData *cd = NULL;
    PatternDatabase *pd = NULL;

    cd = SCHSAllocCompileData(mpm_ctx->pattern_cnt);
    if (cd == NULL) {
        goto error;
    }

    pd = PatternDatabaseAlloc(mpm_ctx->pattern_cnt);
    if (pd == NULL) {
        goto error;
    }

    /* populate the pattern array with the patterns in the hash */
    for (uint32_t i = 0, p = 0; i < INIT_HASH_SIZE; i++) {
        SCHSPattern *node = ctx->init_hash[i], *nnode = NULL;
        while (node != NULL) {
            nnode = node->next;
            node->next = NULL;
            pd->parray[p++] = node;
            node = nnode;
        }
    }

    /* we no longer need the hash, so free its memory */
    free(ctx->init_hash);
    ctx->init_hash = NULL;

    /* Serialise whole database compilation as a relatively easy way to ensure
     * dedupe is safe. */
    do_mutex_lock(&g_db_table_mutex);

    /* Init global pattern database hash if necessary. */
    if (g_db_table == NULL) {
        g_db_table = oryx_htable_create(INIT_DB_HASH_SIZE, PatternDatabaseHash,
                                   PatternDatabaseCompare,
                                   PatternDatabaseTableFree, 0);
        if (g_db_table == NULL) {
            do_mutex_unlock(&g_db_table_mutex);
            goto error;
        }
    }

    /* Check global hash table to see if we've seen this pattern database
     * before, and reuse the Hyperscan database if so. */
    PatternDatabase *pd_cached = oryx_htable_lookup(g_db_table, pd, 1);

    if (pd_cached != NULL) {
        printf("Reusing cached database %p with %" PRIu32
                   " patterns (ref_cnt=%" PRIu32 ")\n",
                   pd_cached->hs_db, pd_cached->pattern_cnt,
                   pd_cached->ref_cnt);
        pd_cached->ref_cnt++;
        ctx->pattern_db = pd_cached;
        do_mutex_unlock(&g_db_table_mutex);
        PatternDatabaseFree(pd);
        SCHSFreeCompileData(cd);
        return 0;
    }

    BUG_ON(ctx->pattern_db != NULL); /* already built? */

    for (uint32_t i = 0; i < pd->pattern_cnt; i++) {
        const SCHSPattern *p = pd->parray[i];

        cd->ids[i] = i;
        cd->flags[i] = HS_FLAG_SINGLEMATCH;
        if (p->flags & MPM_PATTERN_FLAG_NOCASE) {
            cd->flags[i] |= HS_FLAG_CASELESS;
        }

        cd->expressions[i] = HSRenderPattern(p->original_pat, p->len);

        if (p->flags & (MPM_PATTERN_FLAG_OFFSET | MPM_PATTERN_FLAG_DEPTH)) {
            cd->ext[i] = malloc(sizeof(hs_expr_ext_t));
            if (cd->ext[i] == NULL) {
                do_mutex_unlock(&g_db_table_mutex);
                goto error;
            }
            memset(cd->ext[i], 0, sizeof(hs_expr_ext_t));

            if (p->flags & MPM_PATTERN_FLAG_OFFSET) {
                cd->ext[i]->flags |= HS_EXT_FLAG_MIN_OFFSET;
                cd->ext[i]->min_offset = p->offset + p->len;
            }
            if (p->flags & MPM_PATTERN_FLAG_DEPTH) {
                cd->ext[i]->flags |= HS_EXT_FLAG_MAX_OFFSET;
                cd->ext[i]->max_offset = p->offset + p->depth;
            }
        }
    }

    BUG_ON(mpm_ctx->pattern_cnt == 0);

    err = hs_compile_ext_multi((const char *const *)cd->expressions, cd->flags,
                               cd->ids, (const hs_expr_ext_t *const *)cd->ext,
                               cd->pattern_cnt, HS_MODE_BLOCK, NULL, &pd->hs_db,
                               &compile_err);

    if (err != HS_SUCCESS) {
        printf( "failed to compile hyperscan database\n");
        if (compile_err) {
            printf( "compile error: %s\n", compile_err->message);
        }
        hs_free_compile_error(compile_err);
        do_mutex_unlock(&g_db_table_mutex);
        goto error;
    }

    ctx->pattern_db = pd;

    do_mutex_lock(&g_scratch_proto_mutex);
    err = hs_alloc_scratch(pd->hs_db, &g_scratch_proto);
    do_mutex_unlock(&g_scratch_proto_mutex);
    if (err != HS_SUCCESS) {
        printf( "%s***Failed to allocate scratch%s\n", draw_color(COLOR_RED), draw_color(COLOR_FIN));
        do_mutex_unlock(&g_db_table_mutex);
        goto error;
    }

    err = hs_database_size(pd->hs_db, &ctx->hs_db_size);
    if (err != HS_SUCCESS) {
        printf( "%s***Failed to query database size%s\n", draw_color(COLOR_RED), draw_color(COLOR_FIN));
        do_mutex_unlock(&g_db_table_mutex);
        goto error;
    }

    mpm_ctx->memory_cnt++;
    mpm_ctx->memory_size += ctx->hs_db_size;

    printf("Built %" PRIu32 " patterns into a database of size %" PRIuMAX
               " bytes\n", mpm_ctx->pattern_cnt, (uintmax_t)ctx->hs_db_size);

    /* Cache this database globally for later. */
    pd->ref_cnt = 1;
    oryx_htable_add(g_db_table, pd, 1);
    do_mutex_unlock(&g_db_table_mutex);

    SCHSFreeCompileData(cd);
    return 0;

error:
    if (pd) {
        PatternDatabaseFree(pd);
    }
    if (cd) {
        SCHSFreeCompileData(cd);
    }
    return -1;
}

/**
 * \brief Init the mpm thread context.
 *
 * \param mpm_ctx        Pointer to the mpm context.
 * \param mpm_thread_ctx Pointer to the mpm thread context.
 */
void SCHSInitThreadCtx(MpmCtx __oryx_unused_param__ *mpm_ctx, MpmThreadCtx *mpm_thread_ctx)
{
    memset(mpm_thread_ctx, 0, sizeof(MpmThreadCtx));

    SCHSThreadCtx *ctx = malloc(sizeof(SCHSThreadCtx));
    if (ctx == NULL) {
        exit(EXIT_FAILURE);
    }
    mpm_thread_ctx->ctx = ctx;

    memset(ctx, 0, sizeof(SCHSThreadCtx));
    mpm_thread_ctx->memory_cnt++;
    mpm_thread_ctx->memory_size += sizeof(SCHSThreadCtx);

    ctx->scratch = NULL;
    ctx->scratch_size = 0;

    do_mutex_lock(&g_scratch_proto_mutex);

    if (g_scratch_proto == NULL) {
        /* There is no scratch prototype: this means that we have not compiled
         * any Hyperscan databases. */
        do_mutex_unlock(&g_scratch_proto_mutex);
        printf("%s***No scratch space prototype%s\n", draw_color(COLOR_RED), draw_color(COLOR_FIN));
        return;
    }

    hs_error_t err = hs_clone_scratch(g_scratch_proto,
                                      (hs_scratch_t **)&ctx->scratch);

    do_mutex_unlock(&g_scratch_proto_mutex);

    if (err != HS_SUCCESS) {
        printf( "Unable to clone scratch prototype\n");
        exit(EXIT_FAILURE);
    }

    err = hs_scratch_size(ctx->scratch, &ctx->scratch_size);
    if (err != HS_SUCCESS) {
        printf( "Unable to query scratch size\n");
        exit(EXIT_FAILURE);
    }

    mpm_thread_ctx->memory_cnt++;
    mpm_thread_ctx->memory_size += ctx->scratch_size;
}

/**
 * \brief Initialize the HS context.
 *
 * \param mpm_ctx       Mpm context.
 */
void SCHSInitCtx(MpmCtx *mpm_ctx)
{
    if (mpm_ctx->ctx != NULL)
        return;

    mpm_ctx->ctx = malloc(sizeof(SCHSCtx));
    if (mpm_ctx->ctx == NULL) {
        exit(EXIT_FAILURE);
    }
    memset(mpm_ctx->ctx, 0, sizeof(SCHSCtx));

    mpm_ctx->memory_cnt++;
    mpm_ctx->memory_size += sizeof(SCHSCtx);

    /* initialize the hash we use to speed up pattern insertions */
    SCHSCtx *ctx = (SCHSCtx *)mpm_ctx->ctx;
    ctx->init_hash = malloc(sizeof(SCHSPattern *) * INIT_HASH_SIZE);
    if (ctx->init_hash == NULL) {
        exit(EXIT_FAILURE);
    }
    memset(ctx->init_hash, 0, sizeof(SCHSPattern *) * INIT_HASH_SIZE);
}

/**
 * \brief Destroy the mpm thread context.
 *
 * \param mpm_ctx        Pointer to the mpm context.
 * \param mpm_thread_ctx Pointer to the mpm thread context.
 */
void SCHSDestroyThreadCtx(MpmCtx __oryx_unused_param__*mpm_ctx, MpmThreadCtx *mpm_thread_ctx)
{
    SCHSPrintSearchStats(mpm_thread_ctx);

    if (mpm_thread_ctx->ctx != NULL) {
        SCHSThreadCtx *thr_ctx = (SCHSThreadCtx *)mpm_thread_ctx->ctx;

        if (thr_ctx->scratch != NULL) {
            hs_free_scratch(thr_ctx->scratch);
            mpm_thread_ctx->memory_cnt--;
            mpm_thread_ctx->memory_size -= thr_ctx->scratch_size;
        }

        free(mpm_thread_ctx->ctx);
        mpm_thread_ctx->ctx = NULL;
        mpm_thread_ctx->memory_cnt--;
        mpm_thread_ctx->memory_size -= sizeof(SCHSThreadCtx);
    }
}

/**
 * \brief Destroy the mpm context.
 *
 * \param mpm_ctx Pointer to the mpm context.
 */
void SCHSDestroyCtx(MpmCtx *mpm_ctx)
{
    SCHSCtx *ctx = (SCHSCtx *)mpm_ctx->ctx;
    if (ctx == NULL)
        return;

    if (ctx->init_hash != NULL) {
        free(ctx->init_hash);
        ctx->init_hash = NULL;
        mpm_ctx->memory_cnt--;
        mpm_ctx->memory_size -= (INIT_HASH_SIZE * sizeof(SCHSPattern *));
    }

    /* Decrement pattern database ref count, and delete it entirely if the
     * count has dropped to zero. */
    do_mutex_lock(&g_db_table_mutex);
    PatternDatabase *pd = ctx->pattern_db;
    if (pd) {
        BUG_ON(pd->ref_cnt == 0);
        pd->ref_cnt--;
        if (pd->ref_cnt == 0) {
            oryx_htable_del(g_db_table, pd, 1);
            PatternDatabaseFree(pd);
        }
    }
    do_mutex_unlock(&g_db_table_mutex);

    free(mpm_ctx->ctx);
    mpm_ctx->memory_cnt--;
    mpm_ctx->memory_size -= sizeof(SCHSCtx);
}

typedef struct SCHSCallbackCtx_ {
    SCHSCtx *ctx;
    void *pmq;
    uint32_t match_count;
} SCHSCallbackCtx;

/* Hyperscan MPM match event handler */
static int SCHSMatchEvent(unsigned int id, unsigned long long __oryx_unused_param__ from,
                          unsigned long long __oryx_unused_param__ to, unsigned int __oryx_unused_param__ flags,
                          void *ctx)
{
    SCHSCallbackCtx *cctx = ctx;
    PrefilterRuleStore *pmq = cctx->pmq;
    const PatternDatabase *pd = cctx->ctx->pattern_db;
    const SCHSPattern *pat = pd->parray[id];
#if 0
    printf("Hyperscan Match %" PRIu32 ": id=%" PRIu32 " @ %" PRIuMAX
               " (pat id=%" PRIu32 ")\n",
               cctx->match_count, (uint32_t)id, (uintmax_t)to, pat->id);
#endif
    MpmAddSids(pmq, pat->sids, pat->sids_size);

    cctx->match_count++;
    return 0;
}

/**
 * \brief The Hyperscan search function.
 *
 * \param mpm_ctx        Pointer to the mpm context.
 * \param mpm_thread_ctx Pointer to the mpm thread context.
 * \param pmq            Pointer to the Pattern Matcher Queue to hold
 *                       search matches.
 * \param buf            Buffer to be searched.
 * \param buflen         Buffer length.
 *
 * \retval matches Match count.
 */
uint32_t SCHSSearch(MpmCtx *mpm_ctx, MpmThreadCtx *mpm_thread_ctx,
                    PrefilterRuleStore *pmq, const uint8_t *buf, const uint16_t buflen)
{
    uint32_t ret = 0;
    SCHSCtx *ctx = (SCHSCtx *)mpm_ctx->ctx;
    SCHSThreadCtx *hs_thread_ctx = (SCHSThreadCtx *)(mpm_thread_ctx->ctx);
    const PatternDatabase *pd = ctx->pattern_db;

    if (unlikely(buflen == 0)) {
        return 0;
    }

    SCHSCallbackCtx cctx = {
		.ctx = ctx, 
		.pmq = pmq, 
		.match_count = 0
    };

    /* scratch should have been cloned from g_scratch_proto at thread init. */
    hs_scratch_t *scratch = hs_thread_ctx->scratch;
    BUG_ON(pd->hs_db == NULL);
    BUG_ON(scratch == NULL);

    hs_error_t err = hs_scan(pd->hs_db, (const char *)buf, buflen, 0, scratch,
                             SCHSMatchEvent, &cctx);	
    if (err != HS_SUCCESS) {
        /* An error value (other than HS_SCAN_TERMINATED) from hs_scan()
         * indicates that it was passed an invalid database or scratch region,
         * which is not something we can recover from at scan time. */
        printf( "Hyperscan returned error %d\n", err);
        exit(EXIT_FAILURE);
    } else {
        ret = cctx.match_count;
    }

    return ret;
}

/**
 * \brief Add a case insensitive pattern.  Although we have different calls for
 *        adding case sensitive and insensitive patterns, we make a single call
 *        for either case.  No special treatment for either case.
 *
 * \param mpm_ctx Pointer to the mpm context.
 * \param pat     The pattern to add.
 * \param patlen  The pattern length.
 * \param offset  The pattern offset.
 * \param depth   The pattern depth.
 * \param pid     The pattern id.
 * \param sid     The pattern signature id.
 * \param flags   Flags associated with this pattern.
 *
 * \retval  0 On success.
 * \retval -1 On failure.
 */
int SCHSAddPatternCI(MpmCtx *mpm_ctx, uint8_t *pat, uint16_t patlen,
                     uint16_t offset, uint16_t depth, uint32_t pid,
                     sig_id sid, uint8_t flags)
{
    flags |= MPM_PATTERN_FLAG_NOCASE;
    return SCHSAddPattern(mpm_ctx, pat, patlen, offset, depth, pid, sid, flags);
}

/**
 * \brief Add a case sensitive pattern.  Although we have different calls for
 *        adding case sensitive and insensitive patterns, we make a single call
 *        for either case.  No special treatment for either case.
 *
 * \param mpm_ctx Pointer to the mpm context.
 * \param pat     The pattern to add.
 * \param patlen  The pattern length.
 * \param offset  The pattern offset.
 * \param depth   The pattern depth.
 * \param pid     The pattern id.
 * \param sid     The pattern signature id.
 * \param flags   Flags associated with this pattern.
 *
 * \retval  0 On success.
 * \retval -1 On failure.
 */
int SCHSAddPatternCS(MpmCtx *mpm_ctx, uint8_t *pat, uint16_t patlen,
                     uint16_t offset, uint16_t depth, uint32_t pid,
                     sig_id sid, uint8_t flags)
{
    return SCHSAddPattern(mpm_ctx, pat, patlen, offset, depth, pid, sid, flags);
}

void SCHSPrintSearchStats(MpmThreadCtx __oryx_unused_param__ *mpm_thread_ctx)
{
    return;
}

void SCHSPrintInfo(MpmCtx *mpm_ctx)
{
    SCHSCtx *ctx = (SCHSCtx *)mpm_ctx->ctx;

    printf("MPM HS Information:\n");
    printf("Memory allocs:   %" PRIu32 "\n", mpm_ctx->memory_cnt);
    printf("Memory alloced:  %" PRIu32 "\n", mpm_ctx->memory_size);
    printf(" Sizeof:\n");
    printf("  MpmCtx         %" PRIuMAX "\n", (uintmax_t)sizeof(MpmCtx));
    printf("  SCHSCtx:       %" PRIuMAX "\n", (uintmax_t)sizeof(SCHSCtx));
    printf("  SCHSPattern    %" PRIuMAX "\n", (uintmax_t)sizeof(SCHSPattern));
    printf("Unique Patterns: %" PRIu32 "\n", mpm_ctx->pattern_cnt);
    printf("Smallest:        %" PRIu32 "\n", mpm_ctx->minlen);
    printf("Largest:         %" PRIu32 "\n", mpm_ctx->maxlen);
    printf("\n");

    if (ctx) {
        PatternDatabase *pd = ctx->pattern_db;
        char *db_info = NULL;
        if (hs_database_info(pd->hs_db, &db_info) == HS_SUCCESS) {
            printf("HS Database Info: %s\n", db_info);
            free(db_info);
        }
        printf("HS Database Size: %" PRIuMAX " bytes\n",
               (uintmax_t)ctx->hs_db_size);
    }

    printf("\n");
}

/************************** Mpm Registration ***************************/

/**
 * \brief Register the Hyperscan MPM.
 */
void MpmHSRegister(void)
{
    mpm_table[MPM_HS].name = "hs";
    mpm_table[MPM_HS].InitCtx = SCHSInitCtx;
    mpm_table[MPM_HS].InitThreadCtx = SCHSInitThreadCtx;
    mpm_table[MPM_HS].DestroyCtx = SCHSDestroyCtx;
    mpm_table[MPM_HS].DestroyThreadCtx = SCHSDestroyThreadCtx;
    mpm_table[MPM_HS].AddPattern = SCHSAddPatternCS;
    mpm_table[MPM_HS].AddPatternNocase = SCHSAddPatternCI;
    mpm_table[MPM_HS].Prepare = SCHSPreparePatterns;
    mpm_table[MPM_HS].Search = SCHSSearch;
    mpm_table[MPM_HS].PrintCtx = SCHSPrintInfo;
    mpm_table[MPM_HS].PrintThreadCtx = SCHSPrintSearchStats;

    /* Set Hyperscan memory allocators */
    SCHSSetAllocators();
}

/**
 * \brief Clean up global memory used by all Hyperscan MPM instances.
 *
 * Currently, this is just the global scratch prototype.
 */
void MpmHSGlobalCleanup(void)
{
    do_mutex_lock(&g_scratch_proto_mutex);
    if (g_scratch_proto) {
        printf("Cleaning up Hyperscan global scratch\n");
        hs_free_scratch(g_scratch_proto);
        g_scratch_proto = NULL;
    }
    do_mutex_unlock(&g_scratch_proto_mutex);

    do_mutex_lock(&g_db_table_mutex);
    if (g_db_table != NULL) {
        printf("Clearing Hyperscan database cache\n");
        oryx_htable_destroy(g_db_table);
        g_db_table = NULL;
    }
    do_mutex_unlock(&g_db_table_mutex);
}

/*************************************Unittests********************************/

#endif /* HAVE_HYPERSCAN */
