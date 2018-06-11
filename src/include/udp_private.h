#ifndef UDP_PRIVATE_H
#define UDP_PRIVATE_H

#include "common_private.h"

#define UDP_PREFIX	"udp"
/** Maximum UDP entries. */
#define MAX_UDPS (1 << 3)	/** 1 million */
/** Maximum patterns in a udp entry. */
#define UDP_N_PATTERNS		128
#define UDP_INVALID_ID		(MAX_UDPS)

#define foreach_pattern_flags	\
  _(NOCASE, 0, "Case sensitive pattern.") \
  _(INSTALLED, 1, "Has this pattern already been installed to MPM.")\
  _(RESV, 1, "Resv.")

enum pattern_flags_t {
#define _(f,o,s) PATTERN_FLAGS_##f = (1<<o),
	foreach_pattern_flags
#undef _
	PATTERN_FLAGS,
};

#define PATTERN_SIZE	128
#define PATTERN_DEFAULT_FLAGS	(PATTERN_FLAGS_NOCASE/** defaultly */)
#define PATTERN_DEFAULT_OFFSET	64
#define PATTERN_DEFAULT_DEPTH		128
#define PATTERN_INVALID_ID		(UDP_N_PATTERNS)

struct pattern_t {

	/** ID of current pattern. */
	u32 ul_id;

	/** Pattern length. */
	size_t ul_size;

	/** Pattern Value. */
	char sc_val[PATTERN_SIZE];
	
	/** Defined by PATTERN_FLAGS_XXXX. */
	u32 ul_flags;

	/** Offset where this pattern first appears in a packet. */
	u16 us_offset;

	/** Depth of search for this pattern in a packet. */
	u16 us_depth;

	/** Statistics for this pattern. */
	struct stats_trunk_t hits;
};

#define PATTERN_HIT(hits,m) (hits.m)

#define PATTERN_STATS_B(pt)\
	PATTERN_HIT(((pt)->hits), bytes)

#define PATTERN_STATS_P(pt)\
	PATTERN_HIT(((pt)->hits), packets)

#define PATTERN_STATS_INC_BYTE(pt, v)\
	(PATTERN_STATS_B(pt) += v)

#define PATTERN_STATS_INC_PKT(pt, v)\
	(PATTERN_STATS_P(pt) += v)


/** User Defined Pattern.*/
struct udp_t {
	/** Unique, and can be well human-readable. */
	char sc_alias[32];
	/** Unique, and can be allocated by a oryx_vector function automatically. */
	u32 ul_id;
	/** Patterns stored by a oryx_vector. */
	oryx_vector patterns;
	/** Unused. */
	u32 ul_flags;	
	/** Datapath Output is not defined. */
	u32 ul_dpo;

	/** Create time. */
	uint64_t			ull_create_time;

	/** Which map this udp_t belongs to. */
	oryx_vector qua[QUAS];
};

typedef struct {
	int ul_n_udps;
	u32 ul_flags;
	oryx_vector entry_vec;
	struct oryx_htable_t *htable;
	
}vlib_udp_main_t;

vlib_udp_main_t vlib_udp_main;

#define udp_slot(u) ((u)->ul_id)
#define udp_alias(u) ((u)->sc_alias)


#endif

