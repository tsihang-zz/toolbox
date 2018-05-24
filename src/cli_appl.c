#include "oryx.h"
#include "vty.h"
#include "command.h"
#include "prefix.h"
#include "common_private.h"
#include "appl_private.h"
#include "cli_appl.h"

#include "rc4.h"

atomic_t n_application_elements = ATOMIC_INIT(0);
oryx_vector appl_vector_table;

#define PRINT_SUMMARY	\
		vty_out (vty, "matched %d element(s), total %d element(s)%s", \
			atomic_read(&n_application_elements), (int)vec_count(appl_vector_table), VTY_NEWLINE);

/** This element is busy ??
a stub function for applications check. */
#define THIS_ELEMENT_IS_INUNSE(elm) 0

//#define APPL_DEBUG

#define APPL_LOCK
#ifdef APPL_LOCK
static INIT_MUTEX(appl_lock);
#else
#undef do_lock(lock)
#undef do_unlock(lock)
#define do_lock(lock)
#define do_unlock(lock)
#endif

void appl_entry_format (struct appl_t *appl, 
	u32 __oryx_unused__*rule_id, char *unused_var, 
	char __oryx_unused__*vlan, 
	char __oryx_unused__*sip, 
	char __oryx_unused__*dip, 
	char __oryx_unused__*sp, 
	char __oryx_unused__*dp, 
	char __oryx_unused__*proto)
{

	u8 action = APPL_DEFAULT_ACTIONS;
	struct appl_signature_t *as;
	
	if (!appl) return;

	as = appl->instance;
	if (!as) return;

	if (vlan) {
		if (isalldigit (vlan)) 
			as->us_vlan = atoi (vlan);
		else {
			/** default is ANY_VLAN */
			as->us_vlan = ANY_VLAN; /** To avoid warnings. */
		}
	}
	
	if (sip) str2prefix_ipv4 (sip, &as->ip4[L4_PORT_SRC]);
	if (dip) str2prefix_ipv4 (dip, &as->ip4[L4_PORT_DST]);

	/** Format TCP/UDP Port. */
	if (sp) {
		u8 p = L4_PORT_SRC;
		if (isalldigit (sp)) {
			as->us_port[p]  = atoi (sp);
		}
		else {
			/** default is ANY_PORT */
			as->us_port[p] = ANY_PORT; /** To avoid warnings. */
		}
	}
	
	if (dp) {
		u8 p = L4_PORT_DST;
		if (isalldigit (dp)) {
			as->us_port[p]  = atoi (dp);
		}
		else {
			/** default is ANY_PORT */
			as->us_port[p] = ANY_PORT; /** To avoid warnings. */
		}
	}
	
	/** Format TCP/UDP Protocol. */
	if (proto) {
		if (isalldigit (proto)) {
			as->uc_proto  = atoi (proto);
		}
		else {
			/** default is ANY_PROTO */
			as->uc_proto  = ANY_PROTO; /** To avoid warnings. */
		}
	}
}

void appl_entry_new (struct appl_t **appl, char *alias, u32 __oryx_unused__ type)
{
	/** create an appl */
	(*appl) = kmalloc (sizeof (struct appl_t), MPF_CLR, __oryx_unused_val__);
	ASSERT ((*appl));
	
	/** make appl alias. */
	sprintf ((char *)&(*appl)->sc_alias[0], "%s", ((alias != NULL) ? alias: APPL_PREFIX));
	(*appl)->ul_type = APPL_TYPE_STREAM;
	(*appl)->ul_id = APPL_INVALID_ID;
	(*appl)->ul_flags = APPL_DEFAULT_ACTIONS;
	(*appl)->ull_create_time = time(NULL);
	
	(*appl)->instance = kmalloc (sizeof (struct appl_signature_t), MPF_CLR, __oryx_unused_val__);
	ASSERT ((*appl)->instance);
	
	struct appl_signature_t *as = (*appl)->instance;
	as->us_vlan = ANY_VLAN;
	as->uc_proto = ANY_PROTO;
	as->us_port[L4_PORT_SRC] = as->us_port[L4_PORT_DST] = ANY_PORT;
	as->udp = vec_init (12);
}

void appl_entry_destroy (struct appl_t *appl)
{
	kfree (appl->instance);
	kfree (appl);
}

void appl_table_entry_lookup (char *alias, struct appl_t **appl)
{
	vlib_appl_main_t *vp = &vlib_appl_main;
	(*appl) = NULL;
	
	if (!alias) return;

	void *s = oryx_htable_lookup (vp->htable,
		alias, strlen((const char *)alias));

	if (s) {
		(*appl) = (struct appl_t *) container_of (s, struct appl_t, sc_alias);
	}
}


struct appl_t *appl_table_entry_lookup_i (u32 id)
{
	vlib_appl_main_t *vp = &vlib_appl_main;
	return (struct appl_t *) vec_lookup (vp->entry_vec, id);
}

int appl_table_entry_remove (struct appl_t *appl)
{
	vlib_appl_main_t *vp = &vlib_appl_main;
	if (!appl ||
		(appl && 
		appl->ul_id == APPL_INVALID_ID/** Delete appl from oryx_vector */)) {
		return 0;
	}
	
	if (likely(THIS_ELEMENT_IS_INUNSE(appl))) {
		return 0;
	}
	
	do_lock (&appl_lock);
	
	int r = oryx_htable_del(vp->htable, appl->sc_alias, strlen((const char *)appl->sc_alias));

	if (r == 0 /** success */) {
		vec_unset (vp->entry_vec, appl->ul_id);
	}

	do_unlock (&appl_lock);
	
	/** Should you free appl here ? */
	
	return 0;  
}


int appl_table_entry_remove_and_destroy (struct appl_t *appl)
{
	/** Delete appl from oryx_vector */
	appl_table_entry_remove (appl);
	appl_entry_destroy(appl);
	
	return 0;  
}

int appl_table_entry_add (struct appl_t *appl)
{
	vlib_appl_main_t *vp = &vlib_appl_main;
	
	if (appl->ul_id != APPL_INVALID_ID 	/** THIS APPL ??? */) {
		appl = appl;
		/** What should do here ??? */
	} 

	do_lock (&appl_lock);
	
	/** Add appl->sc_alias to hash table for controlplane fast lookup */
	int r = oryx_htable_add(vp->htable, appl->sc_alias, strlen((const char *)appl->sc_alias));
	if (r == 0 /** success*/) {
		/** Add appl to a oryx_vector table for dataplane fast lookup. */
		appl->ul_id = vec_set (vp->entry_vec, appl);
	}

	do_unlock (&appl_lock);
	return r;
}


#if 0
/** add a user defined pattern to existent APPL */
int appl_entry_add_udp (struct appl_t *appl, struct udp_t *udp)
{

	int foreach_signature;
	

	struct udp_t *u;
	struct appl_signature_t *as;

	as = appl->instance;
	if (as) {
		/** check existent. */
		vec_foreach_element (as->udp, foreach_signature, u) {
			if (!u) continue;
			if (u->ul_id  == udp->ul_id){
				printf ("[ERROR]  Same UDP, %s\n", udp->sc_alias);
				return -1;
			}
			printf ("!!!!!!!!!!!!--->    %s",  u->sc_alias);
		}
		int i = vector_set_index (as->udp, udp->ul_id, udp);

		vec_foreach_element (as->udp, foreach_signature, u) {
			/** debug */
			if (u)
			printf ("!!!!!!!!!!!! %s %s %u, %d\n", appl->sc_alias, u->sc_alias, appl->ul_id, i);
		}
	
	}
	return 0;
}
#endif


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

	if (!appl) {
		return -1;
	}

	if (appl->ul_type == APPL_TYPE_STREAM) {
		struct appl_signature_t *as;
		struct prefix *p;
		u8 pfx_buf[L3_IP_ADDR][INET_ADDRSTRLEN];
		u8 port_buf[L4_PORTS][16];
		u8 proto_buf[32];
		
		as = appl->instance;
		if (!as) return 0;

		char tmstr[100];
		tm_format (appl->ull_create_time, "%Y-%m-%d,%H:%M:%S", (char *)&tmstr[0], 100);
		
		p = (struct prefix *)&as->ip4[L3_IP_ADDR_SRC];
		prefix2str_ (p, (char *)&pfx_buf[L3_IP_ADDR_SRC][0], INET_ADDRSTRLEN);

		p = (struct prefix *)&as->ip4[L3_IP_ADDR_DST];
		prefix2str_ (p, (char *)&pfx_buf[L3_IP_ADDR_DST][0], INET_ADDRSTRLEN);

		u8 uc_port;

		uc_port = L4_PORT_SRC;
		if ((as->us_port[uc_port] == 0))
			sprintf ((char *)&port_buf[uc_port][0], "%s", "any");
		else 
			sprintf ((char *)&port_buf[uc_port][0], "%d", as->us_port[uc_port]);
		
		uc_port = L4_PORT_DST;
		if ((as->us_port[uc_port] == 0))
			sprintf ((char *)&port_buf[uc_port][0], "%s", "any");
		else 
			sprintf ((char *)&port_buf[uc_port][0], "%d", as->us_port[uc_port]);

		if ((as->uc_proto == 0))
			sprintf ((char *)&proto_buf[0], "%s", "any");
		else
			sprintf ((char *)&proto_buf[0], "%d", as->uc_proto);

		vty_out (vty, "%24s%4u%20s%20s%10s%10s%10s%8s%23s%s", 
			appl->sc_alias, appl->ul_id, pfx_buf[L3_IP_ADDR_SRC], pfx_buf[L3_IP_ADDR_DST], port_buf[L4_PORT_SRC], port_buf[L4_PORT_DST], proto_buf, 
			CLI_UNDEF_VAL, tmstr, VTY_NEWLINE);

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

	/** display all stream applications. */
	vty_out (vty, "Trying to display %d application elements ...%s", vec_active(appl_vector_table), VTY_NEWLINE);
	vty_out (vty, "%24s%4s%20s%20s%10s%10s%10s%8s%23s%s", 
		"ALIAS", "ID", "IP-SRC", "IP-DST", "PORT-SRC", "PORT-DST", "PROTO", "STATEs", "CREAT-TIME", VTY_NEWLINE);
	
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

	if (argc == 0) {
		foreach_application_func1_param0 (argv[0], 
			appl_table_entry_remove_and_destroy);
	}
	else {
		split_foreach_application_func1_param0 (argv[0], 
			appl_table_entry_remove_and_destroy);
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

	struct appl_t *appl;

	appl_table_entry_lookup ((char *)argv[0], &appl);
	if (likely(appl)) {
		VTY_ERROR_APPLICATION ("same", (char *)argv[0]);
		appl_entry_output (appl, vty);
	} else {
			appl_entry_new (&appl, (char *)argv[0], APPL_TYPE_STREAM);
			if (unlikely (!appl)) {
				VTY_ERROR_APPLICATION ("alloc", (char *)argv[0]);
				return CMD_SUCCESS;
			}

			/** An application without body. */
			
			/** Add appl to hash table. */
			if (appl_table_entry_add (appl)) {
				VTY_ERROR_APPLICATION("add", (char *)argv[0]);
				appl_entry_destroy (appl);
				kfree (appl->instance);
				kfree (appl);
			}
	}
			
    return CMD_SUCCESS;
}

DEFUN(new_application1,
      new_application_cmd1,
      "application WORD vlan (any|<1-4095>) ip_src (any|A.B.C.D/M) ip_dst (any|A.B.C.D/M) port_src (any|<1-65535>) port_dst (any|<1-65535>) proto (any|<1-255>)",
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

	struct appl_t *appl;

	appl_table_entry_lookup ((char *)argv[0], &appl);
	if (likely(appl)) {
		VTY_ERROR_APPLICATION ("same", (char *)argv[0]);
		appl_entry_output (appl, vty);
	} else {
		appl_entry_new (&appl, (char *)argv[0], APPL_TYPE_STREAM);
		if (unlikely (!appl)) {
			VTY_ERROR_APPLICATION ("alloc", (char *)argv[0]);
			return CMD_SUCCESS;
		}
		
		appl_entry_format (appl, NULL, "unused var", 
			(char *)argv[1]	/** VLAN */, 
			(char *)argv[2]	/** IPSRC */, 
			(char *)argv[3]	/** IPDST */, 
			(char *)argv[4]	/** PORTSRC */, 
			(char *)argv[5]	/** PORTDST */, 
			(char *)argv[6])	/** PROTO */;
		
		/** Add appl to hash table. */
		if (appl_table_entry_add (appl)) {
			VTY_ERROR_APPLICATION("add", (char *)argv[0]);
			kfree (appl->instance);
			kfree (appl);
		}
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
	split_foreach_application_func1_param2 (argv[0], 
		appl_entry_desenitize, vty, (char *)argv[1]);

	PRINT_SUMMARY;
	
	return CMD_SUCCESS;
}

void appl_init(vlib_main_t *vm)
{
	vlib_appl_main_t *vp = &vlib_appl_main;
	
	vp->htable = oryx_htable_init(DEFAULT_HASH_CHAIN_SIZE, 
			appl_hval, appl_cmp, appl_free, 0);
	vp->entry_vec = vec_init (MAX_APPLICATIONS);
	
	if (vp->htable == NULL || vp->entry_vec == NULL) {
		printf ("vlib application main init error!\n");
		exit(0);
	}

	appl_vector_table = vlib_appl_main.entry_vec;
	
	install_element (CONFIG_NODE, &show_application_cmd);
	install_element (CONFIG_NODE, &new_application_cmd);
	install_element (CONFIG_NODE, &new_application_cmd1);
	install_element (CONFIG_NODE, &set_application_desensitize_cmd);
	install_element (CONFIG_NODE, &no_application_cmd);

	vm->ul_flags |= VLIB_APP_INITIALIZED;
}

