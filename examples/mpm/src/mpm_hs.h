#ifndef __MPM_HS__H__
#define __MPM_HS__H__

#include "mpm.h"

typedef struct hs_pattern_t_ {
    /* length of the pattern */
    uint16_t len;
    /* flags describing the pattern */
    uint8_t flags;
    /* holds the original pattern that was added */
    uint8_t *original_pat;
    /* pattern id */
    uint32_t id;

    uint16_t offset;
    uint16_t depth;

    /* sid(s) for this pattern */
    uint32_t sids_size;
    sig_id *sids;

    /* only used at ctx init time, when this structure is part of a hash
     * table. */
    struct hs_pattern_t_ *next;
} hs_pattern_t;

typedef struct hs_ctx_t_ {
    /* hash used during ctx initialization */
    hs_pattern_t **init_hash;

    /* pattern database and pattern arrays. */
    void *pattern_db;

    /* size of database, for accounting. */
    size_t hs_db_size;
} hs_ctx_t;

typedef struct hs_threadctx_t_ {
    /* Hyperscan scratch space region for this thread, capable of handling any
     * database that has been compiled. */
    void *scratch;

    /* size of scratch space, for accounting. */
    size_t scratch_size;
} hs_threadctx_t;

#if defined(HAVE_HYPERSCAN)
void mpm_register_hs(void);
void mpm_cleanup_hs(void);
#endif

#endif
