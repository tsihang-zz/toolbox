#include "oryx.h"
#include "mpm_ac.h"
#include "mpm_hs.h"

//#define MPM_DEBUG

mpm_table_elem_t mpm_table[MPM_TABLE_SIZE];

/**
 *  \brief Setup a pmq
 *
 *  \param pmq Pattern matcher queue to be initialized
 *
 *  \retval -1 error
 *  \retval 0 ok
 */
int mpm_pmq_setup(PrefilterRuleStore *pmq)
{
    if (pmq == NULL) {
        return -1;
    }

    memset(pmq, 0, sizeof(PrefilterRuleStore));

    pmq->rule_id_array_size = 128; /* Initial size, TODO: Make configure option. */
    pmq->rule_id_array_cnt = 0;

    size_t bytes = pmq->rule_id_array_size * sizeof(sig_id);
    pmq->rule_id_array = (sig_id*)kmalloc(bytes, MPF_CLR, __oryx_unused_val__);
    if (pmq->rule_id_array == NULL) {
        pmq->rule_id_array_size = 0;
        return -1;
    }
    // Don't need to zero memory since it is always written first.

    return 0;
}

/** \brief Reset a Pmq for reusage. Meant to be called after a single search.
 *  \param pmq Pattern matcher to be reset.
 *  \todo memset is expensive, but we need it as we merge pmq's. We might use
 *        a flag so we can clear pmq's the old way if we can.
 */
void mpm_pmq_reset(PrefilterRuleStore *pmq)
{
    if (pmq == NULL)
        return;

    pmq->rule_id_array_cnt = 0;
    /* TODO: Realloc the rule id array smaller at some size? */
}

/** \brief Cleanup a Pmq
  * \param pmq Pattern matcher queue to be cleaned up.
  */
void mpm_pmq_cleanup(PrefilterRuleStore *pmq)
{
    if (pmq == NULL)
        return;
    if (pmq->rule_id_array != NULL) {
        kfree(pmq->rule_id_array);
        pmq->rule_id_array = NULL;
    }
}

/** \brief Cleanup and free a Pmq
  * \param pmq Pattern matcher queue to be free'd.
  */
void mpm_pmq_free(PrefilterRuleStore *pmq)
{
    if (pmq == NULL)
        return;

    mpm_pmq_cleanup(pmq);
}

void mpm_ctx_init (mpm_ctx_t *mpm_ctx, uint16_t matcher)
{
    mpm_ctx->mpm_type = matcher;
    mpm_table[matcher].ctx_init(mpm_ctx);
}

void mpm_ctx_destroy (mpm_ctx_t *mpm_ctx)
{
    mpm_table[mpm_ctx->mpm_type].ctx_destroy(mpm_ctx);
}

void mpm_threadctx_init(mpm_threadctx_t *mpm_thread_ctx, uint16_t matcher)
{
    mpm_table[matcher].threadctx_init(NULL, mpm_thread_ctx);
}

void mpm_threadctx_destroy(mpm_ctx_t *mpm_ctx, mpm_threadctx_t *mpm_thread_ctx)
{
    mpm_table[mpm_ctx->mpm_type].threadctx_destroy(NULL, mpm_thread_ctx);
}

uint32_t mpm_pattern_search(mpm_ctx_t *mpm_ctx, mpm_threadctx_t __oryx_unused_param__ *mpm_thread_ctx,
                    PrefilterRuleStore *pmq, const uint8_t *buf, uint16_t buflen)
{
	return mpm_table[mpm_ctx->mpm_type].pat_search(mpm_ctx, mpm_thread_ctx, pmq, buf, buflen);
}

int mpm_pattern_prepare(mpm_ctx_t *mpm_ctx)
{
	return mpm_table[mpm_ctx->mpm_type].pat_prepare(mpm_ctx);
}

int mpm_pattern_add_cs(mpm_ctx_t *mpm_ctx, uint8_t *pat, uint16_t patlen,
                    uint16_t offset, uint16_t depth,
                    uint32_t pid, sig_id sid, uint8_t flags)
{
    return mpm_table[mpm_ctx->mpm_type].pat_add(mpm_ctx, pat, patlen,
                                                   offset, depth,
                                                   pid, sid, flags);
}

int mpm_pattern_add_ci(mpm_ctx_t *mpm_ctx, uint8_t *pat, uint16_t patlen,
                    uint16_t offset, uint16_t depth,
                    uint32_t pid, sig_id sid, uint8_t flags)
{
    return mpm_table[mpm_ctx->mpm_type].pat_add_nocase(mpm_ctx, pat, patlen,
                                                         offset, depth,
                                                         pid, sid, flags);
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
static __oryx_always_inline__ uint32_t MpmInitHashRaw(uint8_t *pat, uint16_t patlen)
{
    uint32_t hash = patlen * pat[0];
    if (patlen > 1)
        hash += pat[1];

    return (hash % MPM_INIT_HASH_SIZE);
}

/**
 * \internal
 * \brief Looks up a pattern.  We use it for the hashing process during the
 *        the initial pattern insertion time, to cull duplicate sigs.
 *
 * \param ctx    Pointer to the AC ctx.
 * \param pat    Pointer to the pattern.
 * \param patlen Pattern length.
 * \param flags  Flags.  We don't need this.
 *
 * \retval hash A 32 bit unsigned hash.
 */
static __oryx_always_inline__ mpm_pattern_t *MpmInitHashLookup(mpm_ctx_t *ctx, uint8_t *pat,
                                                  uint16_t patlen, char flags,
                                                  uint32_t pid)
{
    uint32_t hash = MpmInitHashRaw(pat, patlen);

    if (ctx->init_hash == NULL) {
        return NULL;
    }

    mpm_pattern_t *t = ctx->init_hash[hash];
    for ( ; t != NULL; t = t->next) {
        if (!(flags & MPM_PATTERN_CTX_OWNS_ID)) {
            if (t->id == pid)
                return t;
        } else {
            if (t->len == patlen &&
                    memcmp(pat, t->original_pat, patlen) == 0 &&
                    t->flags == flags)
            {
                return t;
            }
        }
    }

    return NULL;
}
												  
static __oryx_always_inline__ uint32_t MpmInitHash(mpm_pattern_t *p)
{
  uint32_t hash = p->len * p->original_pat[0];
  if (p->len > 1)
	  hash += p->original_pat[1];

  return (hash % MPM_INIT_HASH_SIZE);
}

static __oryx_always_inline__ int MpmInitHashAdd(mpm_ctx_t *ctx, mpm_pattern_t *p)
{
    uint32_t hash = MpmInitHash(p);

    if (ctx->init_hash == NULL) {
        return 0;
    }

    if (ctx->init_hash[hash] == NULL) {
        ctx->init_hash[hash] = p;
        return 0;
    }

    mpm_pattern_t *tt = NULL;
    mpm_pattern_t *t = ctx->init_hash[hash];

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
 * \brief Allocs a new pattern instance.
 *
 * \param mpm_ctx Pointer to the mpm context.
 *
 * \retval p Pointer to the newly created pattern.
 */
static __oryx_always_inline__ mpm_pattern_t *mpm_pattern_alloc(mpm_ctx_t *mpm_ctx)
{
    mpm_pattern_t *p = kmalloc(sizeof(mpm_pattern_t), MPF_CLR, __oryx_unused_val__);
    if (unlikely(p == NULL)) {
        exit(EXIT_FAILURE);
    }

    mpm_ctx->memory_cnt++;
    mpm_ctx->memory_size += sizeof(mpm_pattern_t);

    return p;
}

/**
 * \internal
 * \brief Used to free mpm_pattern_t instances.
 *
 * \param mpm_ctx Pointer to the mpm context.
 * \param p       Pointer to the mpm_pattern_t instance to be freed.
 */
void mpm_pattern_free(mpm_ctx_t *mpm_ctx, mpm_pattern_t *p)
{
    if (p != NULL && p->cs != NULL && p->cs != p->ci) {
        kfree(p->cs);
        mpm_ctx->memory_cnt--;
        mpm_ctx->memory_size -= p->len;
    }

    if (p != NULL && p->ci != NULL) {
        kfree(p->ci);
        mpm_ctx->memory_cnt--;
        mpm_ctx->memory_size -= p->len;
    }

    if (p != NULL && p->original_pat != NULL) {
        kfree(p->original_pat);
        mpm_ctx->memory_cnt--;
        mpm_ctx->memory_size -= p->len;
    }

    if (p != NULL) {
        kfree(p);
        mpm_ctx->memory_cnt--;
        mpm_ctx->memory_size -= sizeof(mpm_pattern_t);
    }
    return;
}

/**
 * \internal
 * \brief Add a pattern to the mpm-ac context.
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
int mpm_pattern_add(mpm_ctx_t *mpm_ctx, uint8_t *pat, uint16_t patlen,
                            uint16_t __oryx_unused_param__ offset, uint16_t __oryx_unused_param__ depth, uint32_t pid,
                            sig_id sid, uint8_t flags)
{
#ifdef MPM_DEBUG
    fprintf (stdout, "Adding pattern for ctx %p, patlen %u and pid %u\n",
               mpm_ctx, patlen, pid);
#endif

    if (patlen == 0) {
#ifdef MPM_DEBUG
        fprintf (stdout, "pattern length 0\n");
#endif
        return 0;
    }

    if (flags & MPM_PATTERN_CTX_OWNS_ID)
        pid = UINT_MAX;

    /* check if we have already inserted this pattern */
    mpm_pattern_t *p = MpmInitHashLookup(mpm_ctx, pat, patlen, flags, pid);
    if (p == NULL) {
#ifdef MPM_DEBUG
        fprintf (stdout, "Allocing new pattern\n");
#endif
        /* p will never be NULL */
        p = mpm_pattern_alloc(mpm_ctx);

        p->len = patlen;
        p->flags = flags;
        if (flags & MPM_PATTERN_CTX_OWNS_ID)
            p->id = mpm_ctx->max_pat_id++;
        else
            p->id = pid;

        p->original_pat = kmalloc(patlen, MPF_CLR, __oryx_unused_val__);
        if (p->original_pat == NULL)
            goto error;
        mpm_ctx->memory_cnt++;
        mpm_ctx->memory_size += patlen;
        memcpy(p->original_pat, pat, patlen);

        p->ci = kmalloc(patlen, MPF_CLR, __oryx_unused_val__);
        if (p->ci == NULL)
            goto error;
        mpm_ctx->memory_cnt++;
        mpm_ctx->memory_size += patlen;
        memcpy_tolower(p->ci, pat, patlen);

        /* setup the case sensitive part of the pattern */
        if (p->flags & MPM_PATTERN_FLAG_NOCASE) {
            /* nocase means no difference between cs and ci */
            p->cs = p->ci;
        } else {
            if (memcmp(p->ci, pat, p->len) == 0) {
                /* no diff between cs and ci: pat is lowercase */
                p->cs = p->ci;
            } else {
                p->cs = kmalloc(patlen, MPF_CLR, __oryx_unused_val__);
                if (p->cs == NULL)
                    goto error;
                mpm_ctx->memory_cnt++;
                mpm_ctx->memory_size += patlen;
                memcpy(p->cs, pat, patlen);
            }
        }

        /* put in the pattern hash */
        MpmInitHashAdd(mpm_ctx, p);

        mpm_ctx->pattern_cnt++;

        if (mpm_ctx->maxlen < patlen)
            mpm_ctx->maxlen = patlen;

        if (mpm_ctx->minlen == 0) {
            mpm_ctx->minlen = patlen;
        } else {
            if (mpm_ctx->minlen > patlen)
                mpm_ctx->minlen = patlen;
        }

        /* we need the max pat id */
        if (p->id > mpm_ctx->max_pat_id)
            mpm_ctx->max_pat_id = p->id;

        p->sids_size = 1;
        p->sids = kmalloc(p->sids_size * sizeof(sig_id), MPF_CLR, __oryx_unused_val__);
        BUG_ON(p->sids == NULL);
        p->sids[0] = sid;
    } else {
        /* we can be called multiple times for the same sid in the case
         * of the 'single' modus. Here multiple rule groups share the
         * same mpm ctx and might be adding the same pattern to the
         * mpm_ctx */
        int found = 0;
        uint32_t x = 0;
        for (x = 0; x < p->sids_size; x++) {
            if (p->sids[x] == sid) {
                found = 1;
                break;
            }
        }
	fprintf (stdout, "************** find ? ... %d ->%s\n", found, pat);
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
    mpm_pattern_free(mpm_ctx, p);
    return -1;
}

int mpm_default_matcher = MPM_HS;

void mpm_table_setup(void)
{
	memset(mpm_table, 0, sizeof(mpm_table));

	mpm_register_ac();

#ifdef HAVE_HYPERSCAN
    #ifdef HAVE_HS_VALID_PLATFORM
    /* Enable runtime check for SSSE3. Do not use Hyperscan MPM matcher if
     * check is not successful. */
        if (hs_valid_platform() != HS_SUCCESS) {
            fprintf (stdout, "SSSE3 Support not detected, disabling Hyperscan for "
                      "MPM\n");
            /* Fall back to best Aho-Corasick variant. */
            mpm_default_matcher = MPM_AC;
        } else {
            mpm_register_hs();
        }
    #else
        mpm_register_hs();
    #endif /* HAVE_HS_VALID_PLATFORM */
#endif /* HAVE_HYPERSCAN */

	mpm_default_matcher = MPM_AC;

	int i;
	for (i = MPM_NOTSET; i < (int)MPM_TABLE_SIZE; i ++) {
		mpm_table_elem_t *t = &mpm_table[i];
		if (t->name)
			fprintf (stdout, "MPM--> %s\n", t->name);
	}

}

