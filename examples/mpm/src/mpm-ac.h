#ifndef __ORYX_MPM_AC_H__
#define __ORYX_MPM_AC_H__

#include "mpm.h"

#define SC_AC_STATE_TYPE_U16 uint16_t
#define SC_AC_STATE_TYPE_U32 uint32_t


typedef struct SCACPatternList_ {
    uint8_t *cs;
    uint16_t patlen;

    /* sid(s) for this pattern */
    uint32_t sids_size;
    sig_id *sids;
} SCACPatternList;

typedef struct SCACOutputTable_ {
    /* list of pattern sids */
    uint32_t *pids;
    /* no of entries we have in pids */
    uint32_t no_of_entries;
} SCACOutputTable;

typedef struct SCACCtx_ {
    /* pattern arrays.  We need this only during the goto table creation phase */
    MpmPattern **parray;

    /* no of states used by ac */
    uint32_t state_count;

    uint32_t pattern_id_bitarray_size;

    /* the all important memory hungry state_table */
    SC_AC_STATE_TYPE_U16 (*state_table_u16)[256];
    /* the all important memory hungry state_table */
    SC_AC_STATE_TYPE_U32 (*state_table_u32)[256];

    /* goto_table, failure table and output table.  Needed to create state_table.
     * Will be freed, once we have created the state_table */
    int32_t (*goto_table)[256];
    int32_t *failure_table;
    SCACOutputTable *output_table;
    SCACPatternList *pid_pat_list;

    /* the size of each state */
    uint32_t single_state_size;

    uint32_t allocated_state_count;

#ifdef __SC_CUDA_SUPPORT__
    CUdeviceptr state_table_u16_cuda;
    CUdeviceptr state_table_u32_cuda;
#endif /* __SC_CUDA_SUPPORT__ */
} SCACCtx;

typedef struct SCACThreadCtx_ {
    /* the total calls we make to the search function */
    uint32_t total_calls;
    /* the total patterns that we ended up matching against */
    uint64_t total_matches;
} SCACThreadCtx;

void MpmACRegister(void);


#ifdef __SC_CUDA_SUPPORT__

#define MPM_AC_CUDA_MODULE_NAME "ac_cuda"
#define MPM_AC_CUDA_MODULE_CUDA_BUFFER_NAME "ac_cuda_cb"

static __oryx_always_inline__ void CudaBufferPacket(CudaThreadVars *ctv, Packet *p)
{
    if (p->cuda_pkt_vars.cuda_mpm_enabled) {
        while (!p->cuda_pkt_vars.cuda_done) {
            oryx_sys_mutex_lock(&p->cuda_pkt_vars.cuda_mutex);
            if (p->cuda_pkt_vars.cuda_done) {
                oryx_sys_mutex_unlock(&p->cuda_pkt_vars.cuda_mutex);
                break;
            } else {
                do_cond_wait(&p->cuda_pkt_vars.cuda_cond, &p->cuda_pkt_vars.cuda_mutex);
                oryx_sys_mutex_unlock(&p->cuda_pkt_vars.cuda_mutex);
            }
        }
    }
    p->cuda_pkt_vars.cuda_done = 0;

    if (p->payload_len == 0 ||
        (p->flags & (PKT_NOPAYLOAD_INSPECTION & PKT_NOPACKET_INSPECTION)) ||
        (p->flags & PKT_ALLOC) ||
        (ctv->data_buffer_size_min_limit != 0 && p->payload_len < ctv->data_buffer_size_min_limit) ||
        (p->payload_len > ctv->data_buffer_size_max_limit && ctv->data_buffer_size_max_limit != 0) ) {
        p->cuda_pkt_vars.cuda_mpm_enabled = 0;
        return;
    }

    MpmCtx *mpm_ctx = NULL;
    if (p->proto == IPPROTO_TCP) {
        if (p->flowflags & FLOW_PKT_TOSERVER)
            mpm_ctx = ctv->mpm_proto_tcp_ctx_ts;
        else
            mpm_ctx = ctv->mpm_proto_tcp_ctx_tc;
    } else if (p->proto == IPPROTO_UDP) {
        if (p->flowflags & FLOW_PKT_TOSERVER)
            mpm_ctx = ctv->mpm_proto_udp_ctx_ts;
        else
            mpm_ctx = ctv->mpm_proto_udp_ctx_tc;
    } else {
        mpm_ctx = ctv->mpm_proto_other_ctx;
    }
    if (mpm_ctx == NULL || mpm_ctx->pattern_cnt == 0) {
        p->cuda_pkt_vars.cuda_mpm_enabled = 0;
        return;
    }

#if __WORDSIZE==64
    CudaBufferSlice *slice = CudaBufferGetSlice(ctv->cuda_ac_cb,
                                                p->payload_len + sizeof(uint64_t) + sizeof(CUdeviceptr),
                                                (void *)p);
    if (slice == NULL) {
        fprintf (stdout,  "Error retrieving slice.  Please report "
                   "this to dev.");
        p->cuda_pkt_vars.cuda_mpm_enabled = 0;
        return;
    }
    *((uint64_t *)(slice->buffer + slice->start_offset)) = p->payload_len;
    *((CUdeviceptr *)(slice->buffer + slice->start_offset + sizeof(uint64_t))) = ((SCACCtx *)(mpm_ctx->ctx))->state_table_u32_cuda;
    memcpy(slice->buffer + slice->start_offset + sizeof(uint64_t) + sizeof(CUdeviceptr), p->payload, p->payload_len);
#else
    CudaBufferSlice *slice = CudaBufferGetSlice(ctv->cuda_ac_cb,
                                                p->payload_len + sizeof(uint32_t) + sizeof(CUdeviceptr),
                                                (void *)p);
    if (slice == NULL) {
        fprintf (stdout,  "Error retrieving slice.  Please report "
                   "this to dev.");
        p->cuda_pkt_vars.cuda_mpm_enabled = 0;
        return;
    }
    *((uint32_t *)(slice->buffer + slice->start_offset)) = p->payload_len;
    *((CUdeviceptr *)(slice->buffer + slice->start_offset + sizeof(uint32_t))) = ((SCACCtx *)(mpm_ctx->ctx))->state_table_u32_cuda;
    memcpy(slice->buffer + slice->start_offset + sizeof(uint32_t) + sizeof(CUdeviceptr), p->payload, p->payload_len);
#endif
    p->cuda_pkt_vars.cuda_mpm_enabled = 1;
    SC_ATOMIC_SET(slice->done, 1);

    fprintf (stdout, "cuda ac buffering packet %p, payload_len - %"PRIu16" and deviceptr - %"PRIu64"\n",
               p, p->payload_len, (unsigned long)((SCACCtx *)(mpm_ctx->ctx))->state_table_u32_cuda);

    return;
}

void MpmACCudaRegister(void);
void SCACConstructBoth16and32StateTables(void);
int MpmCudaBufferSetup(void);
int MpmCudaBufferDeSetup(void);
void SCACCudaStartDispatcher(void);
void SCACCudaKillDispatcher(void);
uint32_t  SCACCudaPacketResultsProcessing(Packet *p, const MpmCtx *mpm_ctx,
                                          PrefilterRuleStore *pmq);
void DetermineCudaStateTableSize(DetectEngineCtx *de_ctx);

void CudaReleasePacket(Packet *p);

#endif /* __SC_CUDA_SUPPORT__ */

extern void MpmACRegister(void);

#endif
