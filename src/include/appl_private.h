#ifndef APPL_PRIVATE_H
#define APPL_PRIVATE_H

#define APPL_PREFIX	"appl"
#define MAX_APPLICATIONS (1 << 6)	/** 1 million */
#define APPL_INVALID_ID		(MAX_APPLICATIONS)

//#define APPL_DUMP_BANDARY		"%-16s%-16d%-"
#define foreach_appl_type			\
  _(STREAM, "Stream application, 7 tuple.")	\
  _(KEYWORD, "Keyword application")
  
enum appl_type_t {
#define _(f,s) APPL_TYPE_##f,
	foreach_appl_type
#undef _
	APPL_N_TYPES,
};

enum {
	L4_PORT_SRC,
	L4_PORT_DST,
	L4_PORTS,
};

enum {
	L3_IP_ADDR_SRC,
	L3_IP_ADDR_DST,
	L3_IP_ADDR,
};

enum {
	L2_ETHERNET_ADDR_SRC,
	L2_ETHERNET_ADDR_DST,
	L2_ETHERNET,
};

/** vlan=0 is reserved by system, we use this for "any" VLAN.*/
#define ANY_VLAN	(0)
#define ANY_PROTO	(0)
#define ANY_PORT	(0)
#define ANY_IPADDR	(0)


struct appl_signature_t {
	
	/**
	  * Vlan
	  */
	u16 us_vlan;

	/**
	  * ethernet address.
	  */
	/** TODO. */	
	
	/**
	  * ip address for this application.
	  */
	struct prefix_ipv4 ip4[L3_IP_ADDR];

	/**
	  * l4 port for this application.
	  */
	u16 us_port[L4_PORTS];

	/**
	  * Protocol
	  */
	u8 uc_proto;

	/**
	  * User-Defined Pattern for this appl signature.
	  */
	oryx_vector udp;
};

/**
  * Default application action.
  */
#define APPL_DEFAULT_ACTIONS	(0)

struct appl_t {

	/** 
	  * Unique, and can be well human-readable.
	  */
	char sc_alias[32];
	
	/** 
	  * Unique, and can be allocated by a oryx_vector function automatically. 
	  */
	u32 ul_id;

	/** 
	  * We may define as many different applications as we want.
	  */
	u32 ul_type;

	/**
	  * Application instance point to a TCAM entry or RFC entry.
	  */
	void *instance;

	/** 
	  * Defined by appl_action_type_t.
	  * and default ACTIONS defined for an APPL is in APPL_DEFAULT_ACTIONS. 
	  */
	u32 ul_flags;

	/**
	  * Datapath Output is not defined. 
	  */
	u32 ul_dpo;

	/**
	  * RC4 encrypt keyword.
	  */
	u8 uc_keyword_encrypt[256];


	/**
	  * Keyword before enctypt.
	  */
	char *sc_keyword;

	/**
	  * Create time.
	  */
	u64 ull_create_time;

	/**
	  * Lock.
	  */
	os_lock_t ol_lock;
};

#define appl_slot(u) ((u)->ul_id)
#define appl_alias(u) ((u)->sc_alias)

#define appl_entry_lock(appl)\
	(pthread_mutex_lock(&appl->ol_lock))
#define appl_entry_unlock(appl)\
	(pthread_mutex_unlock(&appl->ol_lock))

typedef struct {
	int ul_n_appls;
	u32 ul_flags;
	oryx_vector entry_vec;
	struct oryx_htable_t *htable;
	
}vlib_appl_main_t;

vlib_appl_main_t vlib_appl_main;


#endif

