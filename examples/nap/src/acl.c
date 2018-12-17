#include "oryx.h"
#include "common_private.h"
#include "iface_private.h"
#include "map_private.h"
#include "appl_private.h"
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
struct acl_config_t *g_runtime_acl_config;
int g_runtime_acl_config_qua = 0;


/*
 * Initialize Access Control List (acl) parameters.
 */
void classify_setup_acl(const int socketid)
{
	char name[PATH_MAX];
	struct rte_acl_param acl_param;
	struct rte_acl_ctx *context;
	int acl_table;
	struct acl_config_t *acl_conf;

	for (acl_table = 0; acl_table < ACL_TABLES; acl_table ++) {

		acl_conf = (struct acl_config_t *)&acl_config[acl_table];
		
		/* Create ACL contexts */
		snprintf(name, sizeof(name), "%s%d%d",
				"classify ACL", acl_table, socketid);

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
		
		acl_conf->acl_ctx_ip4[socketid] = context;
		acl_conf->nr_ipv4_rules = 0;
		oryx_logn("acl_config_table(%d) %p", acl_table, acl_conf);
	}

	g_runtime_acl_config_qua = ACL_TABLE0;
	g_runtime_acl_config = &acl_config[g_runtime_acl_config_qua];
}

#define uint32_t_to_char(ip, a, b, c, d) do {\
		*a = (unsigned char)(ip >> 24 & 0xff);\
		*b = (unsigned char)(ip >> 16 & 0xff);\
		*c = (unsigned char)(ip >> 8 & 0xff);\
		*d = (unsigned char)(ip & 0xff);\
	} while (0)

static __oryx_always_inline__
void print_one_ipv4_rule(FILE *fp, struct acl4_rule *rule, int extra)
{
	unsigned char a, b, c, d;

	uint32_t_to_char(rule->field[SRC_FIELD_IPV4].value.u32,
			&a, &b, &c, &d);
	fprintf (fp, "%hhu.%hhu.%hhu.%hhu/%u ", a, b, c, d,
			rule->field[SRC_FIELD_IPV4].mask_range.u32);
	fflush(fp);
	
	uint32_t_to_char(rule->field[DST_FIELD_IPV4].value.u32,
			&a, &b, &c, &d);
	fprintf (fp, "%hhu.%hhu.%hhu.%hhu/%u ", a, b, c, d,
			rule->field[DST_FIELD_IPV4].mask_range.u32);
	fflush(fp);
	
	fprintf (fp, "%hu:%hu %hu:%hu 0x%hhx/0x%hhx ",
		rule->field[SRCP_FIELD_IPV4].value.u16,
		rule->field[SRCP_FIELD_IPV4].mask_range.u16,
		rule->field[DSTP_FIELD_IPV4].value.u16,
		rule->field[DSTP_FIELD_IPV4].mask_range.u16,
		rule->field[PROTO_FIELD_IPV4].value.u8,
		rule->field[PROTO_FIELD_IPV4].mask_range.u8);
	fflush(fp);

	if (extra) {
		fprintf (fp, "0x%x-0x%x-0x%x ",
			rule->data.category_mask,
			rule->data.priority,
			rule->data.userdata);	
		fflush(fp);
	}
}

static __oryx_always_inline__
void dump_ipv4_rules(FILE *fp, struct acl4_rule *rule, int nr_entries, int extra)
{
	int i;
	
	for (i = 0; i < nr_entries; i++, rule++) {
		fprintf (fp, "\t%d:", i + 1);
		fflush(fp);
		print_one_ipv4_rule(fp, rule, extra);
		fprintf (fp, "\n");		
		fflush(fp);
	}
}

static __oryx_always_inline__
void appl2_ar(struct appl_t *appl, struct acl_route *ar)
{
	ar->u.k4.ip_src 			= (appl->ip_src_mask == ANY_IPADDR)			? 0 : appl->ip_src;
	ar->ip_src_mask				= appl->ip_src_mask;
	ar->u.k4.ip_dst 			= (appl->ip_dst_mask == ANY_IPADDR)			? 0 : appl->ip_dst;
	ar->ip_dst_mask 			= appl->ip_dst_mask;
	ar->u.k4.port_src			= (appl->l4_port_src_mask == ANY_PORT)		? 0 : appl->l4_port_src;
	ar->port_src_mask			= (appl->l4_port_src_mask == ANY_PORT)		? 0xFFFF : appl->l4_port_src_mask;
	ar->u.k4.port_dst			= (appl->l4_port_dst_mask == ANY_PORT)		? 0 : appl->l4_port_dst;
	ar->port_dst_mask			= (appl->l4_port_dst_mask == ANY_PORT)		? 0xFFFF : appl->l4_port_dst_mask;
	ar->u.k4.proto				= (appl->ip_next_proto_mask == ANY_PROTO)	? 0 : appl->ip_next_proto;
	ar->ip_next_proto_mask		= (appl->ip_next_proto_mask == ANY_PROTO)	? 0 : appl->ip_next_proto_mask;
	ar->map_mask				= appl->ul_map_mask;
	ar->appid					= appl_id(appl);
	ar->prio					= appl->priority;
}
NB_SOCKETS

static __oryx_always_inline__
void convert_acl(struct     acl_route *ar,
		struct rte_acl_rule *v)
{
	struct ipv4_5tuple *k;
	k = &ar->u.k4;

	v->field[PROTO_FIELD_IPV4].value.u8 = k->proto;
	v->field[PROTO_FIELD_IPV4].mask_range.u8 = ar->ip_next_proto_mask;

	v->field[SRC_FIELD_IPV4].value.u32 = k->ip_src;
	v->field[SRC_FIELD_IPV4].mask_range.u32 = ar->ip_src_mask;
	
	v->field[DST_FIELD_IPV4].value.u32 = k->ip_dst;
	v->field[DST_FIELD_IPV4].mask_range.u32 = ar->ip_dst_mask;
	
	v->field[SRCP_FIELD_IPV4].value.u16 = k->port_src;
	v->field[SRCP_FIELD_IPV4].mask_range.u16 = ar->port_src_mask;

	v->field[DSTP_FIELD_IPV4].value.u16 = k->port_dst;
	v->field[DSTP_FIELD_IPV4].mask_range.u16 = ar->port_dst_mask;

	acl_set_userdata(v, ar);
	v->data.priority = (RTE_ACL_MAX_PRIORITY - ar->prio);
	v->data.category_mask = -1;
}

static int acl_build_entries(struct rte_acl_ctx *context,
			struct rte_acl_rule *acl_base)
{
	int socketid = 0;
	struct rte_acl_config acl_build_param;

	/* Perform builds */
	memset(&acl_build_param, 0, sizeof(acl_build_param));
	acl_build_param.num_categories = DEFAULT_MAX_CATEGORIES;
	acl_build_param.num_fields = dim;
	memcpy(&acl_build_param.defs, ipv4_defs, sizeof(ipv4_defs));	

	return rte_acl_build(context, &acl_build_param);
}

struct rte_acl_rule *acl_add_entries(struct rte_acl_ctx *context,
			struct acl_route *entries, int nr_entries)
{
	int ret = 0;
	uint8_t *acl_rules;
	int i;
	struct rte_acl_rule *acl_base, *next;
	
	acl_rules = calloc(nr_entries, sizeof(struct acl4_rule));
	if (!acl_rules) {
		return NULL;
	}
	
	memset (acl_rules, 0, sizeof(struct acl4_rule) * nr_entries);

	for (i = 0; i < nr_entries; i ++) {
		next = (struct rte_acl_rule *)(acl_rules +
				i * sizeof(struct acl4_rule));
		convert_acl(&entries[i], next);
	}

	acl_base = (struct rte_acl_rule *)acl_rules;

	ret = rte_acl_add_rules(context, acl_base, nr_entries);
	if (ret < 0) {
		oryx_loge(ret, "add rules failed\n");
		free (acl_base);
		return NULL;
	}

	ret = acl_build_entries(context, acl_base);
	if (ret != 0) {
		oryx_loge(ret, "Failed to build ACL trie\n");
		free (acl_base);
		return NULL;
	}
	oryx_logn("IPv4 Route entries %u:", nr_entries);
	dump_ipv4_rules(stdout, (struct acl4_rule *)acl_base, nr_entries, 1);
	rte_acl_dump(context);

	return acl_base;
}

static __oryx_always_inline__
void reset_acl_config_ctx(struct acl_config_t *acl_conf)
{
	int socketid = 0;
	acl_conf->nr_ipv4_rules = 0;
	acl_conf->nr_ipv6_rules = 0;
	rte_acl_reset(acl_conf->acl_ctx_ip4[socketid]);

}

void sync_acl(vlib_main_t *vm, int *nr_entries)
{
	vlib_map_main_t *mm = &vlib_map_main;
	vlib_appl_main_t *am = &vlib_appl_main;
	struct appl_t *appl;
	struct map_t *map;
	int each = 0, i = 0, hv = 0, nb_entries = 0;
	int socketid = 0;
	struct acl_route *entries, *entry;
	struct rte_acl_ctx *context;
	struct acl_config_t *acl_next_config; 
	const char	*f = "/data/acl_sync_result.txt";
	char cat_null[128] = "cat /dev/null > ";
	static FILE *acl_fp;

	if(!(vm->ul_flags & VLIB_DP_SYNC_ACL))
		return;

	vec_foreach_element(am->entry_vec, each, appl) {
		if (unlikely(!appl) || !appl_is_inuse(appl))
			continue;
		nb_entries ++;
	}

	if (nb_entries == 0)
		return;

	entries = malloc(nb_entries * sizeof (struct acl_route));
	BUG_ON(entries == NULL);

	vec_foreach_element(am->entry_vec, each, appl) {
		if (unlikely(!appl) || !appl_is_inuse(appl))
			continue;

		if (i > nb_entries)
			break;
		
		entry = &entries[i ++];
		appl2_ar(appl, entry);
	}
	
	acl_next_config	= &acl_config[(g_runtime_acl_config_qua + 1) % ACL_TABLES];
	reset_acl_config_ctx(acl_next_config);
	
	context = acl_next_config->acl_ctx_ip4[socketid];
	
	struct rte_acl_rule *acl_base = acl_add_entries(context, entries, nb_entries);
	if (!acl_base) {
		oryx_loge(-1,
			"(%d) sync acl error %d elements", hv, nb_entries);
	} else {
		/* clear ACL result file. */		
		strcat(cat_null, f);
		system(cat_null);
		
		/* open result file. */
		if(!acl_fp) {
			acl_fp = fopen(f, "a+");
			if(!acl_fp) acl_fp = stdout;
		}
		dump_ipv4_rules(acl_fp, (struct acl4_rule *)acl_base, nb_entries, 1);
		
		/* free acl_base ASAP */
		free(acl_base);
		
		vec_foreach_element(am->entry_vec, each, appl) {
			if (unlikely(!appl) || !appl_is_inuse(appl))
				continue;
			appl->ul_flags &= ~APPL_CHANGED;
			acl_next_config->nr_ipv4_rules ++;
		}
		
		oryx_logn("nb_entries %d, nb_rules %d", nb_entries, acl_next_config->nr_ipv4_rules);
#if defined(BUILD_DEBUG)
		BUG_ON(acl_next_config->nr_ipv4_rules != nb_entries);
#endif		
		/* fast switch. */		
		lock_lcores(vm);
		vm->ul_flags &= ~VLIB_DP_SYNC_ACL;
		g_runtime_acl_config_qua += 1;
		unlock_lcores(vm);
	}

	(*nr_entries) = nb_entries;
}

