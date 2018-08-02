#include "oryx.h"


int main (
        int     __oryx_unused_param__   argc,
        char    __oryx_unused_param__   ** argv
)

{
	/** do a simple test */
	struct CounterCtx ctx, ctx1; 

	oryx_initialize();
	
	memset(&ctx, 0, sizeof(struct CounterCtx));
	memset(&ctx1, 0, sizeof(struct CounterCtx));

	counter_id id1 = oryx_register_counter("t1", "c1", &ctx);
	BUG_ON(id1 != 1);
	counter_id id2 = oryx_register_counter("t2", "c2", &ctx);
	BUG_ON(id2 != 2);
	
	oryx_counter_get_array_range(1, 
		atomic_read(&ctx.curr_id), &ctx);

	oryx_counter_inc(&ctx, id1);
	oryx_counter_inc(&ctx, id2);

	u64 id1_val = 0;
	u64 id2_val = 0;
	
	id1_val = oryx_counter_get(&ctx, id1);
	id2_val = oryx_counter_get(&ctx, id2);

	BUG_ON(id1_val != 1);
	BUG_ON(id2_val != 1);

	oryx_counter_inc(&ctx, id1);
	oryx_counter_inc(&ctx, id2);

	id1_val = oryx_counter_get(&ctx, id1);
	id2_val = oryx_counter_get(&ctx, id2);

	BUG_ON(id1_val != 2);
	BUG_ON(id2_val != 2);

	oryx_counter_add(&ctx, id1, 100);
	oryx_counter_add(&ctx, id2, 100);

	id1_val = oryx_counter_get(&ctx, id1);
	id2_val = oryx_counter_get(&ctx, id2);

	BUG_ON(id1_val != 102);
	BUG_ON(id2_val != 102);

	oryx_counter_set(&ctx, id1, 1000);
	oryx_counter_set(&ctx, id2, 1000);

	id1_val = oryx_counter_get(&ctx, id1);
	id2_val = oryx_counter_get(&ctx, id2);

	BUG_ON(id1_val != 1000);
	BUG_ON(id2_val != 1000);

	counter_id id1_1 = oryx_register_counter("t1", "c1", &ctx1);
	counter_id id2_2 = oryx_register_counter("t2", "c2", &ctx1);

	fprintf (stdout, "id1=%d, id2=%d, id1_1=%d, id2_2=%d\n", id1, id2, id1_1, id2_2);
	
	oryx_release_counter(&ctx);
	oryx_release_counter(&ctx1);

	BUG_ON(ctx.h_size != 0);
	BUG_ON(ctx1.h_size != 0);
	
	return 0;
}

