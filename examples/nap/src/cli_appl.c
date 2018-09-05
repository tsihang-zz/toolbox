#include "oryx.h"
#include "vty.h"
#include "command.h"
#include "prefix.h"
#include "common_private.h"
#include "util_ipset.h"
#include "cli_appl.h"
#include "map_private.h"
#include "rc4.h"

vlib_appl_main_t vlib_appl_main = {
	.lock = INIT_MUTEX_VAL,
};

atomic_t n_application_elements = ATOMIC_INIT(0);

#define PRINT_SUMMARY	\
		vty_out (vty, "matched %d element(s), total %d element(s)%s", \
			atomic_read(&n_application_elements), am->nb_appls, VTY_NEWLINE);

static void
ht_appl_free (const ht_value_t v)
{
	/** To avoid warnings. */
}

static ht_key_t
ht_appl_hval (struct oryx_htable_t *ht,
		const ht_value_t v, uint32_t s) 
{
     uint8_t *d = (uint8_t *)v;
     uint32_t i;
     uint32_t hv = 0;

     for (i = 0; i < s; i++) {
         if (i == 0)      hv += (((uint32_t)*d++));
         else if (i == 1) hv += (((uint32_t)*d++) * s);
         else             hv *= (((uint32_t)*d++) * i) + s + i;
     }

     hv *= s;
     hv %= ht->array_size;
     
     return hv;
}

static int
ht_appl_cmp (const ht_value_t v1, 
		uint32_t s1,
		const ht_value_t v2,
		uint32_t s2)
{
	int xret = 0;

	if (!v1 || !v2 ||s1 != s2 ||
		memcmp(v1, v2, s2))
		xret = 1;
	
	return xret;
}

#define VTY_ERROR_APPLICATION(prefix, alias)\
			vty_out (vty, "%s(Error)%s %s application \"%s\"%s", \
				draw_color(COLOR_RED), draw_color(COLOR_FIN), prefix, (const char *)alias, VTY_NEWLINE)
		
#define VTY_SUCCESS_APPLICATION(prefix, v)\
			vty_out (vty, "%s(Success)%s %s application \"%s\"(%u)%s", \
				draw_color(COLOR_GREEN), draw_color(COLOR_FIN), prefix, v->sc_alias, v->ul_id, VTY_NEWLINE)
		
static __oryx_always_inline__
int prefix2str_(const struct prefix *p, char *str, int size)
{
	char buf[BUFSIZ];
	
	if (p->prefixlen == 0)
		snprintf(str, size, "%s", "any");
	else {
		inet_ntop(p->family, &p->u.prefix, buf, BUFSIZ);
			snprintf(str, size, "%s/%d", buf, p->prefixlen);
	}
	return 0;
}

/** if a null appl specified, appl_entry_output display all */
static int appl_entry_output (struct appl_t *appl, struct vty *vty)
{
	BUG_ON(appl == NULL);
	
	struct prefix *p;
	uint8_t pfx_buf[__SRC_DST__][INET_ADDRSTRLEN];
	uint8_t port_buf[__SRC_DST__][16];
	uint8_t proto_buf[32];
	struct prefix_ipv4 ip4;
	
	char tmstr[100];
	fmt_time (appl->create_time, "%Y-%m-%d,%H:%M:%S", (char *)&tmstr[0], 100);

	if(appl->ip_src_mask == ANY_IPADDR) {
		sprintf((char *)&pfx_buf[__SRC__][0], "%s", "any");
	} else {
		ip4.family			= AF_INET;
		ip4.prefixlen		= appl->ip_src_mask;
		ip4.prefix.s_addr	= hton32(appl->ip_src);
		p = (struct prefix *)&ip4;
		prefix2str_ (p, (char *)&pfx_buf[__SRC__][0], INET_ADDRSTRLEN);
	}

	if(appl->ip_dst_mask == ANY_IPADDR) {
		sprintf((char *)&pfx_buf[__DST__][0], "%s", "any");
	} else {
		ip4.family			= AF_INET;
		ip4.prefixlen		= appl->ip_dst_mask;
		ip4.prefix.s_addr	= hton32(appl->ip_dst);
		p = (struct prefix *)&ip4;
		prefix2str_ (p, (char *)&pfx_buf[__DST__][0], INET_ADDRSTRLEN);
	}

	if(appl->l4_port_src_mask == ANY_PORT) {
		sprintf ((char *)&port_buf[__SRC__][0], "%s", "any");
	} else {
		sprintf ((char *)&port_buf[__SRC__][0], "%d:%d", appl->l4_port_src, appl->l4_port_src_mask);
	}

	if(appl->l4_port_dst_mask == ANY_PORT) {
		sprintf ((char *)&port_buf[__DST__][0], "%s", "any");
	} else {
		sprintf ((char *)&port_buf[__DST__][0], "%d:%d", appl->l4_port_dst, appl->l4_port_dst_mask);
	}

	if(appl->ip_next_proto_mask == ANY_PROTO) {
		sprintf ((char *)&proto_buf[0], "%s", "any");
	} else {
		sprintf ((char *)&proto_buf[0], "%02x/%02x", appl->ip_next_proto, appl->ip_next_proto_mask);
	}
	
	vty_out (vty, "%24s%4u%8u%20s%20s%10s%10s%10s%12s%08x%23s%23lu%s", 
				appl_alias(appl), appl_id(appl), appl->priority,
				pfx_buf[__SRC__], pfx_buf[__DST__], port_buf[__SRC__], port_buf[__DST__], proto_buf, 
				(appl->ul_flags & APPL_CHANGED) ? "!synced" : "synced",
				appl->ul_map_mask, tmstr, appl->refcnt, VTY_NEWLINE);

	return 0;
}

static int no_appl_table_entry (struct appl_t *appl, struct vty *vty)
{
	int ret;
	vlib_appl_main_t *am = &vlib_appl_main;
	vlib_main_t *vm = am->vm;
	
	/** Is this application is inuse ? */
	if(appl_is_inuse(appl)) {
		vty_out(vty, "%s inuse by map(s)(%08x)%s",
						appl_alias(appl), appl->ul_map_mask, VTY_NEWLINE);
		return 0;
	}
	
	ret = appl_entry_del(am, appl);
	if (!ret) {
		vm->ul_flags |= VLIB_DP_SYNC_ACL;
		vty_out(vty, "Need \"sync\" after this command take affect.%s", VTY_NEWLINE);
	}

	return 0;
}

static int appl_entry_desenitize (struct appl_t *appl,
					struct vty *vty, const char *value)
{
	char keyword_backup[256] = {0};

	if (appl->sc_keyword) {
		memcpy (keyword_backup, appl->sc_keyword, strlen (appl->sc_keyword));
		free (appl->sc_keyword);
	}
	
	if (value) {
		appl->sc_keyword = strdup (value);
		/** Init rc4. */
		rc4_init (appl->uc_keyword_encrypt, (uint8_t *)appl->sc_keyword, (uint64_t)strlen (appl->sc_keyword));
		VTY_SUCCESS_APPLICATION ("Encrypt-Keyword", appl);
		vty_out (vty, "%s -> %s", keyword_backup, appl->sc_keyword);
		vty_newline(vty);
	}

	return 0;
}

static int appl_entry_priority (struct appl_t *appl,
					struct vty *vty, const char *value)
{
	vlib_appl_main_t *am = &vlib_appl_main;
	vlib_main_t *vm = am->vm;
	uint32_t priority;
	char *end;

	priority = strtoul((const char *)value, &end, 10);
	if (errno == ERANGE){
		oryx_logn("%d (%s)", errno, value);
		return -EINVAL;
	}

	/** no effect when setting a same priority with before. */
	if (appl->priority == priority)
		return 0;
	
	appl->priority = priority;
	appl->ul_flags |= APPL_CHANGED;

	/** check this application is inuse ? */
	if (appl->ul_map_mask != 0) {	
		vm->ul_flags |= VLIB_DP_SYNC_ACL;
	}
	
	return 0;
}


DEFUN(show_application,
      show_application_cmd,
      "show application [WORD]",
      SHOW_STR SHOW_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR)
{
	vlib_appl_main_t *am = &vlib_appl_main;
	
	/** display all stream applications. */
	vty_out (vty, "Trying to display %d elements ...%s", am->nb_appls, VTY_NEWLINE);
	vty_out (vty, "%24s%4s%8s%20s%20s%10s%10s%10s%12s%8s%23s%23s%s", 
		"ALIAS", "ID", "PRIO", "IP-SRC", "IP-DST", "PORT-SRC", "PORT-DST", "PROTO", "STATEs", "MAP", "CREAT-TIME", "REFCNT", VTY_NEWLINE);
	
	if (argc == 0){
		foreach_application_func1_param1 (argv[0], 
			appl_entry_output, vty)
	}
	else {
		split_foreach_application_func1_param1 (argv[0], 
			appl_entry_output, vty);
	}

	PRINT_SUMMARY;
	
    return CMD_SUCCESS;
}

DEFUN(no_application,
      no_application_cmd,
      "no application [WORD]",
      NO_STR
      NO_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR)
{
	vlib_appl_main_t *am = &vlib_appl_main;

	if (argc == 0) {
		foreach_application_func1_param1(argv[0], 
			no_appl_table_entry, vty);
	}
	else {
		split_foreach_application_func1_param1(argv[0], 
			no_appl_table_entry, vty);
	}

	PRINT_SUMMARY;
	
    return CMD_SUCCESS;
}


DEFUN(new_application,
      new_application_cmd,
      "application WORD",
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR)
{
	vlib_appl_main_t *am = &vlib_appl_main;
	struct appl_t *appl;

	appl_table_entry_deep_lookup((const char *)argv[0], &appl);
	if (likely(appl)) {
		VTY_ERROR_APPLICATION ("same", argv[0]);
		appl_entry_output (appl, vty);
		return CMD_SUCCESS;
	} 

	appl_entry_new (&appl, (const char *)argv[0], APPL_TYPE_STREAM);
	if (unlikely (!appl)) {
		VTY_ERROR_APPLICATION ("alloc", argv[0]);
		return CMD_SUCCESS;
	}

	/** Add appl to hash table. */
	if (appl_entry_add (am, appl)) {
		VTY_ERROR_APPLICATION("add", argv[0]);
	}
			
    return CMD_SUCCESS;
}

DEFUN(new_application1,
      new_application_cmd1,
      "application WORD vlan (any|RANGE|<1-4095>) ip_src (any|A.B.C.D/M) ip_dst (any|A.B.C.D/M) port_src (any|RANGE|<1-65535>) port_dst (any|RANGE|<1-65535>) proto (any|RANGE|<1-255>)",
	KEEP_QUITE_STR KEEP_QUITE_CSTR
	KEEP_QUITE_STR KEEP_QUITE_CSTR
	KEEP_QUITE_STR KEEP_QUITE_CSTR
	KEEP_QUITE_STR KEEP_QUITE_CSTR
	KEEP_QUITE_STR KEEP_QUITE_CSTR
	KEEP_QUITE_STR KEEP_QUITE_CSTR
	KEEP_QUITE_STR KEEP_QUITE_CSTR
	KEEP_QUITE_STR KEEP_QUITE_CSTR
	KEEP_QUITE_STR KEEP_QUITE_CSTR
	KEEP_QUITE_STR KEEP_QUITE_CSTR
	KEEP_QUITE_STR KEEP_QUITE_CSTR
	KEEP_QUITE_STR KEEP_QUITE_CSTR
	KEEP_QUITE_STR KEEP_QUITE_CSTR
	KEEP_QUITE_STR KEEP_QUITE_CSTR
	KEEP_QUITE_STR KEEP_QUITE_CSTR
	KEEP_QUITE_STR KEEP_QUITE_CSTR
	KEEP_QUITE_STR KEEP_QUITE_CSTR
	KEEP_QUITE_STR KEEP_QUITE_CSTR
	KEEP_QUITE_STR KEEP_QUITE_CSTR
	KEEP_QUITE_STR KEEP_QUITE_CSTR
	KEEP_QUITE_STR KEEP_QUITE_CSTR
	KEEP_QUITE_STR KEEP_QUITE_CSTR
	KEEP_QUITE_STR KEEP_QUITE_CSTR)
{
	vlib_appl_main_t *am = &vlib_appl_main;
	struct appl_t *appl = NULL;

	appl_table_entry_deep_lookup((const char *)argv[0], &appl);
	if (likely(appl)) {
		VTY_ERROR_APPLICATION ("same", argv[0]);
		appl_entry_output (appl, vty);
		return CMD_SUCCESS;
	}

	appl_entry_new (&appl, (const char *)argv[0], APPL_TYPE_STREAM);
	if (unlikely (!appl)) {
		VTY_ERROR_APPLICATION ("alloc", argv[0]);
		return CMD_SUCCESS;
	}
	
	if (appl_entry_format (appl, NULL, (const char *)"unused var", 
					(const char *)argv[1]	/** VLAN */, 
					(const char *)argv[2]	/** IPSRC */, 
					(const char *)argv[3]	/** IPDST */, 
					(const char *)argv[4]	/** PORTSRC */, 
					(const char *)argv[5]	/** PORTDST */, 
					(const char *)argv[6])/** PROTO */) {
		vty_out(vty, "error command %s", VTY_NEWLINE);
	}
	
	/** Add appl to hash table. */
	if (appl_entry_add (am, appl)) {
		VTY_ERROR_APPLICATION("add", argv[0]);
	}
	
    return CMD_SUCCESS;
}

DEFUN(set_application_desensitize,
      set_application_desensitize_cmd,
      "application WORD desensitize KEYWORD",
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR)
{
	vlib_appl_main_t *am = &vlib_appl_main;
	split_foreach_application_func1_param2 (argv[0], 
		appl_entry_desenitize, vty, (const char *)argv[1]);

	PRINT_SUMMARY;
	
	return CMD_SUCCESS;
}

DEFUN(set_application_priority,
	set_application_priority_cmd,
	"application WORD priority <1-128>",
	KEEP_QUITE_STR KEEP_QUITE_CSTR
	KEEP_QUITE_STR KEEP_QUITE_CSTR
	KEEP_QUITE_STR KEEP_QUITE_CSTR
	KEEP_QUITE_STR KEEP_QUITE_CSTR)
{
  vlib_appl_main_t *am = &vlib_appl_main;
  split_foreach_application_func1_param2 (argv[0], 
	  appl_entry_priority, vty, (const char *)argv[1]);

  PRINT_SUMMARY;
  
  return CMD_SUCCESS;
}

DEFUN(test_application,
	test_application_cmd,
	"test application",
	KEEP_QUITE_STR KEEP_QUITE_CSTR
	KEEP_QUITE_STR KEEP_QUITE_CSTR)
{
  const char *range	= "150:180";
  const char *mask	= "150/255";
  uint32_t val_start, val_end;
  uint32_t val, val_mask;
  int ret, i;
  char alias[32] = {0};
  char ip_src[32], ip_dst[32];
  vlib_appl_main_t *am	= &vlib_appl_main;
  struct appl_t *appl	= NULL;
	
  ret = format_range (range, UINT16_MAX, 0, ':', &val_start, &val_end);
  vty_out (vty, "%s ret = %d, start %d end %d%s", range,
	  ret, val_start, val_end, VTY_NEWLINE);

  ret = format_range (mask, UINT16_MAX, 0, '/', &val, &val_mask);
  vty_out (vty, "%s ret = %d, %d/%d%s", mask,
	  ret, val, val_mask, VTY_NEWLINE);

  for (i = 0; i < MAX_APPLICATIONS; i ++) {
     memset(alias, 0, 31);
     oryx_pattern_generate(alias, 16);
	  appl_table_entry_deep_lookup((const char *)&alias[0], &appl);
	  if (likely(appl)) {
		  VTY_ERROR_APPLICATION ("same", (char *)&alias[0]);
		  appl_entry_output (appl, vty);
		  return CMD_SUCCESS;
	  }

	  oryx_ipaddr_generate(ip_src);
	  oryx_ipaddr_generate(ip_dst);
	  
	  appl_entry_new (&appl, (const char *)&alias[0], APPL_TYPE_STREAM);
	  if (unlikely (!appl)) {
		  VTY_ERROR_APPLICATION ("alloc", (char *)&alias[0]);
		  return CMD_SUCCESS;
	  }
	  
	  if(appl_entry_format (appl, NULL,
			  	  (const char *)"unused var", 
				  (const char *)"any" /** VLAN */, 
				  (const char *)&ip_src[0] /** IPSRC */, 
				  (const char *)&ip_dst[0] /** IPDST */, 
				  (const char *)"any" /** PORTSRC */, 
				  (const char *)"any" /** PORTDST */, 
				  (const char *)"any")/** PROTO */) {
		  vty_out(vty, "error command %s", VTY_NEWLINE);
	  }
	  
	  /** Add appl to hash table. */
	  if (appl_entry_add (am, appl)) {
		  VTY_ERROR_APPLICATION("add", argv[0]);
	  }
  }
  return CMD_SUCCESS;
}

void appl_config_write(struct vty *vty)
{
	int each;
	vlib_appl_main_t *am = &vlib_appl_main;
	struct appl_t *v;
	char fmt_buf[1024] = {0};

	vec_foreach_element(am->entry_vec, each, v){
		if (v && (v->ul_flags & APPL_VALID)) {
			memset (fmt_buf, 0, 1024);
			appl_entry_unformat(v, fmt_buf, 1024);
			vty_out(vty, "%s%s", fmt_buf, VTY_NEWLINE);
		}
	}
}

void vlib_appl_init(vlib_main_t *vm)
{
	vlib_appl_main_t *am = &vlib_appl_main;

	am->vm			= vm;
	am->entry_vec	= vec_init (MAX_APPLICATIONS);
	am->htable		= oryx_htable_init(DEFAULT_HASH_CHAIN_SIZE, 
							ht_appl_hval, ht_appl_cmp, ht_appl_free, 0);
	
	if (am->htable == NULL || am->entry_vec == NULL)
		oryx_panic(-1, 
			"vlib appl main init error!");

	
	install_element (CONFIG_NODE, &show_application_cmd);
	install_element (CONFIG_NODE, &new_application_cmd);
	install_element (CONFIG_NODE, &new_application_cmd1);
	install_element (CONFIG_NODE, &set_application_desensitize_cmd);
	install_element (CONFIG_NODE, &set_application_priority_cmd);
	install_element (CONFIG_NODE, &no_application_cmd);

	install_element (CONFIG_NODE, &test_application_cmd);

	vm->ul_flags |= VLIB_APP_INITIALIZED;
}

