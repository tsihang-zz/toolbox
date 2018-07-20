#ifndef CONSISTENT_HASH_H
#define CONSISTENT_HASH_H


#define NODE_FLG_INITED		(1 << 0)
#define NODE_FLG_CLASSFY_TRAVEL	(1 << 1)
#define NODE_FLG_INVALID	(1 << 2)

#define NODE_DEFAULT_VNS	160

/*
  * Real Instance Node structure definnition.
  * Real instance node is set up in a cluster for data store and proccess..
  */
struct node_t {
	
	char idesc [32];	/** Discripter for this real node instance, 
						and should be uniquel. */

	char ipaddr[32];
						
	int replicas;	/** Pre-allocated virtual node count for this real node instance. */	

	int valid_vns;	/** Validate virtual nodes within this real node instance. */
	
	int flags;		/** State & runtime options. */

	struct list_head node;

	uint32_t hits;	/** For hit testing. */
};

#define N_HITS_INC(n) ((n)->hits ++)
#define N_HITS(n) ((n)->hits)
#define N_VALID_VNS(n) ((n)->valid_vns)
#define N_VALID_VNS_INC(n) ((n)->valid_vns ++)

/*
  * Virtual Node structure definnition.
  * Virtual node is used to load balance within a cluster.
  */
struct vnode_t {
	
	union {
	    const char *s;	/** A character pointer when the key's format is a string.*/
	    intptr_t p;		/** A pointer when the key's format is a address */
	    uint32_t i;		/** A specified unsigned integer key, 
	    					always used with hash function. */
	} key;		/** The key for this vnode */

	void *physical_node;		/** Poninter to ITS real instance node */
	char *videsc;		/** Virtual node descriptions */
	int index;		/** Index within a real instance */
	
	struct rb_node node;	/** We are trying to store virtual node on a red black tree. 
							Red-Black tree is always used for node-finding, 
							fast-querying etc. */
	uint32_t hits;	/** For hit testing. */
};

#define VN_HITS_INC(vn) ((vn)->hits ++)
#define VN_KEY_P(vn) (vn->key.p)
#define VN_KEY_S(vn) (vn->key.s)
#define VN_KEY_I(vn) (vn->key.i)


typedef uint32_t (*hash_fun_ptr)(char *, size_t);

/*
  * Consistent Hash Root structure definnition.
  * Consistent hash.
  */
struct chash_root {
	
	struct rb_root vn_root;	/** Virtual node RB root, we do not have a IPC lock for rbtree. */

	struct rb_node *vn_max;	/** Maximum key value for current 32 bit key mapping region.
								Return the real node instance that has a  maximum key value if needed.
								It's one of the query policies when we can not locate a real node instance 
								with a known key by consistent hash algorithms.*/

	struct rb_node *vn_min;	/** Miniimum key value for current 32 bit key mapping region. 
								Return the real node instance that has a  minimum key value if needed.
								It's one of the query policies when we can not locate a real node instance 
								with a known key by consistent hash algorithms.*/
	
	int total_ns;				/** Total count of real node instance. */
	int total_hit_times;

	hash_fun_ptr hash_func;	/** Hash algorithms function. */

	struct list_head node_head;	/** List stored all real node instance. */
	os_mutex_t  nhlock;	/** Node head lock */


};

#define VN_DEFAULT(ch,default)\
	if (likely (ch) && likely (ch->vn_min))\
		default = rb_entry(ch->vn_min, struct vnode_t, node);

extern void * _n_new ();
extern void * _n_clone (struct node_t *n);
extern void *_vn_find_ring (struct chash_root *ch, const void *key);
extern struct node_t *node_lookup (struct chash_root *ch, char *key);
extern void node_install (struct chash_root *ch, struct node_t *n);
extern void node_set (struct node_t *n, char *desc, char *ipaddr, int replicas);

#endif

