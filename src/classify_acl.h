#ifndef CLASSIFY_ACL_H
#define CLASSIFY_ACL_H

/** access control list */

#define MAX_ACL_RULE_NUM	(1024)
#define DEFAULT_MAX_CATEGORIES	1
#define VALID_MAP_ID_START	1

enum {
	ACL_TABLE0,
	ACL_TABLE1,
	ACL_TABLES,
};

struct acl_user_data_t {
	uint32_t ul_map_mask;
};

struct acl_config_t{
	char					mapped[NB_SOCKETS];
	struct rte_acl_ctx		*acx_ipv4[NB_SOCKETS];
	struct rte_acl_ctx		*acx_ipv6[NB_SOCKETS];

#ifdef L3FWDACL_DEBUG
	struct acl4_rule		*rule_ipv4;
	struct acl6_rule		*rule_ipv6;
#endif

	oryx_vector				ud_lookup_vector;/** user data lookup vector. */
} acl_config[ACL_TABLES];

extern struct acl_config_t *g_runtime_acl_config;

static struct{
	const char *rule_ipv4_name;
	const char *rule_ipv6_name;
	int scalar;
} parm_config;


#define __PACK_USERDATA(res)\
	(res + VALID_MAP_ID_START)
#define __UNPACK_USERDATA(res)\
	(res - VALID_MAP_ID_START)

struct acl_route {
	union {
		struct ipv4_5tuple k4;
		struct ipv6_5tuple k6;
	}u;
	uint32_t ip_src_mask: 8;
	uint32_t ip_dst_mask: 8;
	uint32_t ip_next_proto_mask: 8;
	uint32_t pad0: 7;
	uint32_t actived: 1;	/** after acl build set actived to 1. */
	uint32_t port_src_mask: 16;
	uint32_t port_dst_mask: 16;

	uint32_t id;/** map_mask is a traffic direction
					which has a set of in_ports and out_ports. */
};

#define OFF_ETHHEAD	(sizeof(struct ether_hdr))
#define OFF_IPV42PROTO (offsetof(struct ipv4_hdr, next_proto_id))
#define OFF_IPV62PROTO (offsetof(struct ipv6_hdr, proto))
#define MBUF_IPV4_2PROTO(m)	\
	rte_pktmbuf_mtod_offset((m), uint8_t *, OFF_ETHHEAD + OFF_IPV42PROTO)
#define MBUF_IPV6_2PROTO(m)	\
	rte_pktmbuf_mtod_offset((m), uint8_t *, OFF_ETHHEAD + OFF_IPV62PROTO)


struct acl_search_t {
	const uint8_t *data_ipv4[DPDK_MAX_RX_BURST];
	struct rte_mbuf *m_ipv4[DPDK_MAX_RX_BURST];
	uint32_t res_ipv4[DPDK_MAX_RX_BURST];
	int num_ipv4;

	const uint8_t *data_ipv6[DPDK_MAX_RX_BURST];
	struct rte_mbuf *m_ipv6[DPDK_MAX_RX_BURST];
	uint32_t res_ipv6[DPDK_MAX_RX_BURST];
	int num_ipv6;

	const uint8_t *data_notip[DPDK_MAX_RX_BURST];
	struct rte_mbuf *m_notip[DPDK_MAX_RX_BURST];
	int num_notip;
};

extern void classify_setup_acl(const int socketid);
extern int acl_add_entries(struct acl_route *entries, int num);

static __oryx_always_inline__
void acl_set_userdata(struct rte_acl_rule *rar,
							struct acl_route *ar)
{
	oryx_vector v = g_runtime_acl_config->ud_lookup_vector;
	int userdata = 0;
	
	struct acl_user_data_t *aud = malloc(sizeof(struct acl_user_data_t));
	BUG_ON(aud == NULL);

	memset(aud, 0, sizeof(struct acl_user_data_t));
	aud->ul_map_mask = ar->id;
	userdata = vec_set(v, (void *)aud);
	rar->data.userdata = __PACK_USERDATA(userdata);
}

static __oryx_always_inline__
int acl_get_userdata(uint32_t userdata, struct acl_user_data_t **aud)
{
	oryx_vector v = g_runtime_acl_config->ud_lookup_vector;
	userdata = __UNPACK_USERDATA(userdata);
	
#if defined(BUILD_DEBUG)
	BUG_ON(aud == NULL);
#endif
	if(!vec_active(v) ||
		userdata > vec_active(v))
		return -1;
	
	(*aud) = NULL;
	(*aud) = vec_lookup(v, userdata);
	return 0;
}

#endif
