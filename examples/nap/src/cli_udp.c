#include "oryx.h"
#include "vty.h"
#include "command.h"
#include "prefix.h"
#include "common_private.h"
#include "map_private.h"
#include "udp_private.h"
#include "cli_udp.h"

#if 0
atomic_t n_udp_elements = ATOMIC_INIT(0);
oryx_vector udp_vector_table;

#define UDP_LOCK
#ifdef UDP_LOCK
static INIT_MUTEX(udp_lock);
#else
#undef do_mutex_lock
#undef do_mutex_unlock
#define do_mutex_lock(lock)
#define do_mutex_unlock(lock)
#endif

#define VTY_ERROR_UDP(prefix, alias)\
	vty_out (vty, "%s(Error)%s %s udp \"%s\"%s", \
		draw_color(COLOR_RED), draw_color(COLOR_FIN), prefix, alias, VTY_NEWLINE)

#define VTY_SUCCESS_UDP(prefix, v)\
	vty_out (vty, "%s(Success)%s %s udp \"%s\"(%u)%s", \
		draw_color(COLOR_GREEN), draw_color(COLOR_FIN), prefix, v->sc_alias, v->ul_id, VTY_NEWLINE)

#define VTY_ERROR_PATTERN(prefix, alias)\
	vty_out (vty, "%s(Error)%s %s pattern \"%s\"%s", \
		draw_color(COLOR_RED), draw_color(COLOR_FIN), prefix, alias, VTY_NEWLINE)

#define VTY_SUCCESS_PATTERN(prefix, v)\
	vty_out (vty, "%s(Success)%s %s pattern \"%s\"(%u)%s", \
		draw_color(COLOR_GREEN), draw_color(COLOR_FIN), prefix, v->sc_val, v->ul_id, VTY_NEWLINE)

static __oryx_always_inline__
void udp_free (ht_value_t v)
{
	/** Never free here! */
	v = v;
}

static uint32_t
udp_hval (struct oryx_htable_t *ht,
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
udp_cmp (ht_value_t v1, 
		uint32_t s1,
		ht_value_t v2,
		uint32_t s2)
{
	int xret = 0;

	if (!v1 || !v2 ||s1 != s2 ||
		memcmp(v1, v2, s2))
		xret = 1;
	
	return xret;
}

int udp_pattern_new (struct pattern_t **pattern)
{

	(*pattern) = kmalloc (sizeof (struct pattern_t), MPF_CLR, __oryx_unused_val__);
	ASSERT((*pattern));
	
	(*pattern)->ul_id = PATTERN_INVALID_ID;
	(*pattern)->ul_flags = PATTERN_DEFAULT_FLAGS;
	(*pattern)->ul_size = 0;
	(*pattern)->us_offset = PATTERN_DEFAULT_OFFSET;
	(*pattern)->us_depth = PATTERN_DEFAULT_DEPTH;
	
	return 0;
}

static void udp_pattern_free (struct pattern_t **pattern)
{
	fprintf (stdout, "		freeing pattern [\"%16s\", id=%d] ...  ", (*pattern)->sc_val, (*pattern)->ul_id);
	kfree(*pattern);
	fprintf (stdout, "done\n");
}

struct pattern_t *udp_entry_pattern_lookup (struct udp_t *udp, char *pattern, size_t pl)
{

	int i;
	struct pattern_t *p;

	vec_foreach_element (udp->patterns, i, p) {
		if (!p) continue;
		if (p->ul_size == pl && 
			!memcmp (p->sc_val, pattern, pl)) {
			return p;
		}
	}

	return NULL;
}

struct pattern_t *udp_entry_pattern_lookup_id (struct udp_t *udp, uint32_t id)
{
	BUG_ON(udp == NULL);
	BUG_ON(udp->patterns == NULL);
	
	if (!vec_active(udp->patterns) ||
		id > vec_active(udp->patterns))
		return 0;

	return vec_lookup (udp->patterns, id);
}

int udp_entry_pattern_add (struct pattern_t *pat, struct udp_t *udp)
{
	/** Add udp to a oryx_vector table for dataplane fast lookup. */
	pat->ul_id = vec_set (udp->patterns, pat);
	oryx_logn ("[\"%16s\"] installing pattern \"%16s\" ...  done(%d)", 
			udp_alias(udp), pat->sc_val, pat->ul_id);
	return 0;
}

int udp_entry_pattern_remove (struct pattern_t *pat, struct udp_t *udp)
{
	/** unset pattern from an oryx_vector. */
	vec_unset (udp->patterns, pat->ul_id);
	oryx_logn ("[\"%16s\"] uninstalling pattern \"%16s\" ...  done(%d)", 
			udp_alias(udp), pat->sc_val, pat->ul_id);

	/** free this pattern after removed from udp. */
	udp_pattern_free (&pat);
	
	return 0;
}

static int udp_pattern_output (struct pattern_t *pattern, struct vty *vty)
{

	if (pattern) {
		vty_out (vty, "	%16s%u%s", "Pattern: ", pattern->ul_id, VTY_NEWLINE);
		vty_out (vty, "			%16s%s, %s%s", "States: ", 
			(pattern->ul_flags & PATTERN_FLAGS_NOCASE) ? "CI" : "CS",
			(pattern->ul_flags & PATTERN_FLAGS_INSTALLED) ? "installed" : "uninstalled", VTY_NEWLINE);
		vty_out (vty, "			%16soffset=%u, depth=%u, length=%lu%s", "Summary: ",
			(pattern->us_offset), (pattern->us_depth), pattern->ul_size, VTY_NEWLINE);
		vty_out (vty, "			%16s%s%s", "Context: ", pattern->sc_val, VTY_NEWLINE);
		vty_out (vty, "			%16s%llu/%llu%s", 
			"Hits: ", PATTERN_STATS_P(pattern), PATTERN_STATS_B(pattern), VTY_NEWLINE);

	}

	return 0;
}


static void udp_entry_output (struct udp_t *udp, struct vty *vty)
{

	int i;
	char tmstr[100];
	struct pattern_t *p;
	
	if (!udp) {
		return;
	}
	
	fmt_time (udp->ull_create_time, "%Y-%m-%d,%H:%M:%S", (char *)&tmstr[0], 100);
	vty_out (vty, "%16s\"%s\"(%u)		%s%s", "Udp ", udp->sc_alias, udp->ul_id, tmstr, VTY_NEWLINE);

	u8 qua;
	uint32_t passed_maps;
	uint32_t dropped_maps;
	oryx_vector v;

	qua = QUA_PASS;
	passed_maps = vec_active (udp->qua[qua]);

	qua = QUA_DROP;
	dropped_maps = vec_active (udp->qua[qua]);
	
	vty_out (vty, "	%16s%s", "Quadrant:", VTY_NEWLINE);

	vty_out (vty, "			%16s(%u): ", "Drop", dropped_maps);
	if (dropped_maps == 0)
		vty_out (vty, "%s%s", "N/A", VTY_NEWLINE);
	else {
		int i;
		qua = QUA_DROP;
		struct map_t *m;
		v = udp->qua[qua];
	
		vec_foreach_element (v, i, m) {
			if (m) {
				vty_out (vty, "%s", map_alias(m));
				dropped_maps --;
				if (dropped_maps) vty_out (vty, ",");
				else dropped_maps = dropped_maps;
			} else {
				m = m;
			}
		}
		vty_out (vty, "%s", VTY_NEWLINE);
	}

	vty_out (vty, "			%16s(%u): ", "Pass", passed_maps);
	if (passed_maps == 0)
		vty_out (vty, "%s%s", "N/A", VTY_NEWLINE);
	else {
		int i;
		qua = QUA_PASS;
		struct map_t *m;
		v = udp->qua[qua];
	
		vec_foreach_element (v, i, m) {
			if (m) {
				vty_out (vty, "%s", map_alias(m));
				passed_maps --;
				if (passed_maps) vty_out (vty, ",");
				else passed_maps = passed_maps;
			} else {
				m = m;
			}
		}
		vty_out (vty, "%s", VTY_NEWLINE);
	}
	
	vec_foreach_element (udp->patterns, i, p) {
		if (p) {
			udp_pattern_output (p, vty);
		}
	}
	vty_out (vty, "---------------------------------%s", VTY_NEWLINE);
		
}

int udp_entry_add (struct udp_t *udp)
{
	vlib_udp_main_t *vp = &vlib_udp_main;

	if (udp->ul_id != UDP_INVALID_ID 	/** THIS APPL ??? */) {
		udp = udp;
		/** What should do here ??? */
	} 
	
	int r = 0;

	fprintf (stdout, "[\"%16s\"] installing  ...  ", udp_alias(udp));
	/** Add udp->sc_alias to hash table for controlplane fast lookup */
	r = oryx_htable_add(vp->htable, udp_alias(udp), strlen((const char *)udp_alias(udp)));

	/** Add alias to hash table for fast lookup by udp alise. */
	if (r == 0 /** success */) {
		/** Add udp to a oryx_vector table for dataplane fast lookup. */
		udp->ul_id = vec_set (vp->entry_vec, udp);
		fprintf (stdout, "done(%d)\n", udp_slot(udp));
	} else {
		fprintf (stdout, "not done(%d)\n", udp_slot(udp));
	}

	return r;
}

int udp_entry_remove (struct udp_t *udp)
{

	int r = 0;
	vlib_udp_main_t *vp = &vlib_udp_main;
	char log[ORYX_LOG_MAX_LOG_MSG_LEN] = {0};

	do_mutex_lock (&udp_lock);
	
	fprintf (stdout, "[\"%16s\"] uninstalling  ...  ", udp_alias(udp));
	r = oryx_htable_del(vp->htable, udp->sc_alias, strlen((const char *)udp->sc_alias));
	if (r == 0 ) {
		/** Delete udp from oryx_vector */
		vec_unset (vp->entry_vec, udp->ul_id);
		fprintf (stdout, "done(%d)\n", udp_slot(udp));
	} else {
		fprintf (stdout, "not done(%d)\n", udp_slot(udp));
	}

	do_mutex_unlock (&udp_lock);

	/** Call MAP reloading it's UDP. */
	
	/** Should you free udp here ? */
	return r;  
}

int udp_entry_destroy (struct udp_t *udp)
{
	struct pattern_t *pt;
	int i;

	if (vec_active (udp->patterns)) {
		fprintf (stdout, "Trying to free patterns on and before destroy [\"%16s\"]   ...  \n", udp_alias(udp));
		vec_foreach_element (udp->patterns, i, pt) {
			if (pt) udp_pattern_free (&pt);
		}
	}
	

	vec_free (udp->patterns);
	vec_free (udp->qua[QUA_DROP]);
	vec_free (udp->qua[QUA_PASS]);

	fprintf (stdout, "[\"%16s\"] destroying  ...  ", udp_alias(udp));
#if 0
	kfree (udp);
#endif
	fprintf (stdout, "done\n");

	return 0;  
}

static void udp_entry_remove_and_destroy (struct udp_t *udp)
{
	if (!udp) return;

	/** Delete udp from oryx_vector */
	udp_entry_remove (udp);
	udp_entry_destroy(udp);
}

static int udp_entry_destroy_strict (struct udp_t *udp, struct vty __oryx_unused_param__ *vty)
{
	struct map_t *m;
	oryx_vector v0, v1;
	int i;
	uint32_t c0, c1;
	
	v0 = udp->qua[QUA_DROP];
	v1 = udp->qua[QUA_PASS];
	c0 = vec_active (v0);
	c1 = vec_active (v1);
	
	if (c0 || c1) {
		VTY_ERROR_UDP ("busy", udp_alias(udp)); 
		{
			if (c0) {
				vec_foreach_element (v0, i, m) {
					if (m) {
						vty_out (vty, "%s", map_alias(m));
						c0 --;
						if (c0) vty_out (vty, ",");
						else c0 = c0;
					} else {
						m = m;
					}
				}
				vty_newline(vty);
			}

			if (c1) {
				vec_foreach_element (v1, i, m) {
					if (m) {
						vty_out (vty, "%s", map_alias(m));
						c1 --;
						if (c1) vty_out (vty, ",");
						else c1 = c1;
					} else {
						m = m;
					}
				}
				vty_newline(vty);
			}
		}

		return 0;
	}

	udp_entry_remove_and_destroy (udp);
	
	/** Call MAP reloading it's UDP. */
	
	/** Should you free udp here ? */
	return 0;  
}

struct udp_t *udp_entry_lookup (char *alias)
{
	vlib_udp_main_t *vp = &vlib_udp_main;

	if (!alias) return NULL;

	return (struct udp_t *) container_of (
		oryx_htable_lookup(vp->htable, alias, strlen((const char *)alias)), 
		struct udp_t, 
		sc_alias);
}

struct udp_t *udp_entry_lookup_id (uint32_t id)
{
	vlib_udp_main_t *vp = &vlib_udp_main;

	BUG_ON(vp->entry_vec == NULL);
	
	if (!vec_active(vp->entry_vec) ||
		id > vec_active(vp->entry_vec))
		return 0;

	return (struct udp_t *) vec_lookup (vp->entry_vec, id);;
}

int udp_entry_new (struct udp_t **udp, char *alias)
{
	/** create an udp */
	(*udp) = kmalloc (sizeof (struct udp_t), MPF_CLR, __oryx_unused_val__);
	ASSERT((*udp));

	oryx_vector pattern;
	pattern = vec_init (UDP_N_PATTERNS);
	if (unlikely (!pattern)) {
		kfree ((*udp));
		(*udp) = NULL;
		return -1;
	}

	(*udp)->qua[QUA_PASS] = vec_init (1);
	(*udp)->qua[QUA_DROP] = vec_init (1);

	(*udp)->patterns = pattern;
	(*udp)->ul_id = UDP_INVALID_ID;
	(*udp)->ul_flags = 0;
	(*udp)->ull_create_time = time(NULL);
	/** make udp alias. */
	sprintf ((char *)&(*udp)->sc_alias[0], "%s", ((alias != NULL) ? alias: UDP_PREFIX));	

	
	return 0;
}


#define PRINT_SUMMARY	\
		vty_out (vty, "matched %d element(s), total %d element(s)%s", \
			atomic_read(&n_udp_elements), (int)vec_count(udp_vector_table), VTY_NEWLINE);

DEFUN(show_udp,
      show_udp_cmd,
      "show udp [WORD]",
      SHOW_STR
      SHOW_CSTR
      SHOW_STR
      SHOW_CSTR
      SHOW_STR
      SHOW_CSTR)
{

	vty_out (vty, "Trying to display %d elements ... (User Defined Patterns)%s", vec_active(udp_vector_table), VTY_NEWLINE);
	vty_out (vty, "---------------------------------%s", VTY_NEWLINE);
	
	if (argc == 0){
		foreach_udp_func1_param1 (argv[0], udp_entry_output, vty)
	}
	else {
		split_foreach_udp_func1_param1 (argv[0], udp_entry_output, vty);
	}
	
	PRINT_SUMMARY;
	
    return CMD_SUCCESS;
}

DEFUN(no_udp,
      no_udp_cmd,
      "no udp [WORD]",
      NO_STR
      NO_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR)
{

	if (argc == 0) {
		foreach_udp_func1_param1 (argv[0], 
			udp_entry_destroy_strict, vty);
	}
	else {
		split_foreach_udp_func1_param1 (argv[0], 
			udp_entry_destroy_strict, vty);
	}
	PRINT_SUMMARY;

    return CMD_SUCCESS;
}

DEFUN(no_udp_pattern,
      no_udp_pattern_cmd,
      "no udp WORD pattern WORD",
      NO_STR
      NO_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR)
{

	struct udp_t *udp;
	
	udp = udp_entry_lookup ((char *)argv[0]);
	if (unlikely (!udp)) {
		VTY_ERROR_UDP ("no such", (char *)argv[0]);
		return CMD_SUCCESS;
	} else {

		/** only support lookup by Pattern ID. */
		split_foreach_udp_pattern_func1_param1 (
				argv[1], udp_entry_pattern_remove, udp);
		PRINT_SUMMARY;
	}

    return CMD_SUCCESS;
}

DEFUN(new_udp,
      new_udp_cmd,
      "udp WORD add PATTERN offset <1-65535> depth <1-65535> case (insensitive|sensitive)",
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR)
{

	struct udp_t *udp;
	
	udp = udp_entry_lookup ((char *)argv[0]);
	if (!udp) {
		udp_entry_new (&udp, (char *)argv[0]);
		if (unlikely (!udp)) {
			VTY_ERROR_UDP ("allocate", (char *)argv[0]);
			return CMD_SUCCESS;
		}

		/** Add udp to hash table. */
		if (udp_entry_add (udp)) {
			VTY_ERROR_UDP ("add", (char *)argv[0]);
			udp_entry_destroy (udp);
			return CMD_SUCCESS;
		}
		goto add_pattern;
	} else {

		struct pattern_t *pattern;
add_pattern:

		pattern = udp_entry_pattern_lookup (udp, (char *)argv[1], strlen ((char *)argv[1]));
		if (pattern) {
			VTY_SUCCESS_PATTERN ("same", pattern);
			return CMD_SUCCESS;
		}
		
		udp_pattern_new (&pattern);
		if (pattern) {

			if (!strncmp (argv[4], "s", 1))
				pattern->ul_flags &= (~PATTERN_FLAGS_NOCASE);
			pattern->us_offset = atoi ((char *)argv[2]);
			pattern->us_depth = atoi ((char *)argv[3]);
			pattern->ul_size = strlen ((char *)argv[1]);
			memcpy (pattern->sc_val, (char *)argv[1], pattern->ul_size);
			
			udp_entry_pattern_add(pattern, udp);
			VTY_SUCCESS_PATTERN ("add", pattern);
		}
	}

    return CMD_SUCCESS;
}


char local_path_pattern_file_txt[1024] = "./patterns.txt";
DEFUN(test_udp_generate_pattern,
      test_udp_generate_pattern_cmd,
      "test udp generate patterns WORD",
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR)
{

	int i, wl = 0;
	char pattern[128] = {0};
	oryx_file_t *fp;
	oryx_status_t s;
	const char *patterns_file = local_path_pattern_file_txt;
	i32 patterns = atoi (argv[0]);
	
	s = oryx_path_exsit (patterns_file);
	if (s) {
		s = oryx_path_remove (patterns_file);
		if (!s) {
			s = oryx_mkfile (patterns_file, &fp, "w");
			if (!fp) return CMD_SUCCESS;
		}
		
	} else {
		s = oryx_mkfile (patterns_file, &fp, "w");
		if (!fp) return CMD_SUCCESS;
	}
	
	/** FILE is ready or not. */
	ASSERT (fp);

	for (i = 0; i < patterns; i ++) {

		oryx_size_t pl = oryx_pattern_generate (pattern, 128);
		struct oryx_file_rw_context_t frw_ctx = {
			.cmd = RW_MODE_WRITE,
			.sc_buffer = (char *)&pattern[0],
			.ul_size = 128,
			.ul_rw_size = pl,
		};
		wl += oryx_file_read_write (fp, &frw_ctx);
	}
	
	vty_out(vty, "Write %d patterns to \"%s\" with %u bytes  avg %u.\r\n", 
		patterns, patterns_file, wl, wl/i);

	oryx_file_close (fp);
	
	return CMD_SUCCESS;
}

struct args_t {
	const char **argv;
	const int argc;
	struct vty *vty;
	int core;
};

static void * thread_fn (void *a)
{

	struct args_t *arg  = (struct args_t *)a;
	const char **argv = arg->argv;
	const int argc = arg->argc;
	struct vty *vty = arg->vty;
	oryx_file_t *fp;
	oryx_status_t s;
	const char *patterns_file = local_path_pattern_file_txt;
	oryx_size_t rl = 0;
	uint32_t random_string_input_size = 0;
	MpmCtx mpm_ctx;
	MpmThreadCtx mpm_thread_ctx;
	PrefilterRuleStore pmq;
	const uint32_t fixed_heap_size = 1024000;
	struct  timeval  start;
	struct  timeval  end;
	u64 t;
	u8 mpm_type = MPM_AC;
	int pattern_id = 0;
	char pattern[128] = {0};
	oryx_size_t pl;
	int i;
	
	for (i = 0; i < argc; i ++) {
		vty_out (vty, "argv[%d] =%s\r\n", i, argv[i]);
	}
	
	if (argc == 2) {
		random_string_input_size = atoi(argv[1]);
	}

	if (random_string_input_size > fixed_heap_size) {
		return CMD_SUCCESS;
	}
	
	s = oryx_path_exsit (patterns_file);
	
	if (!s) {
		vty_out(vty, "pattern file \"%s\" doesn't exsit.\r\n", patterns_file);
		return CMD_SUCCESS;
	}else {
		/** Open pattern file. */
		s = oryx_mkfile (patterns_file, &fp, "r");
		if (!fp) {
			vty_out(vty, "can not open pattern file \"%s\".\r\n", patterns_file);
			return CMD_SUCCESS;
		}
	}
	
	/** FILE is ready or not. */
	ASSERT (fp);

    	memset(&mpm_ctx, 0x00, sizeof(MpmCtx));
    	memset(&mpm_thread_ctx, 0, sizeof(MpmThreadCtx));
	
	if (!strncmp (argv[0], "h", 1)) {
		mpm_type = MPM_HS;
	}

	MpmInitCtx(&mpm_ctx, mpm_type);
    	PmqSetup(&pmq);
		
	MpmAddPatternCI(&mpm_ctx, (uint8_t *)"ABCD", 4, 0, 0, pattern_id ++, 0, 0);
	MpmAddPatternCI(&mpm_ctx, (uint8_t *)"bCdEfG", 6, 0, 0, pattern_id ++, 0, 0);
	MpmAddPatternCI(&mpm_ctx, (uint8_t *)"fghJikl", 7, 0, 0, pattern_id ++, 0, 0);

	/** */
	do {
		memset (pattern, 0, 128);

		struct oryx_file_rw_context_t frw_ctx = {
			.cmd = RW_MODE_READ | RW_MODE_READ_LINE,
			.sc_buffer = (char *)&pattern[0],
			.ul_size = 128,
			.ul_rw_size = 0,
		};
		
		s = oryx_file_read_write (fp, &frw_ctx);
		rl += frw_ctx.ul_rw_size;
		if (s > 0) {
			MpmAddPatternCI (&mpm_ctx, (uint8_t *)&pattern[0], frw_ctx.ul_rw_size, 0, 0, pattern_id ++, 0, 0);
		}
	}while (s > 0);
	
    	MpmPreparePatterns(&mpm_ctx);
	MpmInitThreadCtx (&mpm_thread_ctx, mpm_type);

	u64 total_cost = 0;
	u64 longest_cost = 1;
	u64 shortest_cost = 10000000;
	int times = 10000;
	uint32_t matched_size = 0;

    	char *buf = kmalloc (fixed_heap_size, MPF_CLR, __oryx_unused_val__);
	ASSERT(buf);
	vty_out (vty, "heap is ready %p ...\r\n", buf);
	
	pl = strlen ("abcdefghjiklmnopqrstuvwxyz");
	memcpy (buf, "abcdefghjiklmnopqrstuvwxyz", pl);
	
	for (i = 0; i < times; i ++) {

		if (random_string_input_size) {
			
			pl = oryx_pattern_generate (buf, random_string_input_size);
		}
		
		gettimeofday(&start,NULL);
		MpmSearch(&mpm_ctx, &mpm_thread_ctx, &pmq,
	                               (uint8_t *)buf, pl);
		gettimeofday(&end,NULL);

		t = tm_elapsed_us(&start, &end);

		total_cost += t;
		matched_size += pl;
		
		longest_cost = MAX(longest_cost, t);
		shortest_cost = MIN(shortest_cost, t);
	}

#define MB(bytes) ((float)(bytes / (1024 * 1024)))
#define Mb(bytes) ((float)((bytes * 8) / (1024 * 1024)))
#define Mbps(bytes,us) ((float)((Mb(bytes) * 1000000)/us))

	vty_out(vty, "%32s%s (\"%s\",  %u patterns, %lu bytes, %3f bytes/pattern)\r\n", 
		"MPM: ", mpm_table[mpm_type].name, patterns_file, pattern_id, rl, (float)(rl/pattern_id));
	vty_out(vty, "%32s%d times (%u bytes per loop)\r\n", 
		"loops: ", times, random_string_input_size);
	vty_out(vty, "%32s%.2f us (shortest %llu us, longest %llu us, total %llu us)\r\n", 
		"cost: ", (float)(total_cost / times), shortest_cost, longest_cost, total_cost);
	vty_out(vty, "%32s%u bytes (%.2f MB)\r\n", 
		"matched: ", matched_size, MB(matched_size));
	vty_out(vty, "%32s%.2f Mbps\r\n", 
		"throughput: ", Mbps(matched_size, total_cost));


    MpmDestroyCtx(&mpm_ctx);
    MpmDestroyThreadCtx(&mpm_ctx, &mpm_thread_ctx);
    PmqFree(&pmq);

    oryx_file_close (fp);
    kfree (buf);
	
    return CMD_SUCCESS;
}

DEFUN(test_udp,
      test_udp_cmd,
      "test udp (ac|hyperscan) [WORD]",
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR)
{
#if 0

	struct args_t arg = {
		.argv = argv,
		.argc = argc,
		.vty = vty,
	};

	pthread_t pid = 0;
	
	if (!pthread_create (&pid, NULL, thread_fn, &arg)) {
		if (!pthread_detach(pid)) {
			vty_out (vty, "Starting thread %lu ... please wait for a while.\r\n", pid);
			return CMD_SUCCESS;
		}
	}
#else
	oryx_file_t *fp;
	oryx_status_t s;
	const char *patterns_file = local_path_pattern_file_txt;
	oryx_size_t rl = 0;
	uint32_t random_string_input_size = 0;
	MpmCtx mpm_ctx;
	MpmThreadCtx mpm_thread_ctx;
	PrefilterRuleStore pmq;
	const uint32_t fixed_heap_size = 1024000;
	struct  timeval  start;
	struct  timeval  end;
	u64 t;
	u8 mpm_type = MPM_AC;
	int pattern_id = 0;
	char pattern[128] = {0};
	oryx_size_t pl;

	if (argc == 2) {
		random_string_input_size = atoi(argv[1]);
	}

	if (random_string_input_size > fixed_heap_size) {
		return CMD_SUCCESS;
	}
	
	s = oryx_path_exsit (patterns_file);
	
	if (!s) {
		vty_out(vty, "pattern file \"%s\" doesn't exsit.\r\n", patterns_file);
		return CMD_SUCCESS;
	}else {
		/** Open pattern file. */
		s = oryx_mkfile (patterns_file, &fp, "r");
		if (!fp) {
			vty_out(vty, "can not open pattern file \"%s\".\r\n", patterns_file);
			return CMD_SUCCESS;
		}
	}
	
	/** FILE is ready or not. */
	ASSERT (fp);

    	memset(&mpm_ctx, 0x00, sizeof(MpmCtx));
    	memset(&mpm_thread_ctx, 0, sizeof(MpmThreadCtx));
	
	if (!strncmp (argv[0], "h", 1)) {
		mpm_type = MPM_HS;
	}

	MpmInitCtx(&mpm_ctx, mpm_type);
    	PmqSetup(&pmq);
		
	MpmAddPatternCI(&mpm_ctx, (uint8_t *)"ABCD", 4, 0, 0, pattern_id ++, 0, 0);
	MpmAddPatternCI(&mpm_ctx, (uint8_t *)"bCdEfG", 6, 0, 0, pattern_id ++, 0, 0);
	MpmAddPatternCI(&mpm_ctx, (uint8_t *)"fghJikl", 7, 0, 0, pattern_id ++, 0, 0);

	/** */
	do {
		memset (pattern, 0, 128);

		struct oryx_file_rw_context_t frw_ctx = {
			.cmd = RW_MODE_READ | RW_MODE_READ_LINE,
			.sc_buffer = (char *)&pattern[0],
			.ul_size = 128,
			.ul_rw_size = 0,
		};
		
		s = oryx_file_read_write (fp, &frw_ctx);
		rl += frw_ctx.ul_rw_size;
		if (s > 0) {
			MpmAddPatternCI (&mpm_ctx, (uint8_t *)&pattern[0], frw_ctx.ul_rw_size, 0, 0, pattern_id ++, 0, 0);
		}
	} while (s > 0);
	
    MpmPreparePatterns(&mpm_ctx);
	MpmInitThreadCtx (&mpm_thread_ctx, mpm_type);

	int i;
	u64 total_cost = 0;
	u64 longest_cost = 1;
	u64 shortest_cost = 10000000;
	int times = 10000;
	uint32_t matched_size = 0;

    char *buf = kmalloc (fixed_heap_size, MPF_CLR, __oryx_unused_val__);
	ASSERT(buf);
	vty_out (vty, "heap is ready %p ...\r\n", buf);
	
	pl = strlen ("abcdefghjiklmnopqrstuvwxyz");
	memcpy (buf, "abcdefghjiklmnopqrstuvwxyz", pl);
	
	for (i = 0; i < times; i ++) {

		if (random_string_input_size) {
			
			pl = oryx_pattern_generate (buf, random_string_input_size);
		}
		
		gettimeofday(&start,NULL);
		MpmSearch(&mpm_ctx, &mpm_thread_ctx, &pmq,
	                               (uint8_t *)buf, pl);
		gettimeofday(&end,NULL);

		t = tm_elapsed_us (&start, &end);

		total_cost += t;
		matched_size += pl;
			
		longest_cost = MAX(longest_cost, t);
		shortest_cost = MIN(shortest_cost, t);
	}

#define MB(bytes) ((float)(bytes / (1024 * 1024)))
#define Mb(bytes) ((float)((bytes * 8) / (1024 * 1024)))
#define Mbps(bytes,us) ((float)((Mb(bytes) * 1000000)/us))

	vty_out(vty, "%32s%s (\"%s\",  %u patterns, %lu bytes, %3f bytes/pattern)\r\n", 
		"MPM: ", mpm_table[mpm_type].name, patterns_file, pattern_id, rl, (float)(rl/pattern_id));
	vty_out(vty, "%32s%d times (%u bytes per loop)\r\n", 
		"loops: ", times, random_string_input_size);
	vty_out(vty, "%32s%.2f us (shortest %llu us, longest %llu us, total %llu us)\r\n", 
		"cost: ", (float)(total_cost / times), shortest_cost, longest_cost, total_cost);
	vty_out(vty, "%32s%u bytes (%.2f MB)\r\n", 
		"matched: ", matched_size, MB(matched_size));
	vty_out(vty, "%32s%.2f Mbps\r\n", 
		"throughput: ", Mbps(matched_size, total_cost));


    MpmDestroyCtx(&mpm_ctx);
    MpmDestroyThreadCtx(&mpm_ctx, &mpm_thread_ctx);
    PmqFree(&pmq);

    oryx_file_close (fp);
    kfree (buf);
#endif
    return CMD_SUCCESS;
}

void udp_init(vlib_main_t *vm)
{
	  vlib_udp_main_t *vp = &vlib_udp_main;
	  
	  vp->htable = oryx_htable_init(DEFAULT_HASH_CHAIN_SIZE, 
			  udp_hval, udp_cmp, udp_free, 0);
	  vp->entry_vec = vec_init (MAX_UDPS);
	  
	  if (vp->htable == NULL || vp->entry_vec == NULL) {
		  fprintf (stdout, "vlib udp main init error!\n");
		  exit(0);
	  }

	udp_vector_table = vlib_udp_main.entry_vec;
	  
	install_element (CONFIG_NODE, &show_udp_cmd);
	install_element (CONFIG_NODE, &no_udp_cmd);
	install_element (CONFIG_NODE, &no_udp_pattern_cmd);
	install_element (CONFIG_NODE, &new_udp_cmd);

	install_element (CONFIG_NODE, &test_udp_generate_pattern_cmd);
	install_element (CONFIG_NODE, &test_udp_cmd);

	vm->ul_flags |= VLIB_UDP_INITIALIZED;
}

#endif
