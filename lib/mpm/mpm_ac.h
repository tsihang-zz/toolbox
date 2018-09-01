#ifndef __ORYX_MPM_AC_H__
#define __ORYX_MPM_AC_H__

#include "mpm.h"

#define AC_STATE_TYPE_U16 uint16_t
#define AC_STATE_TYPE_U32 uint32_t


typedef struct ac_pattern_list_t_ {
    uint8_t *cs;
    uint16_t patlen;

    /* sid(s) for this pattern */
    uint32_t sids_size;
    sig_id *sids;
} ac_pattern_list_t;

typedef struct ac_output_table_t_ {
    /* list of pattern sids */
    uint32_t *pids;
    /* no of entries we have in pids */
    uint32_t no_of_entries;
} ac_output_table_t;

typedef struct ac_ctx_t_ {
    /* pattern arrays.  We need this only during the goto table creation phase */
    mpm_pattern_t **parray;

    /* no of states used by ac */
    uint32_t state_count;

    uint32_t pattern_id_bitarray_size;

    /* the all important memory hungry state_table */
    AC_STATE_TYPE_U16 (*state_table_u16)[256];
    /* the all important memory hungry state_table */
    AC_STATE_TYPE_U32 (*state_table_u32)[256];

    /* goto_table, failure table and output table.  Needed to create state_table.
     * Will be freed, once we have created the state_table */
    int32_t (*goto_table)[256];
    int32_t *failure_table;
    ac_output_table_t *output_table;
    ac_pattern_list_t *pid_pat_list;

    /* the size of each state */
    uint32_t single_state_size;

    uint32_t allocated_state_count;

} ac_ctx_t;

typedef struct ac_threadctx_t_ {
    /* the total calls we make to the search function */
    uint32_t total_calls;
    /* the total patterns that we ended up matching against */
    uint64_t total_matches;
} ac_threadctx_t;

extern void mpm_register_ac(void);

#endif
