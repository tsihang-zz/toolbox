#include "oryx.h"
#include "vty.h"
#include "command.h"
#include "prefix.h"
#include "common_private.h"
#include "util_ipset.h"
#include "cli_appl.h"

#include "rc4.h"

vlib_appl_main_t vlib_appl_main = {
	.lock = INIT_MUTEX_VAL,
};

atomic_t n_application_elements = ATOMIC_INIT(0);

#define PRINT_SUMMARY	\
		vty_out (vty, "matched %d element(s), total %d element(s)%s", \
			atomic_read(&n_application_elements), (int)vec_count(am->entry_vec), VTY_NEWLINE);

static __oryx_always_inline__
void appl_free (void __oryx_unused__ *v)
{
	v = v; /** To avoid warnings. */

}

static uint32_t
appl_hval (struct oryx_htable_t *ht,
		void *v, uint32_t s) 
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
appl_cmp (void *v1, 
		uint32_t s1,
		void *v2,
		uint32_t s2)
{
	int xret = 0;

	if (!v1 || !v2 ||s1 != s2 ||
		memcmp(v1, v2, s2))
		xret = 1;
	
#ifdef APPL_DEBUG
	//printf ("(%s, %s), xret = %d\n", (char *)v1, (char *)v2, xret);
#endif
	return xret;
}

#define VTY_ERROR_APPLICATION(prefix, alias)\
			vty_out (vty, "%s(Error)%s %s application \"%s\"%s", \
				draw_color(COLOR_RED), draw_color(COLOR_FIN), prefix, alias, VTY_NEWLINE)
		
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
	if (!appl || !(appl->ul_flags & APPL_VALID)) {
		return -1;
	}

	struct prefix *p;
	u8 pfx_buf[SRC_DST][INET_ADDRSTRLEN];
	u8 port_buf[SRC_DST][16];
	u8 proto_buf[32];
	struct prefix_ipv4 ip4;
	
	char tmstr[100];
	tm_format (appl->ull_create_time, "%Y-%m-%d,%H:%M:%S", (char *)&tmstr[0], 100);

	if(appl->ip_src_mask == ANY_IPADDR) {
		sprintf((char *)&pfx_buf[HD_SRC][0], "%s", "any");
	} else {
		ip4.family = AF_INET;
		ip4.prefixlen = appl->ip_src_mask;
		ip4.prefix.s_addr = hton32(appl->ip_src);
		p = (struct prefix *)&ip4;
		prefix2str_ (p, (char *)&pfx_buf[HD_SRC][0], INET_ADDRSTRLEN);
	}

	if(appl->ip_dst_mask == ANY_IPADDR) {
		sprintf((char *)&pfx_buf[HD_SRC][0], "%s", "any");
	} else {
		ip4.family = AF_INET;
		ip4.prefixlen = appl->ip_dst_mask;
		ip4.prefix.s_addr = hton32(appl->ip_dst);
		p = (struct prefix *)&ip4;
		prefix2str_ (p, (char *)&pfx_buf[HD_DST][0], INET_ADDRSTRLEN);
	}

	if(appl->l4_port_src_mask == ANY_PORT) {
		sprintf ((char *)&port_buf[HD_SRC][0], "%s", "any");
	} else {
		sprintf ((char *)&port_buf[HD_SRC][0], "%d:%d", appl->l4_port_src, appl->l4_port_src_mask);
	}

	if(appl->l4_port_dst_mask == ANY_PORT) {
		sprintf ((char *)&port_buf[HD_DST][0], "%s", "any");
	} else {
		sprintf ((char *)&port_buf[HD_DST][0], "%d:%d", appl->l4_port_dst, appl->l4_port_dst_mask);
	}

	if(appl->ip_next_proto_mask == ANY_PROTO) {
		sprintf ((char *)&proto_buf[0], "%s", "any");
	} else {
		sprintf ((char *)&proto_buf[0], "%d:%d", appl->ip_next_proto, appl->ip_next_proto_mask);
	}
	
	vty_out (vty, "%24s%4u%20s%20s%10s%10s%10s%12s%08x%23s%s", 
		appl_alias(appl),
		appl_id(appl),
		pfx_buf[HD_SRC],
		pfx_buf[HD_DST],
		port_buf[HD_SRC],
		port_buf[HD_DST],
		proto_buf, 
		(appl->ul_flags & APPL_SYNCED) ? "synced" : "dis-synced",
		appl->ul_map_mask,
		tmstr,
		VTY_NEWLINE);

	return 0;
}

static int appl_table_entry_remove (struct appl_t *a)
{
	vlib_appl_main_t *am = &vlib_appl_main;
	return appl_entry_del(am, a);
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
	vty_out (vty, "Trying to display %d application elements ...%s", vec_active(am->entry_vec), VTY_NEWLINE);
	vty_out (vty, "%24s%4s%20s%20s%10s%10s%10s%12s%8s%23s%s", 
		"ALIAS", "ID", "IP-SRC", "IP-DST", "PORT-SRC", "PORT-DST", "PROTO", "STATEs", "MAP", "CREAT-TIME", VTY_NEWLINE);
	
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
		foreach_application_func1_param0(argv[0], 
			appl_table_entry_remove);
	}
	else {
		split_foreach_application_func1_param0(argv[0], 
			appl_table_entry_remove);
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
		VTY_ERROR_APPLICATION ("same", (char *)argv[0]);
		appl_entry_output (appl, vty);
		return CMD_SUCCESS;
	} 

	appl_entry_new (&appl, (char *)argv[0], APPL_TYPE_STREAM);
	if (unlikely (!appl)) {
		VTY_ERROR_APPLICATION ("alloc", (char *)argv[0]);
		return CMD_SUCCESS;
	}

	/** Add appl to hash table. */
	if (appl_entry_add (am, appl)) {
		VTY_ERROR_APPLICATION("add", (char *)argv[0]);
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
		VTY_ERROR_APPLICATION ("same", (char *)argv[0]);
		appl_entry_output (appl, vty);
		return CMD_SUCCESS;
	}

	appl_entry_new (&appl, (char *)argv[0], APPL_TYPE_STREAM);
	if (unlikely (!appl)) {
		VTY_ERROR_APPLICATION ("alloc", (char *)argv[0]);
		return CMD_SUCCESS;
	}
	
	if(appl_entry_format (appl, NULL, "unused var", 
		(char *)argv[1]	/** VLAN */, 
		(char *)argv[2]	/** IPSRC */, 
		(char *)argv[3]	/** IPDST */, 
		(char *)argv[4]	/** PORTSRC */, 
		(char *)argv[5]	/** PORTDST */, 
		(char *)argv[6])/** PROTO */
	) {
		vty_out(vty, "error command %s", VTY_NEWLINE);
	}
	
	/** Add appl to hash table. */
	if (appl_entry_add (am, appl)) {
		VTY_ERROR_APPLICATION("add", (char *)argv[0]);
	}
			
    return CMD_SUCCESS;
}
static int appl_entry_desenitize (struct appl_t *appl, struct vty *vty, char *keyword)
{
	char keyword_backup[256] = {0};

	
	if (appl->sc_keyword) {
		memcpy (keyword_backup, appl->sc_keyword, strlen (appl->sc_keyword));
		free (appl->sc_keyword);
	}
	
	if (keyword) {
		appl->sc_keyword = strdup (keyword);
		/** Init rc4. */
		rc4_init (appl->uc_keyword_encrypt, (u8 *)appl->sc_keyword, (u64)strlen (appl->sc_keyword));
		VTY_SUCCESS_APPLICATION ("Encrypt-Keyword", appl);
		vty_out (vty, "%s -> %s", keyword_backup, appl->sc_keyword);
		vty_newline(vty);
	}

	return 0;
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
		appl_entry_desenitize, vty, (char *)argv[1]);

	PRINT_SUMMARY;
	
	return CMD_SUCCESS;
}

DEFUN(test_range,
	test_range_cmd,
	"show RANGE",
	KEEP_QUITE_STR KEEP_QUITE_CSTR
	KEEP_QUITE_STR KEEP_QUITE_CSTR)
{
  uint32_t val_start, val_end;
  int ret;
  
  ret = format_range (argv[0], UINT16_MAX, 0, ':', &val_start, &val_end);
  vty_out (vty, "%s ret = %d, start %d end %d%s", argv[0],
	  ret, val_start, val_end, VTY_NEWLINE);

  return CMD_SUCCESS;
}

void appl_init(vlib_main_t *vm)
{
	vlib_appl_main_t *am = &vlib_appl_main;
	
	am->htable = oryx_htable_init(DEFAULT_HASH_CHAIN_SIZE, 
			appl_hval, appl_cmp, appl_free, 0);
	am->entry_vec = vec_init (MAX_APPLICATIONS);
	
	if (am->htable == NULL || am->entry_vec == NULL) {
		printf ("vlib application main init error!\n");
		exit(0);
	}
	
	install_element (CONFIG_NODE, &show_application_cmd);
	install_element (CONFIG_NODE, &new_application_cmd);
	install_element (CONFIG_NODE, &new_application_cmd1);
	install_element (CONFIG_NODE, &set_application_desensitize_cmd);
	install_element (CONFIG_NODE, &no_application_cmd);

	install_element (CONFIG_NODE, &test_range_cmd);

	vm->ul_flags |= VLIB_APP_INITIALIZED;
}

