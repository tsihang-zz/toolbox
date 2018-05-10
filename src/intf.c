#include "oryx.h"

#if defined(HAVE_QUAGGA)
#include "vty.h"
#include "command.h"
#include "prefix.h"
#endif

#include "et1500.h"

vector panel_intfs_vlan_table;
DECLARE_TABLE(panel_port);

#define PORT_LOCK
#ifdef PORT_LOCK
static INIT_MUTEX(port_lock);
#else
#undef do_lock(lock)
#undef do_unlock(lock)
#define do_lock(lock)
#define do_unlock(lock)
#endif

#define VTY_ERROR_PORT(prefix, alias)\
	vty_out (vty, "%s(Error)%s %s port \"%s\"%s", \
		draw_color(COLOR_RED), draw_color(COLOR_FIN), prefix, alias, VTY_NEWLINE)

#define VTY_SUCCESS_PORT(prefix, v)\
	vty_out (vty, "%s(Success)%s %s port \"%s\"(%u)%s", \
		draw_color(COLOR_GREEN), draw_color(COLOR_FIN), prefix, v->sc_alias, v->ul_id, VTY_NEWLINE)

static __oryx_always_inline__
void mk_alias (struct port_t *entry)
{

	switch (entry->uc_speed) {
	case INTF_SPEED_1G:
			sprintf (entry->sc_alias, "%s%u", (char *)"G0/", entry->ul_id);
			break;
	case INTF_SPEED_10G:
			sprintf (entry->sc_alias, "%s%u", (char *)"XG0/", entry->ul_id);
			break;
	case INTF_SPEED_40G:
			sprintf (entry->sc_alias, "%s%u", (char *)"4XG0/", entry->ul_id);
			break;
	case INTF_SPEED_100G:
			sprintf (entry->sc_alias, "%s%u", (char *)"10XG0/", entry->ul_id);
			break;
	default:
			sprintf (entry->sc_alias, "%s%u", (char *)"UnknownG0/", entry->ul_id);
			break;
	}
}


static __oryx_always_inline__
void port_free (ht_value_t v)
{
	/** Never free here! */
	v = v;
}

static uint32_t
port_hval (struct oryx_htable_t *ht,
		ht_value_t v, uint32_t s) 
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
port_cmp (ht_value_t v1, 
		uint32_t s1,
		ht_value_t v2,
		uint32_t s2)
{
	int xret = 0;

	if (!v1 || !v2 ||s1 != s2 ||
		memcmp(v1, v2, s2))
		xret = 1;	/** Compare failed. */
	
#ifdef APPL_DEBUG
	//printf ("(%s, %s), xret = %d\n", (char *)v1, (char *)v2, xret);
#endif
	return xret;
}

static __oryx_always_inline__
void port_entry_new (struct port_t **port, u32 id)
{
	/** create an port */
	(*port) = kmalloc (sizeof (struct port_t), MPF_CLR, __oryx_unused_val__);

	ASSERT ((*port));

	(*port)->ul_id = id;
	(*port)->us_mtu = 1500;
	(*port)->belong_maps = vector_init(1024);
	/** make port alias. */
	mk_alias (*port);
		
}

static void port_entry_destroy (struct port_t *port)
{
	vector_free (port->belong_maps);
	kfree (port);
}

/** if a null port specified, map_entry_output display all */
static __oryx_always_inline__
void port_entry_output (struct port_t *port, struct vty *vty)
{

	ASSERT (port);

	printout (vty, "%s", VTY_NEWLINE);
	/** find this port named 'alias'. */
	printout (vty, "%16s\"%s\"(%u)%s", "Port ", port->sc_alias, port->ul_id, VTY_NEWLINE);

	printout (vty, "		%16s%s%s", 
		"Duplex: ", 
		(port->ul_flags & NB_INTF_FLAGS_FULL_DUPLEX) ? "Full" : "Half",
		VTY_NEWLINE);
	printout (vty, "		%16s%s%s", 
		"States: ", 
		(port->ul_flags & NB_INTF_FLAGS_LINKUP) ? "Up" : "Down", 
		VTY_NEWLINE);
	printout (vty, "		%16s%s%s", 
		"Type: ", 
		(port->ul_flags & NB_INTF_FLAGS_NETWORK) ? "Network" : "Tool", 
		VTY_NEWLINE);
	printout (vty, "		%16s%s%s", 
		"ForceUP: ", 
		(port->ul_flags & NB_INTF_FLAGS_FORCEDUP) ? "Yes" : "No", 
		VTY_NEWLINE);

	printout (vty, "		%16s%d%s", 
		"MTU: ", 
		port->us_mtu, 
		VTY_NEWLINE);

	printout (vty, "		%16s", "Maps: ");

	vector v = port->belong_maps;
	int actives = vector_active(v);
	if (!actives) printout (vty, "N/A%s", VTY_NEWLINE);
	else {
		int i;
		
		struct map_t *p;
		vector_foreach_element (v, i, p) {
			if (p) {
				printout (vty, "%s", p->sc_alias);
				-- actives;
				if (actives) printout (vty, ", ");
				else actives = actives;
			}
			else {p = p;}
		}
		printout (vty, "%s", VTY_NEWLINE);
	}
	printout (vty, "%16s%s", "_______________________________________________", VTY_NEWLINE);

}

	
/** if a null port specified, map_entry_output display all */
static __oryx_always_inline__
void port_entry_stat_output (struct port_t *port, struct vty *vty)
{

	char format[256] = {0};
	
	if (unlikely (!port)) 
		return;
	{

		/** find this port named 'alias'. */
		printout (vty, "%15s(%u)", port->sc_alias, port->ul_id);

		sprintf (format, "%llu/%llu", PORT_INBYTE(port), PORT_OUTBYTE(port));
		printout (vty, "%20s", format);

		sprintf (format, "%llu/%llu", PORT_INPKT(port), PORT_OUTPKT(port));
		printout (vty, "%20s", format);


		vty_newline(vty);
	} 
}

static __oryx_always_inline__
void do_port_activity_check(struct port_t *p) {

	/** update port activity flags. */
	/*
	 * TODO.
	 */
	
	if (p->ul_flags & NB_INTF_FLAGS_LINKUP)
		return;
	else {
		/** remap port if in a specified map. */
		;
	}

	return;
}

static __oryx_always_inline__
void port_activity_prob_tmr_handler(struct oryx_timer_t *tmr, int __oryx_unused__ argc, 
                char **argv)
{
	int foreach_element;
	vector vec;
	struct port_t *p;

	vec = TABLE(panel_port)->vec;

	vector_foreach_element(vec, foreach_element, p){
		if (likely(p)) {
			do_port_activity_check(p);
		}
	}
}

void port_activity_tmr_init(void){
	struct oryx_timer_t *activity_tmr;
	uint32_t ul_activity_tmr_setting_flags = TMR_OPTIONS_PERIODIC | TMR_OPTIONS_ADVANCED;

	activity_tmr = oryx_tmr_create(1, "port activity monitoring tmr", 
							ul_activity_tmr_setting_flags,
							port_activity_prob_tmr_handler, 
							0, NULL, 3000);

	if(likely(activity_tmr))
		oryx_tmr_start(activity_tmr);
}

static __oryx_always_inline__
void do_port_rename (struct port_t *port, struct prefix_t *new_alias)
{

	/** Delete old alias from hash table. */
	oryx_htable_del (TABLE(panel_port)->ht, port_alias(port), strlen (port_alias(port)));
	memset (port_alias(port), 0, strlen (port_alias(port)));
	memcpy (port_alias(port), (char *)new_alias->v, 
		strlen ((char *)new_alias->v));
	/** New alias should be rewrite to hash table. */
	oryx_htable_add (TABLE(panel_port)->ht, port_alias(port), strlen (port_alias(port)));		
}

static __oryx_always_inline__
void port_entry_config (struct port_t *port, void __oryx_unused__ *vty_, void *arg)
{

	struct vty *vty = vty_;
	struct prefix_t *var;
	u32 ul_flags = port->ul_flags;
	
	var = (struct prefix_t *)arg;

	switch (var->cmd)
	{
		case INTERFACE_SET_ALIAS:
			if (strlen ((char *)var->v) > sizeof (port->sc_alias)) {
				VTY_ERROR_PORT("invalid alias", (char *)port->sc_alias);
				break;
			}
			struct port_t *exist;/** check same alias. */
			struct prefix_t lp = {
				.cmd = LOOKUP_ALIAS,
				.v = var->v,
				.s = __oryx_unused_val__,
			};
			port_table_entry_lookup(&lp, &exist);
			if (exist) {
				VTY_ERROR_PORT("same", (char *)exist->sc_alias);
				break;
			}
			do_port_rename (port, var);
			break;
			
		case INTERFACE_SET_MTU:
			port->us_mtu = atoi (var->v);
			break;

		case INTERFACE_SET_LOOPBACK:
			ul_flags |= NB_INTF_FLAGS_LOOPBACK;
			if (!strncmp ((char *)var->v, "d", 1))
				ul_flags &= ~NB_INTF_FLAGS_LOOPBACK;
			port->ul_flags = ul_flags;
			break;
			
		case INTERFACE_CLEAR_STATS:
			PORT_INBYTE(port) = PORT_INPKT(port) = 0;
			PORT_OUTBYTE(port) = PORT_OUTPKT(port) = 0;
			break;
			
		default:
			break;
	}
}

static __oryx_always_inline__
int port_table_entry_remove (struct port_t *port)
{

	do_lock (&port_lock);
	
	/**
	  * Delete port alias from hash table.
	  */
	int r = oryx_htable_del(TABLE(panel_port)->ht, port->sc_alias, strlen((const char *)port->sc_alias));
	if (r == 0) {
		/** 
		  * Delete port entry from vector
		  */
		vector_unset (TABLE(panel_port)->vec, port->ul_id);
	}

	do_unlock (&port_lock);

	/** Should you free port here ? */

	return r;  
}

static __oryx_always_inline__
void port_table_entry_lookup_alias (char *alias, struct port_t **p)
{
	(*p) = NULL;

	/** ALIASE validate check. */
	if (unlikely (!alias)) 
		return;

	void *s = oryx_htable_lookup(TABLE(panel_port)->ht, alias, strlen(alias));

	if (likely (s)) {
		/** THIS IS A VERY CRITICAL POINT. */
		do_lock (&port_lock);
		(*p) = (struct port_t *) container_of (s, struct port_t, sc_alias);
		do_unlock (&port_lock);
	}
}

static __oryx_always_inline__
void port_table_entry_lookup_id (u32 id, struct port_t **p)
{
	/** ID validate check. */

	(*p) = NULL;

	do_lock (&port_lock);
	(*p) = (struct port_t *) vector_lookup_ensure (TABLE(panel_port)->vec, id);
	do_unlock (&port_lock);

}
    
__oryx_always_extern__
int port_table_entry_add (struct port_t *port)
{

	ASSERT (port);
		
	do_lock (&port_lock);
	
	/**
	  * Add port->sc_alias to hash table for controlplane fast lookup.
	  */
	int r = oryx_htable_add(TABLE(panel_port)->ht, port->sc_alias, strlen((const char *)port->sc_alias));
	if (r == 0) {
		/**
		  * Add port to a vector table for dataplane fast lookup. 
	  	  */
		vector_set_index (TABLE(panel_port)->vec, port->ul_id, port);

		port->table = TABLE(panel_port);
	}

	do_unlock (&port_lock);

	return r;
}

static int port_table_entry_remove_and_destroy (struct port_t *port)
{

	int r;

	ASSERT (port);
	
	r = port_table_entry_remove (port);
	if (likely (r)) {
		port_entry_destroy (port);
	}
	
	return r;  
}

void port_table_entry_lookup (struct prefix_t *lp, 
	struct port_t **p)
{

	ASSERT (lp);
	ASSERT (p);
	
	switch (lp->cmd) {
		case LOOKUP_ID:
			port_table_entry_lookup_id ((*(u32*)lp->v), p);
			break;

		case LOOKUP_ALIAS:
			port_table_entry_lookup_alias ((char*)lp->v, p);
			break;

		default:
			break;
	}
}

#if defined(HAVE_QUAGGA)

atomic_t n_intf_elements = ATOMIC_INIT(0);

#define PRINT_SUMMARY	\
	printout (vty, "matched %d element(s), total %d element(s)%s", \
		atomic_read(&n_intf_elements), (int)vector_count(TABLE(panel_port)->vec), VTY_NEWLINE);

DEFUN(show_interfacce,
      show_interface_cmd,
      "show interface [WORD]",
      SHOW_STR SHOW_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR)
{

	printout (vty, "Trying to display %s%d%s elements ...%s", 
		draw_color(COLOR_RED), vector_active(TABLE(panel_port)->vec), draw_color(COLOR_FIN), 
		VTY_NEWLINE);
	
	printout (vty, "%16s%s", "_______________________________________________", VTY_NEWLINE);

	if (argc == 0) {
		foreach_port_func1_param1 (
			argv[0], port_entry_output, vty);
	}
	else {
		split_foreach_port_func1_param1 (
			argv[0], port_entry_output, vty);
	}

	PRINT_SUMMARY;

    return CMD_SUCCESS;
}

DEFUN(show_interfacce_stats,
      show_interfacce_stats_cmd,
      "show interface stats [WORD]",
      SHOW_STR SHOW_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR)
{

	printout (vty, "Trying to display %d elements ...%s", 
			vector_active(TABLE(panel_port)->vec), VTY_NEWLINE);
	printout (vty, "%15s%20s%20s%s", "Port", "Bytes(I/O)", "Packets(I/O)", VTY_NEWLINE);
	
	if (argc == 0) {
		foreach_port_func1_param1 (
			argv[0], port_entry_stat_output, vty);
	}
	else {
		split_foreach_port_func1_param1 (
			argv[0], port_entry_stat_output, vty);
	}

	PRINT_SUMMARY;

    return CMD_SUCCESS;
}

DEFUN(interface_alias,
      interface_alias_cmd,
      "interface WORD alias WORD",
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
	  KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR)
{

	struct prefix_t var = {
		.cmd = INTERFACE_SET_ALIAS,
		.v = (char *)argv[1],
		.s = __oryx_unused_val__,
	};
	
	split_foreach_port_func1_param2 (
		argv[0], port_entry_config, vty, (void *)&var);

	PRINT_SUMMARY;
	
	return CMD_SUCCESS;
}

DEFUN(interface_mtu,
      interface_mtu_cmd,
      "interface WORD mtu <64-65535>",
      KEEP_QUITE_STR
      KEEP_QUITE_CSTR
      KEEP_QUITE_STR
      KEEP_QUITE_CSTR
      KEEP_QUITE_STR
      KEEP_QUITE_CSTR
      KEEP_QUITE_STR
      KEEP_QUITE_CSTR)
{

	struct prefix_t var = {
		.cmd = INTERFACE_SET_MTU,
		.v = (char *)argv[1],
		.s = __oryx_unused_val__,
	};
	
	split_foreach_port_func1_param2 (
		argv[0], port_entry_config, vty, (void *)&var);

	PRINT_SUMMARY;
	
	return CMD_SUCCESS;
}

DEFUN(interface_looback,
      interface_looback_cmd,
      "interface WORD loopback (enable|disable)",
      KEEP_QUITE_STR
      KEEP_QUITE_CSTR
      KEEP_QUITE_STR
      KEEP_QUITE_CSTR
      KEEP_QUITE_STR
      KEEP_QUITE_CSTR
      KEEP_QUITE_STR
      KEEP_QUITE_CSTR
      KEEP_QUITE_STR
      KEEP_QUITE_CSTR)
{

	struct prefix_t var = {
		.cmd = INTERFACE_SET_LOOPBACK,
		.v = (char *)argv[1],
		.s = __oryx_unused_val__,
	};
	
	split_foreach_port_func1_param2 (
		argv[0], port_entry_config, vty, (void *)&var);

	PRINT_SUMMARY;
	
	return CMD_SUCCESS;
}

DEFUN(interface_stats_clear,
      interface_stats_clear_cmd,
      "clear interface stats WORD",
      KEEP_QUITE_STR
      KEEP_QUITE_CSTR
      KEEP_QUITE_STR
      KEEP_QUITE_CSTR
      KEEP_QUITE_STR
      KEEP_QUITE_CSTR
      KEEP_QUITE_STR
      KEEP_QUITE_CSTR)
{

	struct prefix_t var = {
		.cmd = INTERFACE_CLEAR_STATS,
		.v = (char *)argv[1],
		.s = __oryx_unused_val__,
	};
	
	split_foreach_port_func1_param2 (
		argv[0], port_entry_config, vty, (void *)&var);

	PRINT_SUMMARY;
	
	return CMD_SUCCESS;
}

#endif

/** A random Pattern generator.*/
static __oryx_always_inline__
void vlan_id_generate (struct port_t *port)
{
#if 0	
	/** VLAN already exist ? */
	do {
		port->ul_vid = random() % 4095;
	} while (vector_lookup_ensure (panel_intfs_vlan_table, entry->ul_vid));
	//vector_set_index (panel_intfs_vlan_table, entry->ul_vid, entry);
#else
	port = port;
#endif

}


void port_init(void)
{

	/**
	 * Init PORT hash table, NAME -> PORT_ID.
	 */
	struct oryx_htable_t *port_hash_table = oryx_htable_init(DEFAULT_HASH_CHAIN_SIZE, 
			port_hval, port_cmp, port_free, 0);
	printf ("[port_hash_table] htable init ");
	
	if (port_hash_table == NULL) {
		printf ("error!\n");
		goto finish;
	}
	printf ("okay\n");
	
       /**
	 * Init VLAN table, VLAN -> INTERFACE INSTANCE.
	 */
	panel_intfs_vlan_table = vector_init (PANEL_N_PORTS);

       /**
	 * Init INTERFACE table, INTERFACE ID -> INTERFACE INSTANCE.
	 */	
	vector port_vector_table = vector_init (PANEL_N_PORTS);

	INIT_TABLE(panel_port, port_vector_table, port_hash_table);
		
#if defined(HAVE_QUAGGA)
	install_element (CONFIG_NODE, &show_interface_cmd);
	install_element (CONFIG_NODE, &show_interfacce_stats_cmd);
	install_element (CONFIG_NODE, &interface_alias_cmd);
	install_element (CONFIG_NODE, &interface_mtu_cmd);
	install_element (CONFIG_NODE, &interface_looback_cmd);
	install_element (CONFIG_NODE, &interface_stats_clear_cmd);
#endif

	port_activity_tmr_init();


#ifndef HAVE_SELF_TEST
#define HAVE_SELF_TEST
#endif

#if defined(HAVE_SELF_TEST)
	/** P O R T   I N I T  */
	int n_intfs = PANEL_N_PORTS + 1;
	u32 id;
	struct port_t *entry;
	
	while (n_intfs -- > 0) {		
		
		id = n_intfs;

		/** Interface entry already exist ? */
		struct prefix_t lp = {
			.cmd = LOOKUP_ID,
			.v = (void *)&id,
			.s = __oryx_unused_val__,
		};

		port_table_entry_lookup (&lp, &entry);
		if (entry != NULL) continue;

		port_entry_new (&entry, id);
		if (!entry) exit (0);

		vlan_id_generate (entry);

		if (n_intfs % 4) {
			entry->ul_flags |= NB_INTF_FLAGS_FORCEDUP;
		}

		if (n_intfs % 1) {
			entry->ul_flags |= NB_INTF_FLAGS_FULL_DUPLEX;
		}
		
		if (n_intfs % 2) {
			entry->ul_flags |= NB_INTF_FLAGS_LINKUP;
		}

		if (n_intfs % 3) {
			entry->ul_flags |= NB_INTF_FLAGS_NETWORK;
		}
		
		port_table_entry_add (entry);
	}

#if 1
	struct port_t *p1, *p2;

	struct prefix_t lp = {
		.cmd = LOOKUP_ALIAS,
		.v = "G0/1",
		.s = strlen ("G0/1"),
	};
	/** Find port which named "G0/1" */
	port_table_entry_lookup (&lp, &p1);
	if (!p1) goto finish;

	lp.cmd = LOOKUP_ID;
	lp.v = (void *)&p1->ul_id;
	port_table_entry_lookup (&lp, &p2);
	if (!p2) goto finish;

	if (p1 != p2) goto finish;
	printf ("Lookup PORT (%p, %p) success\n", p1, p2);

	sleep (1);

	struct prefix_t lp0 = {
		.cmd = LOOKUP_ALIAS,
		.s = strlen ("G0/1"),
		.v = "G0/1",
	};
	port_table_entry_lookup (&lp0, &p1);
	
	id = 1;
	struct prefix_t lp1 = {
		.cmd = LOOKUP_ID,
		.s = strlen ("1"),
		.v = (void *)&id,
	};
	port_table_entry_lookup (&lp1, &p1);
	if (!p1) {
		printf ("------ Lookup PORT (%s) failed\n", (char *)lp1.v);
	}
#endif
#endif

finish:

	return;
	
}


