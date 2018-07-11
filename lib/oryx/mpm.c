#include "oryx.h"
#include "mpm-ac.h"
#include "mpm-hs.h"

//#define MPM_DEBUG


#if 0
/**
 * \brief Register a new Mpm Context.
 *
 * \param name A new profile to be registered to store this MpmCtx.
 *
 * \retval id Return the id created for the new MpmCtx profile.
 */
int32_t MpmFactoryRegisterMpmCtxProfile(DetectEngineCtx *de_ctx, const char *name)
{
    void *ptmp;
    /* the very first entry */
    if (de_ctx->mpm_ctx_factory_container == NULL) {
        de_ctx->mpm_ctx_factory_container = kmalloc(sizeof(MpmCtxFactoryContainer));
        if (de_ctx->mpm_ctx_factory_container == NULL) {
            oryx_loge(SC_ERR_MEM_ALLOC, "Error allocating memory");
            exit(EXIT_FAILURE);
        }
        memset(de_ctx->mpm_ctx_factory_container, 0, sizeof(MpmCtxFactoryContainer));

        MpmCtxFactoryItem *item = kmalloc(sizeof(MpmCtxFactoryItem));
        if (unlikely(item == NULL)) {
            oryx_loge(SC_ERR_MEM_ALLOC, "Error allocating memory");
            exit(EXIT_FAILURE);
        }

        item[0].name = name;

        /* toserver */
        item[0].mpm_ctx_ts = kmalloc(sizeof(MpmCtx));
        if (item[0].mpm_ctx_ts == NULL) {
            oryx_loge(SC_ERR_MEM_ALLOC, "Error allocating memory");
            exit(EXIT_FAILURE);
        }
        memset(item[0].mpm_ctx_ts, 0, sizeof(MpmCtx));
        item[0].mpm_ctx_ts->global = 1;

        /* toclient */
        item[0].mpm_ctx_tc = kmalloc(sizeof(MpmCtx));
        if (item[0].mpm_ctx_tc == NULL) {
            oryx_loge(SC_ERR_MEM_ALLOC, "Error allocating memory");
            exit(EXIT_FAILURE);
        }
        memset(item[0].mpm_ctx_tc, 0, sizeof(MpmCtx));
        item[0].mpm_ctx_tc->global = 1;

        /* our id starts from 0 always.  Helps us with the ctx retrieval from
         * the array */
        item[0].id = 0;

        /* store the newly created item */
        de_ctx->mpm_ctx_factory_container->items = item;
        de_ctx->mpm_ctx_factory_container->no_of_items++;

        /* the first id is always 0 */
        return item[0].id;
    } else {
        int i;
        MpmCtxFactoryItem *items = de_ctx->mpm_ctx_factory_container->items;
        for (i = 0; i < de_ctx->mpm_ctx_factory_container->no_of_items; i++) {
            if (items[i].name != NULL && strcmp(items[i].name, name) == 0) {
                /* looks like we have this mpm_ctx freed */
                if (items[i].mpm_ctx_ts == NULL) {
                    items[i].mpm_ctx_ts = kmalloc(sizeof(MpmCtx));
                    if (items[i].mpm_ctx_ts == NULL) {
                        oryx_loge(SC_ERR_MEM_ALLOC, "Error allocating memory");
                        exit(EXIT_FAILURE);
                    }
                    memset(items[i].mpm_ctx_ts, 0, sizeof(MpmCtx));
                    items[i].mpm_ctx_ts->global = 1;
                }
                if (items[i].mpm_ctx_tc == NULL) {
                    items[i].mpm_ctx_tc = kmalloc(sizeof(MpmCtx));
                    if (items[i].mpm_ctx_tc == NULL) {
                        oryx_loge(SC_ERR_MEM_ALLOC, "Error allocating memory");
                        exit(EXIT_FAILURE);
                    }
                    memset(items[i].mpm_ctx_tc, 0, sizeof(MpmCtx));
                    items[i].mpm_ctx_tc->global = 1;
                }
                return items[i].id;
            }
        }

        /* let's make the new entry */
        ptmp = krealloc(items,
                         (de_ctx->mpm_ctx_factory_container->no_of_items + 1) * sizeof(MpmCtxFactoryItem));
        if (unlikely(ptmp == NULL)) {
            kfree(items);
            items = NULL;
            oryx_loge(SC_ERR_MEM_ALLOC, "Error allocating memory");
            exit(EXIT_FAILURE);
        }
        items = ptmp;

        de_ctx->mpm_ctx_factory_container->items = items;

        MpmCtxFactoryItem *new_item = &items[de_ctx->mpm_ctx_factory_container->no_of_items];
        new_item[0].name = name;

        /* toserver */
        new_item[0].mpm_ctx_ts = kmalloc(sizeof(MpmCtx));
        if (new_item[0].mpm_ctx_ts == NULL) {
            oryx_loge(SC_ERR_MEM_ALLOC, "Error allocating memory");
            exit(EXIT_FAILURE);
        }
        memset(new_item[0].mpm_ctx_ts, 0, sizeof(MpmCtx));
        new_item[0].mpm_ctx_ts->global = 1;

        /* toclient */
        new_item[0].mpm_ctx_tc = kmalloc(sizeof(MpmCtx));
        if (new_item[0].mpm_ctx_tc == NULL) {
            oryx_loge(SC_ERR_MEM_ALLOC, "Error allocating memory");
            exit(EXIT_FAILURE);
        }
        memset(new_item[0].mpm_ctx_tc, 0, sizeof(MpmCtx));
        new_item[0].mpm_ctx_tc->global = 1;

        new_item[0].id = de_ctx->mpm_ctx_factory_container->no_of_items;
        de_ctx->mpm_ctx_factory_container->no_of_items++;

        /* the newly created id */
        return new_item[0].id;
    }
}


int32_t MpmFactoryIsMpmCtxAvailable(const DetectEngineCtx *de_ctx, const MpmCtx *mpm_ctx)
{
    if (mpm_ctx == NULL)
        return 0;

    if (de_ctx->mpm_ctx_factory_container == NULL) {
        return 0;
    } else {
        int i;
        for (i = 0; i < de_ctx->mpm_ctx_factory_container->no_of_items; i++) {
            if (mpm_ctx == de_ctx->mpm_ctx_factory_container->items[i].mpm_ctx_ts ||
                mpm_ctx == de_ctx->mpm_ctx_factory_container->items[i].mpm_ctx_tc) {
                return 1;
            }
        }
        return 0;
    }
}

MpmCtx *MpmFactoryGetMpmCtxForProfile(const DetectEngineCtx *de_ctx, int32_t id, int direction)
{
    if (id == MPM_CTX_FACTORY_UNIQUE_CONTEXT) {
        MpmCtx *mpm_ctx = kmalloc(sizeof(MpmCtx));
        if (unlikely(mpm_ctx == NULL)) {
            oryx_loge(SC_ERR_MEM_ALLOC, "Error allocating memory");
            exit(EXIT_FAILURE);
        }
        memset(mpm_ctx, 0, sizeof(MpmCtx));
        return mpm_ctx;
    } else if (id < -1) {
        oryx_loge(ERRNO_INVALID_ARGU, "Invalid argument - %d\n", id);
        return NULL;
    } else if (id >= de_ctx->mpm_ctx_factory_container->no_of_items) {
        /* this id does not exist */
        return NULL;
    } else {
        return (direction == 0) ?
            de_ctx->mpm_ctx_factory_container->items[id].mpm_ctx_ts :
            de_ctx->mpm_ctx_factory_container->items[id].mpm_ctx_tc;
    }
}

void MpmFactoryReClaimMpmCtx(const DetectEngineCtx *de_ctx, MpmCtx *mpm_ctx)
{
    if (mpm_ctx == NULL)
        return;

    if (!MpmFactoryIsMpmCtxAvailable(de_ctx, mpm_ctx)) {
        if (mpm_ctx->mpm_type != MPM_NOTSET)
            mpm_table[mpm_ctx->mpm_type].DestroyCtx(mpm_ctx);
        kfree(mpm_ctx);
    }

    return;
}

void MpmFactoryDeRegisterAllMpmCtxProfiles(DetectEngineCtx *de_ctx)
{
    if (de_ctx->mpm_ctx_factory_container == NULL)
        return;

    int i = 0;
    MpmCtxFactoryItem *items = de_ctx->mpm_ctx_factory_container->items;
    for (i = 0; i < de_ctx->mpm_ctx_factory_container->no_of_items; i++) {
        if (items[i].mpm_ctx_ts != NULL) {
            if (items[i].mpm_ctx_ts->mpm_type != MPM_NOTSET)
                mpm_table[items[i].mpm_ctx_ts->mpm_type].DestroyCtx(items[i].mpm_ctx_ts);
            kfree(items[i].mpm_ctx_ts);
        }
        if (items[i].mpm_ctx_tc != NULL) {
            if (items[i].mpm_ctx_tc->mpm_type != MPM_NOTSET)
                mpm_table[items[i].mpm_ctx_tc->mpm_type].DestroyCtx(items[i].mpm_ctx_tc);
            kfree(items[i].mpm_ctx_tc);
        }
    }

    kfree(de_ctx->mpm_ctx_factory_container->items);
    kfree(de_ctx->mpm_ctx_factory_container);
    de_ctx->mpm_ctx_factory_container = NULL;

    return;
}
#endif

#ifdef __SC_CUDA_SUPPORT__

static void MpmCudaConfFree(void *conf)
{
    kfree(conf);
    return;
}

static void *MpmCudaConfParse(ConfNode *node)
{
    const char *value;

    MpmCudaConf *conf = kmalloc(sizeof(MpmCudaConf), MPF_CLR, __oryx_unused_val__);
    if (unlikely(conf == NULL))
        exit(EXIT_FAILURE);
    memset(conf, 0, sizeof(*conf));

    if (node != NULL)
        value = ConfNodeLookupChildValue(node, "data-buffer-size-min-limit");
    else
        value = NULL;
    if (value == NULL) {
        /* default */
        conf->data_buffer_size_min_limit = UTIL_MPM_CUDA_DATA_BUFFER_SIZE_MIN_LIMIT_DEFAULT;
    } else if (ParseSizeStringU16(value, &conf->data_buffer_size_min_limit) < 0) {
        oryx_loge(SC_ERR_INVALID_YAML_CONF_ENTRY, "Invalid entry for %s."
                   "data-buffer-size-min-limit - \"%s\"\n", node->name, value);
        exit(EXIT_FAILURE);
    }

    if (node != NULL)
        value = ConfNodeLookupChildValue(node, "data-buffer-size-max-limit");
    else
        value = NULL;
    if (value == NULL) {
        /* default */
        conf->data_buffer_size_max_limit = UTIL_MPM_CUDA_DATA_BUFFER_SIZE_MAX_LIMIT_DEFAULT;
    } else if (ParseSizeStringU16(value, &conf->data_buffer_size_max_limit) < 0) {
        printf ("Invalid entry for %s."
                   "data-buffer-size-max-limit - \"%s\"\n", node->name, value);
        exit(EXIT_FAILURE);
    }

    if (node != NULL)
        value = ConfNodeLookupChildValue(node, "cudabuffer-buffer-size");
    else
        value = NULL;
    if (value == NULL) {
        /* default */
        conf->cb_buffer_size = UTIL_MPM_CUDA_CUDA_BUFFER_DBUFFER_SIZE_DEFAULT;
    } else if (ParseSizeStringU32(value, &conf->cb_buffer_size) < 0) {
        printf ("Invalid entry for %s."
                   "cb-buffer-size - \"%s\"\n", node->name, value);
        exit(EXIT_FAILURE);
    }

    if (node != NULL)
        value = ConfNodeLookupChildValue(node, "gpu-transfer-size");
    else
        value = NULL;
    if (value == NULL) {
        /* default */
        conf->gpu_transfer_size = UTIL_MPM_CUDA_GPU_TRANSFER_SIZE;
    } else if (ParseSizeStringU32(value, &conf->gpu_transfer_size) < 0) {
        printf ("Invalid entry for %s."
                   "gpu-transfer-size - \"%s\"\n", node->name, value);
        exit(EXIT_FAILURE);
    }

    if (node != NULL)
        value = ConfNodeLookupChildValue(node, "batching-timeout");
    else
        value = NULL;
    if (value == NULL) {
        /* default */
        conf->batching_timeout = UTIL_MPM_CUDA_BATCHING_TIMEOUT_DEFAULT;
    } else if ((conf->batching_timeout = atoi(value)) < 0) {
        printf ("Invalid entry for %s."
                   "batching-timeout - \"%s\"\n", node->name, value);
        exit(EXIT_FAILURE);
    }

    if (node != NULL)
        value = ConfNodeLookupChildValue(node, "device-id");
    else
        value = NULL;
    if (value == NULL) {
        /* default */
        conf->device_id = UTIL_MPM_CUDA_DEVICE_ID_DEFAULT;
    } else if ((conf->device_id = atoi(value)) < 0) {
        printf ("Invalid entry for %s."
                   "device-id - \"%s\"\n", node->name, value);
        exit(EXIT_FAILURE);
    }

    if (node != NULL)
        value = ConfNodeLookupChildValue(node, "cuda-streams");
    else
        value = NULL;
    if (value == NULL) {
        /* default */
        conf->cuda_streams = UTIL_MPM_CUDA_CUDA_STREAMS_DEFAULT;
    } else if ((conf->cuda_streams = atoi(value)) < 0) {
        printf ("Invalid entry for %s."
                   "cuda-streams - \"%s\"\n", node->name, value);
        exit(EXIT_FAILURE);
    }

    return conf;
}

void MpmCudaEnvironmentSetup()
{
    if (PatternMatchDefaultMatcher() != MPM_AC_CUDA)
        return;

    CudaHandlerAddCudaProfileFromConf("mpm", MpmCudaConfParse, MpmCudaConfFree);

    MpmCudaConf *conf = CudaHandlerGetCudaProfile("mpm");
    if (conf == NULL) {
        printf ("Error obtaining cuda mpm "
                       "profile.\n");
        exit(EXIT_FAILURE);
    }

    if (MpmCudaBufferSetup() < 0) {
        printf ("Error setting up env for ac "
                   "cuda\n");
        exit(EXIT_FAILURE);
    }

    return;
}

#endif

/**
 *  \brief Setup a pmq
 *
 *  \param pmq Pattern matcher queue to be initialized
 *
 *  \retval -1 error
 *  \retval 0 ok
 */
int PmqSetup(PrefilterRuleStore *pmq)
{
    SCEnter();

    if (pmq == NULL) {
        SCReturnInt(-1);
    }

    memset(pmq, 0, sizeof(PrefilterRuleStore));

    pmq->rule_id_array_size = 128; /* Initial size, TODO: Make configure option. */
    pmq->rule_id_array_cnt = 0;

    size_t bytes = pmq->rule_id_array_size * sizeof(sig_id);
    pmq->rule_id_array = (sig_id*)kmalloc(bytes, MPF_CLR, __oryx_unused_val__);
    if (pmq->rule_id_array == NULL) {
        pmq->rule_id_array_size = 0;
        SCReturnInt(-1);
    }
    // Don't need to zero memory since it is always written first.

    SCReturnInt(0);
}

/** \brief Add array of Signature IDs to rule ID array.
 *
 *   Checks size of the array first
 *
 *  \param pmq storage for match results
 *  \param new_size number of Signature IDs needing to be stored.
 *
 */
int
MpmAddSidsResize(PrefilterRuleStore *pmq, uint32_t new_size)
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

            printf ("Failed to realloc PatternMatchQueue"
                       " rule ID array. Some signature ID matches lost\n");
            return 0;
        }
    }
    pmq->rule_id_array = new_array;
    pmq->rule_id_array_size = new_size;

    return new_size;
}

/** \brief Reset a Pmq for reusage. Meant to be called after a single search.
 *  \param pmq Pattern matcher to be reset.
 *  \todo memset is expensive, but we need it as we merge pmq's. We might use
 *        a flag so we can clear pmq's the old way if we can.
 */
void PmqReset(PrefilterRuleStore *pmq)
{
    if (pmq == NULL)
        return;

    pmq->rule_id_array_cnt = 0;
    /* TODO: Realloc the rule id array smaller at some size? */
}

/** \brief Cleanup a Pmq
  * \param pmq Pattern matcher queue to be cleaned up.
  */
void PmqCleanup(PrefilterRuleStore *pmq)
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
void PmqFree(PrefilterRuleStore *pmq)
{
    if (pmq == NULL)
        return;

    PmqCleanup(pmq);
}

MpmTableElmt mpm_table[MPM_TABLE_SIZE];

void MpmInitThreadCtx(MpmThreadCtx *mpm_thread_ctx, uint16_t matcher)
{
    mpm_table[matcher].InitThreadCtx(NULL, mpm_thread_ctx);
}

void MpmInitCtx (MpmCtx *mpm_ctx, uint16_t matcher)
{
    mpm_ctx->mpm_type = matcher;
    mpm_table[matcher].InitCtx(mpm_ctx);
}

void MpmDestroyThreadCtx(MpmCtx *mpm_ctx, MpmThreadCtx *mpm_thread_ctx)
{
    mpm_table[mpm_ctx->mpm_type].DestroyThreadCtx(NULL, mpm_thread_ctx);
}

void MpmDestroyCtx (MpmCtx *mpm_ctx)
{
    mpm_table[mpm_ctx->mpm_type].DestroyCtx(mpm_ctx);
}


uint32_t MpmSearch(MpmCtx *mpm_ctx, MpmThreadCtx __oryx_unused_param__ *mpm_thread_ctx,
                    PrefilterRuleStore *pmq, const uint8_t *buf, uint16_t buflen)
{
	return mpm_table[mpm_ctx->mpm_type].Search(mpm_ctx, mpm_thread_ctx, pmq, buf, buflen);
}

int MpmPreparePatterns(MpmCtx *mpm_ctx)
{
	return mpm_table[mpm_ctx->mpm_type].Prepare(mpm_ctx);
}

int MpmAddPatternCS(struct MpmCtx_ *mpm_ctx, uint8_t *pat, uint16_t patlen,
                    uint16_t offset, uint16_t depth,
                    uint32_t pid, sig_id sid, uint8_t flags)
{
    return mpm_table[mpm_ctx->mpm_type].AddPattern(mpm_ctx, pat, patlen,
                                                   offset, depth,
                                                   pid, sid, flags);
}

int MpmAddPatternCI(struct MpmCtx_ *mpm_ctx, uint8_t *pat, uint16_t patlen,
                    uint16_t offset, uint16_t depth,
                    uint32_t pid, sig_id sid, uint8_t flags)
{
    return mpm_table[mpm_ctx->mpm_type].AddPatternNocase(mpm_ctx, pat, patlen,
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
static __oryx_always_inline__ MpmPattern *MpmInitHashLookup(MpmCtx *ctx, uint8_t *pat,
                                                  uint16_t patlen, char flags,
                                                  uint32_t pid)
{
    uint32_t hash = MpmInitHashRaw(pat, patlen);

    if (ctx->init_hash == NULL) {
        return NULL;
    }

    MpmPattern *t = ctx->init_hash[hash];
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

/**
 * \internal
 * \brief Allocs a new pattern instance.
 *
 * \param mpm_ctx Pointer to the mpm context.
 *
 * \retval p Pointer to the newly created pattern.
 */
static __oryx_always_inline__ MpmPattern *MpmAllocPattern(MpmCtx *mpm_ctx)
{
    MpmPattern *p = kmalloc(sizeof(MpmPattern), MPF_CLR, __oryx_unused_val__);
    if (unlikely(p == NULL)) {
        exit(EXIT_FAILURE);
    }

    mpm_ctx->memory_cnt++;
    mpm_ctx->memory_size += sizeof(MpmPattern);

    return p;
}

/**
 * \internal
 * \brief Used to free MpmPattern instances.
 *
 * \param mpm_ctx Pointer to the mpm context.
 * \param p       Pointer to the MpmPattern instance to be freed.
 */
void MpmFreePattern(MpmCtx *mpm_ctx, MpmPattern *p)
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
        mpm_ctx->memory_size -= sizeof(MpmPattern);
    }
    return;
}

static __oryx_always_inline__ uint32_t MpmInitHash(MpmPattern *p)
{
    uint32_t hash = p->len * p->original_pat[0];
    if (p->len > 1)
        hash += p->original_pat[1];

    return (hash % MPM_INIT_HASH_SIZE);
}

static __oryx_always_inline__ int MpmInitHashAdd(MpmCtx *ctx, MpmPattern *p)
{
    uint32_t hash = MpmInitHash(p);

    if (ctx->init_hash == NULL) {
        return 0;
    }

    if (ctx->init_hash[hash] == NULL) {
        ctx->init_hash[hash] = p;
        return 0;
    }

    MpmPattern *tt = NULL;
    MpmPattern *t = ctx->init_hash[hash];

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
int MpmAddPattern(MpmCtx *mpm_ctx, uint8_t *pat, uint16_t patlen,
                            uint16_t __oryx_unused_param__ offset, uint16_t __oryx_unused_param__ depth, uint32_t pid,
                            sig_id sid, uint8_t flags)
{
#ifdef MPM_DEBUG
    printf("Adding pattern for ctx %p, patlen %u and pid %u\n",
               mpm_ctx, patlen, pid);
#endif

    if (patlen == 0) {
#ifdef MPM_DEBUG
        printf ("pattern length 0\n");
#endif
        return 0;
    }

    if (flags & MPM_PATTERN_CTX_OWNS_ID)
        pid = UINT_MAX;

    /* check if we have already inserted this pattern */
    MpmPattern *p = MpmInitHashLookup(mpm_ctx, pat, patlen, flags, pid);
    if (p == NULL) {
#ifdef MPM_DEBUG
        printf ("Allocing new pattern\n");
#endif
        /* p will never be NULL */
        p = MpmAllocPattern(mpm_ctx);

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
	printf ("************** find ? ... %d ->%s\n", found, pat);
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
    MpmFreePattern(mpm_ctx, p);
    return -1;
}


/************************************Unittests*********************************/

#ifdef UNITTESTS
#endif /* UNITTESTS */

void MpmRegisterTests(void)
{
#ifdef UNITTESTS
    uint16_t i;

    for (i = 0; i < MPM_TABLE_SIZE; i++) {
        if (i == MPM_NOTSET)
            continue;

        g_ut_modules++;

        if (mpm_table[i].RegisterUnittests != NULL) {
            g_ut_covered++;
            mpm_table[i].RegisterUnittests();
        } else {
            if (coverage_unittests)
                printf ("mpm module %s has no "
                        "unittest registration function.", mpm_table[i].name);
        }
    }

#endif
}

int mpm_default_matcher = MPM_HS;

void MpmTableSetup(void)
{
	memset(mpm_table, 0, sizeof(mpm_table));

	MpmACRegister();

#ifdef HAVE_HYPERSCAN
    #ifdef HAVE_HS_VALID_PLATFORM
    /* Enable runtime check for SSSE3. Do not use Hyperscan MPM matcher if
     * check is not successful. */
        if (hs_valid_platform() != HS_SUCCESS) {
            printf("SSSE3 Support not detected, disabling Hyperscan for "
                      "MPM\n");
            /* Fall back to best Aho-Corasick variant. */
            mpm_default_matcher = MPM_AC;
        } else {
            MpmHSRegister();
        }
    #else
        MpmHSRegister();
    #endif /* HAVE_HS_VALID_PLATFORM */
#endif /* HAVE_HYPERSCAN */

#if 0
    MpmACBSRegister();
    MpmACTileRegister();
#ifdef HAVE_HYPERSCAN
    MpmHSRegister();
#endif /* HAVE_HYPERSCAN */
#ifdef __SC_CUDA_SUPPORT__
    MpmACCudaRegister();
#endif /* __SC_CUDA_SUPPORT__ */
#endif

	mpm_default_matcher = MPM_AC;

	int i;
	for (i = MPM_NOTSET; i < (int)MPM_TABLE_SIZE; i ++) {
		MpmTableElmt *t = &mpm_table[i];
		if (t->name)
			printf ("MPM--> %s\n", t->name);
	}

//SCACRegisterTests ();
//SCHSRegisterTests ();
}

