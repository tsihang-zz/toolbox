#ifndef __UTIL_MPM_HS__H__
#define __UTIL_MPM_HS__H__

#if !defined(HAVE_SURICATA)

#include "mpm.h"

typedef struct SCHSPattern_ {
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
    struct SCHSPattern_ *next;
} SCHSPattern;

typedef struct SCHSCtx_ {
    /* hash used during ctx initialization */
    SCHSPattern **init_hash;

    /* pattern database and pattern arrays. */
    void *pattern_db;

    /* size of database, for accounting. */
    size_t hs_db_size;
} SCHSCtx;

typedef struct SCHSThreadCtx_ {
    /* Hyperscan scratch space region for this thread, capable of handling any
     * database that has been compiled. */
    void *scratch;

    /* size of scratch space, for accounting. */
    size_t scratch_size;
} SCHSThreadCtx;

#if defined(HAVE_HYPERSCAN)
void MpmHSRegister(void);
void MpmHSGlobalCleanup(void);
void SCHSPrintInfo(MpmCtx *mpm_ctx);
#endif

#endif
#endif
