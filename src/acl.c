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

struct rte_acl_field_def ipv4_defs[NUM_FIELDS_IPV4] = {

	/* first input field - always one byte long. */
	{
		.type = RTE_ACL_FIELD_TYPE_BITMASK,
		.size = sizeof(uint8_t),
		.field_index = PROTO_FIELD_IPV4,
		.input_index = RTE_ACL_IPV4VLAN_PROTO,
		.offset = 0,
	},

	
	/* next input field (IPv4 source address) - 4 consecutive bytes. */
	{
		.type = RTE_ACL_FIELD_TYPE_MASK,
		.size = sizeof(uint32_t),
		.field_index = SRC_FIELD_IPV4,
		.input_index = RTE_ACL_IPV4VLAN_SRC,
		.offset = offsetof(struct ipv4_hdr, src_addr) -
			offsetof(struct ipv4_hdr, next_proto_id),
	},


	/* next input field (IPv4 destination address) - 4 consecutive bytes. */
	{
		.type = RTE_ACL_FIELD_TYPE_MASK,
		.size = sizeof(uint32_t),
		.field_index = DST_FIELD_IPV4,
		.input_index = RTE_ACL_IPV4VLAN_DST,
		.offset = offsetof(struct ipv4_hdr, dst_addr) -
			offsetof(struct ipv4_hdr, next_proto_id),
	},


	/*
	 * Next 2 fields (src & dst ports) form 4 consecutive bytes.
	 * They share the same input index.
	 */
	{
		.type = RTE_ACL_FIELD_TYPE_RANGE,
		.size = sizeof(uint16_t),
		.field_index = SRCP_FIELD_IPV4,
		.input_index = RTE_ACL_IPV4VLAN_PORTS,
		.offset = sizeof(struct ipv4_hdr) -
			offsetof(struct ipv4_hdr, next_proto_id),
	},

	
	{
		.type = RTE_ACL_FIELD_TYPE_RANGE,
		.size = sizeof(uint16_t),
		.field_index = DSTP_FIELD_IPV4,
		.input_index = RTE_ACL_IPV4VLAN_PORTS,
		.offset = sizeof(struct ipv4_hdr) -
			offsetof(struct ipv4_hdr, next_proto_id) +
			sizeof(uint16_t),
	},


};
/** 
 * @@EG. 
 *   SIP/mask DIP/mask SP DP PROTO/mask 
 * 	 192.168.1.0/24 192.168.2.31/32 0:65535 1234:1234 17/0xff
 */

enum {
	CB_FLD_SRC_ADDR,
	CB_FLD_DST_ADDR,
	CB_FLD_SRC_PORT_LOW,
	CB_FLD_SRC_PORT_DLM,
	CB_FLD_SRC_PORT_HIGH,
	CB_FLD_DST_PORT_LOW,
	CB_FLD_DST_PORT_DLM,
	CB_FLD_DST_PORT_HIGH,
	CB_FLD_PROTO,
	CB_FLD_USERDATA,
	CB_FLD_NUM,
};

RTE_ACL_RULE_DEF(acl4_rule, RTE_DIM(ipv4_defs));
//RTE_ACL_RULE_DEF(acl6_rule, RTE_DIM(ipv6_defs));


int dim = RTE_DIM(ipv4_defs);

/*
 * Initialize Access Control List (acl) parameters.
 */
void classify_setup_acl(const int socketid)
{
	char name[PATH_MAX];
	struct rte_acl_param acl_param;
	struct rte_acl_config acl_build_param;
	struct rte_acl_ctx *context;
	
	/* Create ACL contexts */
	snprintf(name, sizeof(name), "%s%d",
			"classify ACL",
			socketid);

	acl_param.name = name;
	acl_param.socket_id = socketid;
	acl_param.rule_size = RTE_ACL_RULE_SZ(dim);
	acl_param.max_rule_num = MAX_ACL_RULE_NUM;

	if ((context = rte_acl_create(&acl_param)) == NULL)
		rte_exit(EXIT_FAILURE, "Failed to create ACL context\n");

	if (rte_acl_set_ctx_classify(context,
			RTE_ACL_CLASSIFY_NEON) != 0)
		rte_exit(EXIT_FAILURE,
			"Failed to setup classify method for  ACL context\n");
	
	acl_config.acx_ipv4[socketid] = context;
}

#define uint32_t_to_char(ip, a, b, c, d) do {\
		*a = (unsigned char)(ip >> 24 & 0xff);\
		*b = (unsigned char)(ip >> 16 & 0xff);\
		*c = (unsigned char)(ip >> 8 & 0xff);\
		*d = (unsigned char)(ip & 0xff);\
	} while (0)


static inline void
print_one_ipv4_rule(struct acl4_rule *rule, int extra)
{
	unsigned char a, b, c, d;

	uint32_t_to_char(rule->field[SRC_FIELD_IPV4].value.u32,
			&a, &b, &c, &d);
	printf("%hhu.%hhu.%hhu.%hhu/%u ", a, b, c, d,
			rule->field[SRC_FIELD_IPV4].mask_range.u32);
	uint32_t_to_char(rule->field[DST_FIELD_IPV4].value.u32,
			&a, &b, &c, &d);
	printf("%hhu.%hhu.%hhu.%hhu/%u ", a, b, c, d,
			rule->field[DST_FIELD_IPV4].mask_range.u32);
	printf("%hu:%hu %hu:%hu 0x%hhx/0x%hhx ",
		rule->field[SRCP_FIELD_IPV4].value.u16,
		rule->field[SRCP_FIELD_IPV4].mask_range.u16,
		rule->field[DSTP_FIELD_IPV4].value.u16,
		rule->field[DSTP_FIELD_IPV4].mask_range.u16,
		rule->field[PROTO_FIELD_IPV4].value.u8,
		rule->field[PROTO_FIELD_IPV4].mask_range.u8);
	if (extra)
		printf("0x%x-0x%x-0x%x ",
			rule->data.category_mask,
			rule->data.priority,
			rule->data.userdata);
}

static inline void
dump_ipv4_rules(struct acl4_rule *rule, int num, int extra)
{
	int i;

	for (i = 0; i < num; i++, rule++) {
		printf("\t%d:", i + 1);
		print_one_ipv4_rule(rule, extra);
		printf("\n");
	}
}

int total_num = 0;

static __oryx_always_inline__
void convert_acl(struct     acl_route *ar,
		struct rte_acl_rule *v)
{
	struct ipv4_5tuple *k = &ar->u.k4;
	
	v->field[PROTO_FIELD_IPV4].value.u8 = k->proto;
	v->field[PROTO_FIELD_IPV4].mask_range.u8 = ar->proto_mask;

	v->field[SRC_FIELD_IPV4].value.u32 = k->ip_src;
	v->field[SRC_FIELD_IPV4].mask_range.u32 = ar->ip_src_mask;
	
	v->field[DST_FIELD_IPV4].value.u32 = k->ip_dst;
	v->field[DST_FIELD_IPV4].mask_range.u32 = ar->ip_dst_mask;
	
	v->field[SRCP_FIELD_IPV4].value.u16 = k->port_src;
	v->field[SRCP_FIELD_IPV4].mask_range.u16 = ar->port_src_mask;

	v->field[DSTP_FIELD_IPV4].value.u16 = k->port_dst;
	v->field[DSTP_FIELD_IPV4].mask_range.u16 = ar->port_dst_mask;

	v->data.userdata = (ar->id + VALID_MAP_ID_START);

	v->data.priority = RTE_ACL_MAX_PRIORITY - total_num;
	v->data.category_mask = -1;
	total_num ++;

}

int acl_add_entry(struct acl_route *entry)
{
	int ret = 0;
	unsigned int acl_num = 1;
	int socketid = 0;
	struct rte_acl_config acl_build_param;
	struct rte_acl_ctx *context = acl_config.acx_ipv4[socketid];
	struct rte_acl_rule *acl_base;
	
	acl_base = calloc(1, sizeof(struct acl4_rule));
	if (!acl_base) {
		return -1;
	}
	
	memset (acl_base, 0, sizeof(struct rte_acl_rule));

	convert_acl(entry, acl_base);

	oryx_logn("IPv4 Route entries %u:", 1);
	dump_ipv4_rules((struct acl4_rule *)acl_base, 1, 1);

	if (rte_acl_add_rules(context, acl_base, acl_num) < 0)
		rte_exit(EXIT_FAILURE, "add rules failed\n");

	/* Perform builds */
	memset(&acl_build_param, 0, sizeof(acl_build_param));

	acl_build_param.num_categories = DEFAULT_MAX_CATEGORIES;
	acl_build_param.num_fields = dim;
	memcpy(&acl_build_param.defs, ipv4_defs, sizeof(ipv4_defs));

	if (rte_acl_build(context, &acl_build_param) != 0)
		rte_exit(EXIT_FAILURE, "Failed to build ACL trie\n");

	rte_acl_dump(context);

	free (acl_base);
	
	return ret;
}

