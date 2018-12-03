
#ifndef __ORYX_MPM_H__
#define __ORYX_MPM_H__


#define BUILD_HYPERSCAN

#define MPM_INIT_HASH_SIZE 65536


/** type for the internal signature id. Since it's used in the matching engine
 *  extensively keeping this as small as possible reduces the overall memory
 *  footprint of the engine. Set to uint32_t if the engine needs to support
 *  more than 64k sigs. */
//#define sig_id uint16_t
#define sig_id uint32_t

/** same for pattern id's */
#define pat_id uint16_t

enum {
    MPM_NOTSET = 0,

    /* aho-corasick */
    MPM_AC,
#ifdef __SC_CUDA_SUPPORT__
    MPM_AC_CUDA,
#endif
    MPM_AC_BS,
    MPM_AC_TILE,
    MPM_HS,
    /* table size */
    MPM_TABLE_SIZE,
};

/* MPM matcher to use by default, i.e. when "mpm-algo" is set to "auto".
 * If Hyperscan is available, use it. Otherwise, use AC. */
#ifdef BUILD_HYPERSCAN
# define DEFAULT_MPM    MPM_HS
#else
# ifdef __tile__
#  define DEFAULT_MPM   MPM_AC_TILE
# else
#  define DEFAULT_MPM   MPM_AC
# endif
#endif

/* Internal Pattern Index: 0 to pattern_cnt-1 */
typedef uint32_t MpmPatternIndex;

typedef struct mpm_threadctx_t_ {
    void *ctx;

    uint32_t memory_cnt;
    uint32_t memory_size;

} mpm_threadctx_t;

/** \brief helper structure for the pattern matcher engine. The Pattern Matcher
 *         thread has this and passes a pointer to it to the pattern matcher.
 *         The actual pattern matcher will fill the structure. */
typedef struct PrefilterRuleStore_ {
    /* used for storing rule id's */

    /* Array of rule IDs found. */
    sig_id *rule_id_array;
    /* Number of rule IDs in the array. */
    uint32_t rule_id_array_cnt;
    /* The number of slots allocated for storing rule IDs */
    uint32_t rule_id_array_size;

} PrefilterRuleStore;

typedef struct mpm_pattern_t_ {
    /* length of the pattern */
    uint16_t len;
    /* flags decribing the pattern */
    uint8_t flags;
    /* holds the original pattern that was added */
    uint8_t *original_pat;
    /* case sensitive */
    uint8_t *cs;
    /* case INsensitive */
    uint8_t *ci;
    /* pattern id */
    uint32_t id;

    /* sid(s) for this pattern */
    uint32_t sids_size;
    sig_id *sids;

    struct mpm_pattern_t_ *next;
} mpm_pattern_t;

typedef struct mpm_ctx_t_ {
    void *ctx;
    uint16_t mpm_type;

    /* Indicates if this a global mpm_ctx.  Global mpm_ctx is the one that
     * is instantiated when we use "single".  Non-global is "full", i.e.
     * one per sgh.  We are using a uint16_t here to avoiding using a pad.
     * You can use a uint8_t here as well. */
    uint16_t global;

    /* unique patterns */
    uint32_t pattern_cnt;

    uint16_t minlen;
    uint16_t maxlen;

    uint32_t memory_cnt;
    uint32_t memory_size;

    uint32_t max_pat_id;

    /* hash used during ctx initialization */
    mpm_pattern_t **init_hash;
} mpm_ctx_t;

/** pattern is case insensitive */
#define MPM_PATTERN_FLAG_NOCASE     0x01
/** pattern is negated */
#define MPM_PATTERN_FLAG_NEGATED    0x02
/** pattern has a depth setting */
#define MPM_PATTERN_FLAG_DEPTH      0x04
/** pattern has an offset setting */
#define MPM_PATTERN_FLAG_OFFSET     0x08
/** one byte pattern (used in b2g) */
#define MPM_PATTERN_ONE_BYTE        0x10
/** the ctx uses it's own internal id instead of
 *  what is passed through the API */
#define MPM_PATTERN_CTX_OWNS_ID     0x20

typedef struct mpm_table_elem_t_ {
    const char *name;
    void (*ctx_init)(mpm_ctx_t *);
    void (*threadctx_init)(mpm_ctx_t *, mpm_threadctx_t *);
    void (*ctx_destroy)(mpm_ctx_t *);
    void (*threadctx_destroy)(mpm_ctx_t *, mpm_threadctx_t *);

    /** function pointers for adding patterns to the mpm ctx.
     *
     *  \param mpm_ctx Mpm context to add the pattern to
     *  \param pattern pointer to the pattern
     *  \param pattern_len length of the pattern in bytes
     *  \param offset pattern offset setting
     *  \param depth pattern depth setting
     *  \param pid pattern id
     *  \param sid signature _internal_ id
     *  \param flags pattern flags
     */
    int  (*pat_add)(mpm_ctx_t *, uint8_t *, uint16_t, uint16_t, uint16_t, uint32_t, sig_id, uint8_t);
    int  (*pat_add_nocase)(mpm_ctx_t *, uint8_t *, uint16_t, uint16_t, uint16_t, uint32_t, sig_id, uint8_t);
    int  (*pat_prepare)(mpm_ctx_t *);
    uint32_t (*pat_search)(mpm_ctx_t *, mpm_threadctx_t *, PrefilterRuleStore *, const uint8_t *, uint16_t);
    void (*Cleanup)(mpm_threadctx_t *);
    void (*ctx_print)(mpm_ctx_t *);
    void (*threadctx_print)(mpm_threadctx_t *);
    uint8_t flags;
} mpm_table_elem_t;

extern mpm_table_elem_t mpm_table[];
extern int mpm_default_matcher;

int mpm_pmq_setup(PrefilterRuleStore *);
void mpm_pmq_reset(PrefilterRuleStore *);
void mpm_pmq_cleanup(PrefilterRuleStore *);
void mpm_pmq_free(PrefilterRuleStore *);

void mpm_table_setup(void);
void mpm_ctx_init(mpm_ctx_t *mpm_ctx, uint16_t matcher);
void mpm_ctx_destroy (mpm_ctx_t *mpm_ctx);
void mpm_threadctx_init(mpm_threadctx_t *mpm_thread_ctx, uint16_t);
void mpm_threadctx_destroy(mpm_ctx_t *mpm_ctx, mpm_threadctx_t *mpm_thread_ctx);
uint32_t mpm_pattern_search(mpm_ctx_t *mpm_ctx, mpm_threadctx_t __oryx_unused__ *mpm_thread_ctx,
                    PrefilterRuleStore *pmq, const uint8_t *buf, uint16_t buflen);
int mpm_pattern_prepare(mpm_ctx_t *mpm_ctx);
int mpm_pattern_add_cs(mpm_ctx_t *mpm_ctx, uint8_t *pat, uint16_t patlen,
                    uint16_t offset, uint16_t depth,
                    uint32_t pid, sig_id sid, uint8_t flags);
int mpm_pattern_add_ci(mpm_ctx_t *mpm_ctx, uint8_t *pat, uint16_t patlen,
                    uint16_t offset, uint16_t depth,
                    uint32_t pid, sig_id sid, uint8_t flags);

void mpm_pattern_free(mpm_ctx_t *mpm_ctx, mpm_pattern_t *p);

int mpm_pattern_add(mpm_ctx_t *mpm_ctx, uint8_t *pat, uint16_t patlen,
                            uint16_t offset, uint16_t depth, uint32_t pid,
                            sig_id sid, uint8_t flags);

/** \brief Add array of Signature IDs to rule ID array.
 *
 *	 Checks size of the array first
 *
 *	\param pmq storage for match results
 *	\param new_size number of Signature IDs needing to be stored.
 *
 */
static __oryx_always_inline__
int mpm_sids_resize(PrefilterRuleStore *pmq, uint32_t new_size)
{
	/* Need to make the array bigger. Double the size needed to
	 * also handle the case that sids_size might still be
	 * larger than the old size.
	 */
	new_size = new_size * 2;
	sig_id *new_array = (sig_id*)krealloc(pmq->rule_id_array,
											   new_size * sizeof(sig_id), MPF_NOFLGS, __oryx_unused_val__);
	if (unlikely(new_array == NULL)) {
		/* Try again just big enough. */
		new_size = new_size / 2;
		new_array = (sig_id*)krealloc(pmq->rule_id_array,
										 new_size * sizeof(sig_id), MPF_NOFLGS, __oryx_unused_val__);
		if (unlikely(new_array == NULL)) {

			fprintf (stdout, "Failed to realloc PatternMatchQueue"
					   " rule ID array. Some signature ID matches lost\n");
			return 0;
		}
	}
	pmq->rule_id_array = new_array;
	pmq->rule_id_array_size = new_size;

	return new_size;
}


/** \brief Add array of Signature IDs to rule ID array.
 *
 *   Checks size of the array first. Calls mpm_sids_resize to increase
 *   The size of the array, since that is the slow path.
 *
 *  \param pmq storage for match results
 *  \param sids pointer to array of Signature IDs
 *  \param sids_size number of Signature IDs in sids array.
 *
 */
static __oryx_always_inline__
void mpm_hold_matched_sids(PrefilterRuleStore *pmq, sig_id *sids, uint32_t sids_size)
{
    if (sids_size == 0)
        return;

    uint32_t new_size = pmq->rule_id_array_cnt + sids_size;
    if (new_size > pmq->rule_id_array_size) {
        if (mpm_sids_resize(pmq, new_size) == 0) {
            // Failed to allocate larger memory for all the SIDS, but
            // keep as many as we can.
            sids_size = pmq->rule_id_array_size - pmq->rule_id_array_cnt;
        }
    }
#ifdef MPM_DEBUG
    fprintf (stdout, "Adding %u sids\n", sids_size);
#endif

    // Add SIDs for this pattern to the end of the array
    sig_id *ptr = pmq->rule_id_array + pmq->rule_id_array_cnt;
    sig_id *end = ptr + sids_size;
    do {
        *ptr++ = *sids++;
    } while (ptr != end);
    pmq->rule_id_array_cnt += sids_size;
}

#endif
