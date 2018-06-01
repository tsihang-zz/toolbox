#include "oryx.h"
#include "dpdk_classify.h"

#if !defined(HAVE_DPDK)
/**
 * ACL Field definition.
 * Each field in the ACL rule has an associate definition.
 * It defines the type of field, its size, its offset in the input buffer,
 * the field index, and the input index.
 * For performance reasons, the inner loop of the search function is unrolled
 * to process four input bytes at a time. This requires the input to be grouped
 * into sets of 4 consecutive bytes. The loop processes the first input byte as
 * part of the setup and then subsequent bytes must be in groups of 4
 * consecutive bytes.
 */

struct rte_acl_field_def {
	uint8_t  type;        /**< type - RTE_ACL_FIELD_TYPE_*. */
	uint8_t	 size;        /**< size of field 1,2,4, or 8. */
	uint8_t	 field_index; /**< index of field inside the rule. */
	uint8_t  input_index; /**< 0-N input index. */
	uint32_t offset;      /**< offset to start of field. */
};
#endif

/*
 * Rule and trace formats definitions.
 */

enum {
	PROTO_FIELD_IPV4,
	SRC_FIELD_IPV4,
	DST_FIELD_IPV4,
	SRCP_FIELD_IPV4,
	DSTP_FIELD_IPV4,
	NUM_FIELDS_IPV4
};

/*
 * That effectively defines order of IPV4VLAN classifications:
 *  - PROTO
 *  - VLAN (TAG and DOMAIN)
 *  - SRC IP ADDRESS
 *  - DST IP ADDRESS
 *  - PORTS (SRC and DST)
 */
enum {
	RTE_ACL_IPV4VLAN_PROTO,
	RTE_ACL_IPV4VLAN_VLAN,
	RTE_ACL_IPV4VLAN_SRC,
	RTE_ACL_IPV4VLAN_DST,
	RTE_ACL_IPV4VLAN_PORTS,
	RTE_ACL_IPV4VLAN_NUM
};


struct rte_acl_field_def ipv4_defs[] = {

	/* first input field - always one byte long. */
	{
		.type = RTE_ACL_FIELD_TYPE_BITMASK,
		.size = sizeof (uint8_t),
		.field_index = 0,
		.input_index = 0,
		.offset = offsetof (struct ipv4_5tuple, proto),
	},
	
	/* next input field (IPv4 source address) - 4 consecutive bytes. */
	{
		.type = RTE_ACL_FIELD_TYPE_MASK,
		.size = sizeof (uint32_t),
		.field_index = 1,
		.input_index = 1,
		.offset = offsetof (struct ipv4_5tuple, ip_src),
	},

	/* next input field (IPv4 destination address) - 4 consecutive bytes. */
	{
		.type = RTE_ACL_FIELD_TYPE_MASK,
		.size = sizeof (uint32_t),
		.field_index = 2,
		.input_index = 2,
		.offset = offsetof (struct ipv4_5tuple, ip_dst),
	},

	/*
	 * Next 2 fields (src & dst ports) form 4 consecutive bytes.
	 * They share the same input index.
	 */
	{
		.type = RTE_ACL_FIELD_TYPE_RANGE,
		.size = sizeof (uint16_t),
		.field_index = 3,
		.input_index = 3,
		.offset = offsetof (struct ipv4_5tuple, port_src),
	},
	
	{
		.type = RTE_ACL_FIELD_TYPE_RANGE,
		.size = sizeof (uint16_t),
		.field_index = 4,
		.input_index = 3,
		.offset = offsetof (struct ipv4_5tuple, port_dst),
	},

};

/** 
 * @@EG. 
 *   SIP/mask DIP/mask SP DP PROTO/mask 
 * 	 192.168.1.0/24 192.168.2.31/32 0:65535 1234:1234 17/0xff
 */


/*
 * Initialize Access Control List (acl) parameters.
 */
void classify_setup_acl(const int socketid)
{
}

