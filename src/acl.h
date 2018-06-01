#ifndef ACL_H
#define ACL_H

/** access control list */

static struct {
	char mapped[NB_SOCKETS];
	struct rte_acl_ctx *acx_ipv4[NB_SOCKETS];
	struct rte_acl_ctx *acx_ipv6[NB_SOCKETS];
#ifdef L3FWDACL_DEBUG
	struct acl4_rule *rule_ipv4;
	struct acl6_rule *rule_ipv6;
#endif
} acl_config;

static struct{
	const char *rule_ipv4_name;
	const char *rule_ipv6_name;
	int scalar;
} parm_config;

void classify_setup_acl(const int socketid);

#endif
