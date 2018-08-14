#include "oryx.h"
#include "cv_hash.h"

/** Hash function. */
uint32_t hash_algo (const char *instr, size_t s)
{
	return oryx_js_hash (instr, s);
}


/** Virtual node comparison handler which used to find a virtual node with a STR key
	in and registered to a RB root. */
static __oryx_always_inline__
int _vn_cmp (const struct rb_node* pos, const void* ptr)
{
    struct vnode_t* vn = rb_entry(pos, struct vnode_t, node);
    return strcmp((char*)ptr, VN_KEY_S(vn));
}

/** Virtual node comparison handler which used to find a virtual node with a ADDR key
	in and registered to a RB root. */
static __oryx_always_inline__
int _vn_cmpp (const struct rb_node* pos, const void* ptr)
{
    struct vnode_t* vn = rb_entry(pos, struct vnode_t, node);
    return ((intptr_t)ptr - VN_KEY_P(vn));
}

/** Virtual node comparison handler which used to find a virtual node with a INTEGER key
	in and registered to a RB root. */
static __oryx_always_inline__
int _vn_cmpi (const struct rb_node* pos, const void* ptr)
{
    struct vnode_t* vn = rb_entry(pos, struct vnode_t, node);
    return (int)(*(uint32_t *)ptr - VN_KEY_I(vn));
}

/** Virtual node dump handler. */
static __oryx_always_inline__
void _vn_dump (struct vnode_t *vn, int __attribute__((__unused__)) flags)
{

	if (unlikely(!vn))
		return;

	struct node_t * n = vn->physical_node;
		
	fprintf (stdout, "-- \"%s(%d)\"@%s(%d vnodes)\n", vn->videsc, vn->index, n->ipaddr, n->replicas);
}

/** Virtual node travel handler which used to travek all virtual node
	and execute $fn. */
static __oryx_always_inline__
void _vn_travel (struct chash_root *ch, void (*fn)(struct vnode_t *, int), int flags)
{
	
	struct rb_node* rbn;
	struct vnode_t* vn;
	
	rbn = rb_first(&ch->vn_root);

	if (flags & NODE_FLG_CLASSFY_TRAVEL) {
		struct node_t *n1 = NULL, *p;
		
		do_mutex_lock (&ch->nhlock);
		list_for_each_entry_safe (n1, p, &ch->node_head, node) {
			
			rbn = rb_first (&ch->vn_root);
			while (rbn) {
				vn = rb_entry (rbn, struct vnode_t, node);
				if (likely (vn)) {
					if ((intptr_t)n1 == (intptr_t)vn->physical_node) {
						if (likely (fn))
							fn (vn, flags);
					}
				}
				
				rbn  = rb_next(rbn);
			}
		}
		do_mutex_unlock (&ch->nhlock);
		
	}

	else {
		while(rbn) {
			vn = rb_entry(rbn, struct vnode_t, node);
			if (likely (vn)) {
				if (likely (fn))
					fn (vn, flags);
			}
			rbn  = rb_next(rbn);
		}
	}
}

/** Virtual node allocation handler which used to allocating a new virtual node
	and returns its address. */
static __oryx_always_inline__
void *_vn_alloc(void *n, uint32_t key, int i, char *videsc)
{

	struct vnode_t *vn;

	vn = (struct vnode_t *)malloc (sizeof (struct vnode_t));
	if (likely (vn)) {
		
		rb_init_node(&vn->node);
		vn->physical_node = n;
		vn->index = i;
		vn->videsc = malloc (strlen (videsc));
		VN_KEY_I(vn) = key;
		
		if (likely (vn->videsc))
			memcpy (vn->videsc, videsc, strlen (videsc));
	}

	return vn;
}

/** Find a node which key is minimum (or, maximum) among all node. */
static __oryx_always_inline__
struct vnode_t *_vn_default (struct chash_root *ch)
{

	struct vnode_t *dvn = NULL;
	
	VN_DEFAULT(ch, dvn);

	fprintf (stdout, "***    default VN(%s, %u) used !!!\n", dvn->videsc, VN_KEY_I(dvn));
	return dvn;
}

/** Find a node which has a same $KEY. */
static __oryx_always_inline__
void* _vn_find (struct chash_root *ch, const void* key)
{

	struct vnode_t *vn = NULL;
	struct rb_node* pos;

	if(likely(ch) && likely(key) &&
		(NULL != (pos = rb_find (&ch->vn_root, key))))
		vn = rb_entry (pos, struct vnode_t, node);

	return vn;
}

/** Find a node who's key is nearest by $KEY at clockwise. */
void *_vn_find_ring (struct chash_root *ch, const void *key)
{

	int ret;
	struct rb_root *root = &ch->vn_root;
	struct rb_node *node = root->rb_node;
	struct rb_node *rbn = NULL;
	struct vnode_t *vn = NULL;
	
	while (node) {
		ret = root->cmp(node, key);
/**
		if (ret == 0) { return node; }
*/
		if (ret >= 0) { rbn = node; node = node->rb_left; }
		else if (ret < 0) { node = node->rb_right; }
    }

	if (rbn)
		vn = rb_entry (rbn, struct vnode_t, node);
	else
		VN_DEFAULT (ch, vn);
	
        return vn;
}

/** Add a virtual node to a phsical node. */
static __oryx_always_inline__
int _vn_add (struct chash_root *ch, struct node_t *n, struct vnode_t *vn)
{
	if(unlikely(!vn))
	    return -1;

	if (!rb_insert (&vn->node, &ch->vn_root, (void *)(uint32_t *)&VN_KEY_I(vn)))
		n->valid_vns ++;

	 return 0;
}

/** Default naming function for a virtual node. Actually, you can change it if needed.*/
static __oryx_always_inline__
void _vn_fmt (char *idesc_in, int i, char *v_idesc_out, size_t *lo)
{
	*lo = sprintf (v_idesc_out, "%s_%d", idesc_in, i);
}

/** Clone a physical node. */
void * _n_clone (struct node_t *n)
{
	struct node_t *shadow;

	shadow = (struct node_t *)malloc (sizeof (struct node_t));
	if (likely (shadow)) {

		memset (shadow, 0, sizeof (struct node_t));
		
		shadow->flags = 0;
		shadow->replicas = n->replicas;
		strcpy (shadow->ipaddr, n->ipaddr);
		strcpy (shadow->idesc, n->idesc);
		N_HITS(shadow) = 0;
		N_VALID_VNS(shadow) = 0;
		INIT_LIST_HEAD(&shadow->node);
	}

	return shadow;
}

/** New a physical node. */
void * _n_new ()
{
	struct node_t *shadow;
	char key[32];
	
	shadow = (struct node_t *)malloc (sizeof (struct node_t));
	if (likely (shadow)) {

		memset (shadow, 0, sizeof (struct node_t));
		
		shadow->flags = 0;
		shadow->replicas = NODE_DEFAULT_VNS;
		
		oryx_ipaddr_generate(key);
		strcpy (shadow->ipaddr, key);

		oryx_pattern_generate(key, 32);
		strcpy (shadow->idesc, key);

		N_HITS(shadow) = 0;
		N_VALID_VNS(shadow) = 0;
		INIT_LIST_HEAD(&shadow->node);
	}

	return shadow;
}

/** Add a physical node to list after DBCC. */
static __oryx_always_inline__
int _n_add (struct chash_root *ch, struct node_t *n)
{

	struct node_t *n1 = NULL, *p;

	do_mutex_lock (&ch->nhlock);
	
	list_for_each_entry_safe(n1, p, &ch->node_head, node){
		if (!strcmp(n->ipaddr, n1->ipaddr)){
			fprintf (stdout, "A same machine %15s(\"%s\":%p)\n\n", 
						n1->idesc, n1->ipaddr, n1);
			do_mutex_unlock (&ch->nhlock);
			return -1;
		}
	}

	list_add_tail (&n->node, &ch->node_head);
	ch->total_ns ++;
	N_HITS(n) = 0;
	N_VALID_VNS(n) = 0;
	
	do_mutex_unlock (&ch->nhlock);
	
	return 0;
}

/** Delete a specified physical node from list. */
static __oryx_always_inline__
int _n_del (struct chash_root *ch, struct node_t *n)
{

	list_del (&n->node);
	ch->total_ns --;

	return 0;
}

struct chash_root *chash_init ()
{
	struct chash_root *ch;

	ch = (struct chash_root *) malloc (sizeof (struct chash_root));
	if (unlikely(!ch)) {
		fprintf (stdout, "Can not alloc memory. \n");
		return NULL;
	}

	ch->hash_func = hash_algo;
	do_mutex_init(&ch->nhlock);
	rb_init (&ch->vn_root, _vn_cmpi);
	INIT_LIST_HEAD (&ch->node_head);

	return ch;
}

/** Caculate total virtual node counter. */
int total_vns (struct chash_root *ch)
{
	int vns=0;
	struct node_t *n1 = NULL, *p;
	
	do_mutex_lock (&ch->nhlock);
	
	list_for_each_entry_safe(n1, p, &ch->node_head, node)
		vns += n1->valid_vns;
	
	do_mutex_unlock (&ch->nhlock);

	return vns;
}

/** Remove a specified physical node from list.
	Erase all its virtual node and update gloable default virtual node. */
struct node_t *node_remove (struct chash_root *ch, char *nodekey)
{
	int i;
	uint32_t hv;
	size_t lo = 0;
	char key[32] = {0};
	struct node_t *n = NULL, *n1 = NULL, *p;
	struct vnode_t *vn = NULL;

	/* Find the physical node by $nodekey  */
	do_mutex_lock (&ch->nhlock);
	
	list_for_each_entry_safe(n1, p, &ch->node_head, node) {
		if (!strcmp(n1->ipaddr, nodekey)){
			do_mutex_unlock (&ch->nhlock);
			n = n1;
			goto find;
		}
	}
	
	do_mutex_unlock (&ch->nhlock);
	return NULL;
	
find:

	for (i = 0; i < n->replicas; i ++) {
		
		_vn_fmt (n->ipaddr, i, key, &lo);
	
		hv = ch->hash_func (key, strlen (key));
		vn = _vn_find (ch, (void *)&hv);
		if (!vn) continue;
		{
			n1 = vn->physical_node;
			rb_erase (&vn->node, &ch->vn_root);
			n1->valid_vns --;
		}
	}

	_n_del (ch, n);

	ch->vn_max = NULL;
	ch->vn_min = NULL;
	
	do_mutex_lock (&ch->nhlock);
	
	list_for_each_entry_safe(n1, p, &ch->node_head, node) {
		
		for (i = 0; i < n1->replicas; i ++) {
			_vn_fmt (n1->ipaddr, i, key, &lo);
			hv = ch->hash_func (key, strlen (key));
			vn = _vn_find (ch, (void *)&hv);
			if (!vn) continue;

			/** Init the maximun and minmum (of key) node */
			if (ch->vn_max == NULL)
				ch->vn_max = &vn->node;
			if (ch->vn_min == NULL)
				ch->vn_min = &vn->node;
			
			/** Update the maximun (of key) node */
			if (ch->vn_max != NULL) {
				struct vnode_t *vn_max = rb_entry(ch->vn_max, struct vnode_t, node);
				if (VN_KEY_I(vn_max) < VN_KEY_I(vn))
					ch->vn_max = &vn->node;
			}

			/** Update the minimun (of key) node */
			if (ch->vn_min != NULL) {
				struct vnode_t *vn_min = rb_entry(ch->vn_min, struct vnode_t, node);
				if (VN_KEY_I(vn_min) > VN_KEY_I(vn))
					ch->vn_min = &vn->node;
			}
		
		}
	}
	
	do_mutex_unlock (&ch->nhlock);

	return n;
}

/** Install a specified physical node to list. */
void node_install (struct chash_root *ch, struct node_t *n)
{

	int i;
	size_t lo = 0;
	uint32_t hv = 0;
	struct vnode_t *vn;
	char v_idesc [32] = {0};

	if (_n_add (ch, n)) {
		fprintf (stdout, "%15s(%15s) has installed \n", n->idesc, n->ipaddr);
		return;
	}
	
	for (i = 0; i < n->replicas; i++) {

		memset (v_idesc, 0, 32);
		lo = 0;
		
		_vn_fmt (n->ipaddr, i, v_idesc, &lo);
		hv = ch->hash_func (v_idesc, lo);
		
		vn = _vn_find (ch, (void *)(uint32_t *)&hv);
		if (likely (vn)) {
			fprintf (stdout, "%s_%u(%s) has added \n", v_idesc, VN_KEY_I(vn), n->ipaddr);
			return;
		}
		
		/* Allocate a VN and inited VN with $hv and node */
		vn = _vn_alloc (n, hv, i, v_idesc);
		if (unlikely (!vn)) {
			fprintf (stdout, "Can not alloc memory for %s\n", v_idesc);
			return;
		}

		if (!_vn_add (ch, n, vn)) {

			/** Init the maximun and minmum (of key) node */
			if (ch->vn_max == NULL)
				ch->vn_max = &vn->node;
			if (ch->vn_min == NULL)
				ch->vn_min = &vn->node;
			
			/** Update the maximun (of key) node */
			if (ch->vn_max != NULL) {
				struct vnode_t *vn_max = rb_entry(ch->vn_max, struct vnode_t, node);
				if (VN_KEY_I(vn_max) < VN_KEY_I(vn))
					ch->vn_max = &vn->node;
			}

			/** Update the minimun (of key) node */
			if (ch->vn_min != NULL) {
				struct vnode_t *vn_min = rb_entry(ch->vn_min, struct vnode_t, node);
				if (VN_KEY_I(vn_min) > VN_KEY_I(vn))
					ch->vn_min = &vn->node;
			}
		}
	}

}

/** Lookup a physical node from list with a specified key. */
struct node_t *node_lookup (struct chash_root *ch, char *key)
{
	uint32_t hv;
	struct vnode_t *vn = NULL;
	struct node_t *n = _vn_default(ch)->physical_node;

	hv = ch->hash_func (key, strlen (key));
	
	vn = _vn_find_ring(ch, (void *)(uint32_t *)&hv);
	if (likely(vn))
		n = vn->physical_node;
	else
		fprintf (stdout, "Can not find vn with a (%s, %u)\n", key, hv);
	
	ch->total_hit_times ++;
	N_HITS_INC(n);
	
	return n;
}

/** Dump all physical node and statistics.*/
void node_summary (struct chash_root *ch)
{
	struct node_t *n1 = NULL, *p;

	fprintf (stdout, "\n\n\nTotal %15d(%-5d vns) machines\n", ch->total_ns, total_vns(ch));

	fprintf (stdout, "%15s%16s%4s%15s%15s\n", "MACHINE", "IPADDR", "VNS", "HIT", "RATIO");
	do_mutex_lock (&ch->nhlock);
	
	list_for_each_entry_safe(n1, p, &ch->node_head, node){
		fprintf (stdout, "%15s%16s%4d%15d%15.2f%s\n", 
					n1->idesc, n1->ipaddr, n1->valid_vns, N_HITS(n1), (float)N_HITS(n1)/ch->total_hit_times * 100, "%");
	}

	do_mutex_unlock (&ch->nhlock);
}

/** Dump all physical node.*/
void node_dump (struct chash_root *ch)
{
	fprintf (stdout, "\n\n===========Virtual Nodes (%d)============\n", total_vns(ch));
	_vn_travel (ch, _vn_dump, NODE_FLG_CLASSFY_TRAVEL);
}

/** Physical node configuration.*/
void node_set (struct node_t *n, char *desc, char *ipaddr, int replicas)
{
	memcpy ((void *)&n->idesc[0], desc, strlen(desc));
	memcpy ((void *)&n->ipaddr[0], ipaddr, strlen(ipaddr));
	n->flags = NODE_FLG_INITED;
	n->replicas = replicas;
	INIT_LIST_HEAD (&n->node);
}

void chcopy (struct chash_root **new, struct chash_root *old)
{

	struct chash_root *backup;
	struct node_t *n1 = NULL, *p;

	backup = chash_init ();
	
	list_for_each_entry_safe(n1, p, &old->node_head, node){
		struct node_t *shadow;
		shadow = _n_clone (n1);
		node_install (backup, shadow);
	}

	*new = backup;
}

