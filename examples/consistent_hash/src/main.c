#include "oryx.h"
#include "cv_hash.h"

#define THRESHOLD_L1(i) (i*0.08)
#define THRESHOLD_L2(i) (i*0.15)
#define THRESHOLD_L3(i) (i*0.20)


#define TIME_DECLARE()\
	clock_t __s, __e;

#define TIME_START()\
	__s = clock();

#define TIME_FINAL()\
	__e = clock();

#define TIME_CALC()\
	;//printf ("Time occured %f s, %lu, %lu\n", (double) ((__e  - __s)/(double)CLOCKS_PER_SEC), __s, __e);

/** Input times. */
#define MAX_INJECT_DATA	10000000

/** Physical node number. */
#define MAX_BACKEND_MACHINES	100

/** Physical node definition. */
struct node_t backend_node[MAX_BACKEND_MACHINES] = {
		{"Default0", "127.0.0.1", -1, -1, 0, {NULL, NULL}, 0}
};

/** Record map changes from virtual node to physical node 
	when a physical node added or removed. 
	This variable is always used to check consistency for hash.  */
int changes = 0;


struct chash_root * ch_template;
struct chash_root * ch_add;
struct chash_root * ch_del;

void check_miss_while_add ()
{

	int i;
	uint32_t hv;
	uint32_t intp = 0;
	char key[32] = {0};
	struct chash_root *chnew = NULL, *ch = NULL;
	char *colur = draw_color(COLOR_WHITE);
	struct node_t *n = NULL, *new = NULL;
	struct vnode_t *vn = NULL, *vn_backup = NULL;

	chnew = ch_add;
	chcopy (&ch, chnew);
	
	changes = 0;
	
	new = _n_new ();
	if (likely (new)) {
		node_install (ch, new);
		printf ("\n\n\n\nTrying to add a node ... \"%s\", done, total_vns=%d\n", 
			new->ipaddr, total_vns(ch));
	}

	for (i = 0; i < MAX_INJECT_DATA; i ++) {
		memset ((void *)&key[0], 0, 32);
		sprintf (key, "2.%d.%d.%d", 
			((i * next_rand_(&intp)) % 255),
			((i * next_rand_(&intp)) % 255),
			((i * next_rand_(&intp)) % 255));

		hv = ch->hash_func (key, strlen (key));
		
		vn = _vn_find_ring(ch, (void *)(uint32_t *)&hv);
		if (likely(vn))
			n = vn->physical_node;
		
		if (likely(n)) {
			ch->total_hit_times ++;
			N_HITS_INC(n);
			//printf ("[%16s] is in node: [%16s]\n", key, n->idesc);
		}
		
		vn_backup = _vn_find_ring (chnew, (void *)(uint32_t *)&hv);
		vn = _vn_find_ring(ch, (void *)(uint32_t *)&hv);

		if (strcmp (
				((struct node_t *)vn->physical_node)->idesc, 
				((struct node_t *)vn_backup->physical_node)->idesc)) 
			changes ++;
		
	};

	if (changes <= THRESHOLD_L1(MAX_INJECT_DATA))
		colur = draw_color(COLOR_WHITE);
	
	if ((changes > THRESHOLD_L1(MAX_INJECT_DATA)) && (changes <= THRESHOLD_L2(MAX_INJECT_DATA)))
		colur = draw_color(COLOR_YELLOW);

	if (changes >= THRESHOLD_L2(MAX_INJECT_DATA))
		colur = draw_color(COLOR_RED);
	
	printf ("\nAdding ...%18s (%s)\n Changes (%-8u%s%-4.2f%s%s)\n\n", 
		new->ipaddr, new->idesc, 
		changes, colur, (float)changes/MAX_INJECT_DATA * 100, "%", draw_color(COLOR_FIN));

}

void check_miss_while_rm ()
{

	int i;
	uint32_t hv;
	uint32_t intp = 0;
	char key[32] = {0};
	struct chash_root *chnew = NULL, *ch = NULL;
	char *colur = draw_color(COLOR_WHITE);
	struct node_t *n = NULL;
	struct vnode_t *vn = NULL, *vn_backup = NULL;
	struct node_t *removed_node = &backend_node[1];

	chnew = ch_del;
	chcopy (&ch, chnew);
	
	changes = 0;
	
	n = node_remove (ch, removed_node->ipaddr);

	printf ("\n\n\n\nTrying to remove a node ... \"%s\", done, total_vns=%d\n", 
		n->idesc, total_vns(ch));

	free (n);
	
	for (i = 0; i < MAX_INJECT_DATA; i ++) {
		memset ((void *)&key[0], 0, 32);
		sprintf (key, "2.%d.%d.%d", 
			((i * next_rand_(&intp)) % 255),
			((i * next_rand_(&intp)) % 255),
			((i * next_rand_(&intp)) % 255));

		hv = ch->hash_func (key, strlen (key));
		
		vn = _vn_find_ring(ch, (void *)(uint32_t *)&hv);
		if (likely(vn))
			n = vn->physical_node;
		
		if (likely(n)) {
			ch->total_hit_times ++;
			N_HITS_INC(n);
			//printf ("[%16s] is in node: [%16s]\n", key, n->idesc);
		}
		
		vn_backup = _vn_find_ring (chnew, (void *)(uint32_t *)&hv);
		vn = _vn_find_ring(ch, (void *)(uint32_t *)&hv);

		if (strcmp (
				((struct node_t *)vn->physical_node)->idesc, 
				((struct node_t *)vn_backup->physical_node)->idesc)) 
			changes ++;
		
	};

	if (changes <= THRESHOLD_L1(MAX_INJECT_DATA))
		colur = draw_color(COLOR_WHITE);

	if ((changes > THRESHOLD_L1(MAX_INJECT_DATA)) && (changes <= THRESHOLD_L2(MAX_INJECT_DATA)))
		colur = draw_color(COLOR_YELLOW);
	
	if (changes >= THRESHOLD_L2(MAX_INJECT_DATA))
		colur = draw_color(COLOR_RED);
	
	printf ("\nRemoving ...%18s (%s)\n Changes (%-8u%s%-4.2f%s%s)\n\n", 
		removed_node->ipaddr, removed_node->idesc, 
		changes, colur, (float)changes/MAX_INJECT_DATA * 100, "%", draw_color(COLOR_FIN));

}

/** A test handler.*/
void lookup_handler ()
{

	int i = 0;
	uint32_t hv;
	uint32_t intp = 0;
	char key[32] = {0};
	struct node_t *n = NULL;
	struct vnode_t *vn = NULL;
	struct chash_root *ch = ch_template;
	
	chcopy (&ch_add, ch);
	chcopy (&ch_del, ch);
	
	printf ("\n\n\nTrying to inject %d object to %d nodes ... (please wait for a while)\n", 
		MAX_INJECT_DATA, MAX_BACKEND_MACHINES);
	
	FOREVER {

		for (i = 0; i < MAX_INJECT_DATA; i ++) {
			memset ((void *)&key[0], 0, 32);
			sprintf (key, "2.%d.%d.%d", 
				((i * next_rand_(&intp)) % 255),
				((i * next_rand_(&intp)) % 255),
				((i * next_rand_(&intp)) % 255));

			hv = ch->hash_func (key, strlen (key));

			vn = _vn_find_ring(ch, (void *)(uint32_t *)&hv);
			if (likely(vn))
				n = vn->physical_node;
			
			if (likely(n)) {
				ch->total_hit_times ++;
				N_HITS_INC(n);
				//printf ("[%16s] is in node: [%16s]\n", key, n->idesc);
			}
		};

		node_summary (ch);

		check_miss_while_rm ();
		check_miss_while_add ();
		
		break;
	};
	
}

int main(
	int 	__oryx_unused_param__	argc,
	char	__oryx_unused_param__	** argv
)
{
	oryx_initialize();

	int i = 0;

	ch_template = chash_init();
	
	for (i = 0; i < MAX_BACKEND_MACHINES; i++) {
		char key [32];
		char machine[32];
		sprintf (machine, "Machine_%d", i);
		oryx_ipaddr_generate (key);
		node_set (&backend_node[i], machine, key, 160);
	}

	for (i = 0; i < MAX_BACKEND_MACHINES; i ++) {
		struct node_t *shadow;
		shadow = _n_clone (&backend_node[i]);
		node_install (ch_template, shadow);
	}
	
	lookup_handler ();

	return 0;
}

