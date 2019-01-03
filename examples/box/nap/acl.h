#ifndef __BOX_ACL_H__
#define __BOX_ACL_H__

/** access control list */

#define MAX_ACL_RULE_NUM	(1024)
#define DEFAULT_MAX_CATEGORIES	1
#define VALID_APPLICATION_ID_START	1

enum {
	ACL_TABLE0,
	ACL_TABLE1,
	ACL_TABLES,
};

struct acl_user_data_t {
	uint32_t ul_map_mask;
};

struct acl_config_t{
	struct rte_acl_ctx		*acl_ctx_ip4[VLIB_SOCKETS];
	struct rte_acl_ctx		*acl_ctx_ip6[VLIB_SOCKETS];

	int32_t nr_ipv4_rules;
	int32_t nr_ipv6_rules;
	
#ifdef L3FWDACL_DEBUG
	struct acl4_rule		*rule_ipv4;
	struct acl6_rule		*rule_ipv6;
#endif
} acl_config[ACL_TABLES];

extern struct acl_config_t *g_runtime_acl_config;
extern int g_runtime_acl_config_qua;

static struct{
	const char *rule_ipv4_name;
	const char *rule_ipv6_name;
	int scalar;
} parm_config;


#define __PACK_USERDATA(res)\
	(res + VALID_APPLICATION_ID_START)
#define __UNPACK_USERDATA(res)\
	(res - VALID_APPLICATION_ID_START)

#define OFF_IPv42PROTO (offsetof(struct ipv4_hdr, next_proto_id))
#define OFF_IPv62PROTO (offsetof(struct ipv6_hdr, proto))

#define MBUF_IPv4_2PROTO(m)	\
	rte_pktmbuf_mtod_offset((m), uint8_t *, ETHERNET_HEADER_LEN + OFF_IPv42PROTO)
#define MBUF_IPv6_2PROTO(m)	\
	rte_pktmbuf_mtod_offset((m), uint8_t *, ETHERNET_HEADER_LEN + OFF_IPv62PROTO)

#define DSA_MBUF_IPv4_2PROTO(m)	\
	rte_pktmbuf_mtod_offset((m), uint8_t *, ETHERNET_DSA_HEADER_LEN + OFF_IPv42PROTO)
#define DSA_MBUF_IPv6_2PROTO(m)	\
	rte_pktmbuf_mtod_offset((m), uint8_t *, ETHERNET_DSA_HEADER_LEN + OFF_IPv62PROTO)

ORYX_DECLARE (
	void classify_setup_acl (
		IN const int socketid
	)
);

ORYX_DECLARE (
	struct rte_acl_rule *acl_add_entries (
		IN struct rte_acl_ctx *context,
		IN struct acl_route *entries,
		IN int nr_entries
	)
);

ORYX_DECLARE (
	void reset_acl_config_ctx (
		IN struct acl_config_t *acl_conf
	)
);

#endif

