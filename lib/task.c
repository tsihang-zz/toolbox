
#include "oryx.h"
#include "task.h"

/** For thread sync */
static sem_t oryx_task_sync_sem;

/** All system semphore is initialized in this api */
static void os_sync_init(void)
{
    sem_init(&oryx_task_sync_sem, 0, 1);
    /** Add other semphore here */
}

static void os_unsync(void)
{
    sem_post(&oryx_task_sync_sem);
}

static void os_sync(void)
{
    sem_wait(&oryx_task_sync_sem);
}

struct oryx_task_mgr_t{

	/** lock. */
	oryx_thread_mutex_t  lock;

	/** all tasks are hold by head. */
	struct list_head	head;

	/** tasks statistics. */
	u32 ul_count;

	/** task manager settings. */
	u32 ul_setting_flags;
};

static struct oryx_task_mgr_t taskmgr = {
	.ul_setting_flags = 0,
	.ul_count = 0,
	.lock = INIT_MUTEX_VAL,
};

static __oryx_always_inline__
void task_deregistry (struct oryx_task_t *task)
{
	struct oryx_task_mgr_t *tm = &taskmgr;

	oryx_thread_mutex_lock(&tm->lock);
	list_del(&task->list);	
	tm->ul_count --;
	oryx_thread_mutex_unlock(&tm->lock);

	if(task->ul_flags & TASK_CAN_BE_RECYCLABLE)
		kfree(task);
}

static __oryx_always_inline__
void task_registry (struct oryx_task_t *task)
{
	struct oryx_task_mgr_t *tm = &taskmgr;

	oryx_thread_mutex_lock(&tm->lock);
	list_add_tail(&task->list, &tm->head);	
	tm->ul_count ++;
	oryx_thread_mutex_unlock(&tm->lock);
}

static __oryx_always_inline__
struct oryx_task_t  *oryx_task_query_id (oryx_os_thread_t pid)
{
	struct oryx_task_t *task = NULL, *p;
	struct oryx_task_mgr_t *tm = &taskmgr;

	oryx_thread_mutex_lock(&tm->lock);
	list_for_each_entry_safe(task, p, &tm->head, list){
		if (pid == task->pid){
			oryx_thread_mutex_lock(&tm->lock);
			return task;
		}
	}
	oryx_thread_mutex_unlock(&tm->lock);
	return NULL;
}

static __oryx_always_inline__
struct oryx_task_t  *oryx_task_query_alias (char *sc_alias)
{
	struct oryx_task_t *task = NULL, *p;
	struct oryx_task_mgr_t *tm = &taskmgr;

	oryx_thread_mutex_lock(&tm->lock);
	list_for_each_entry_safe(task, p, &tm->head, list){
		if (!strcmp(sc_alias, task->sc_alias)){
			oryx_thread_mutex_lock(&tm->lock);
			return task;
		}
	}
	oryx_thread_mutex_unlock(&tm->lock);
	return NULL;
}

static struct oryx_task_t  *oryx_task_query (struct prefix_t *lp)
{
	switch (lp->cmd) {
		case LOOKUP_ID:
			return oryx_task_query_id (*(oryx_os_thread_t *)lp->v);
		case LOOKUP_ALIAS:
			return oryx_task_query_alias ((char *)lp->v);
		default:
			break;
	}

	return NULL;
}

void oryx_task_registry (struct oryx_task_t *task)
{
	struct oryx_task_t *t;
	
	if (unlikely(!task)) {
		return;
	}

	struct prefix_t p = {
		.cmd = LOOKUP_ALIAS,
		.v = task->sc_alias,
		.s = strlen (task->sc_alias),
	};

	t = oryx_task_query (&p);
	if (unlikely (!t)) {
		task_registry (task);
	}

	return;
}

void oryx_task_deregistry_id (oryx_os_thread_t pid)
{
	struct oryx_task_t *t;
	
	struct prefix_t p = {
		.cmd = LOOKUP_ID,
		.v = &pid,
		.s = __oryx_unused_val__,
	};
		
	t = oryx_task_query (&p);
	if (likely (t)) {
		task_deregistry (t);
	}
}


/** Thread description should not be same with registered one. */
struct oryx_task_t *oryx_task_spawn (char __oryx_unused__*alias, 
		uint32_t __oryx_unused__ ul_prio, void __oryx_unused__*attr,  void * (*handler)(void *), void *arg)
{
	if(unlikely(!alias)){
		return NULL;
	}

	struct prefix_t p = {
		.cmd = LOOKUP_ALIAS,
		.v = alias,
		.s = strlen (alias),
	};

	struct oryx_task_t *t;

	t =  oryx_task_query (&p);
	if (likely (t)) {
		return t;
	}

	/** else: no such task. */
	t = (struct oryx_task_t *)kmalloc(sizeof(struct oryx_task_t), 
										MPF_CLR, __oryx_unused_val__);
	if(unlikely(!t)){
		return NULL;
	}
	
	t->ul_prio = ul_prio;
	t->fn_handler = handler;
	t->argv = arg;
	INIT_LIST_HEAD(&t->list);
	memcpy(t->sc_alias, alias, strlen(alias));
	
	os_unsync();
	if (!pthread_create(&t->pid, NULL, t->fn_handler, t->argv) &&
	    		 (!pthread_detach(t->pid))) {
		
		task_registry(t);

		t->ul_flags |= TASK_RUNNING;
		t->ull_startup_time = time (NULL);	
		return t;
		
	}
	kfree (t);
	os_sync();
	return NULL;
}

void oryx_task_launch(void)
{
	struct oryx_task_t *t = NULL, *p;
	struct oryx_task_mgr_t *tm = &taskmgr;

	list_for_each_entry_safe(t, p, &tm->head, list) {
		
		/** task has been startup, so skip it. */
		if (t->pid > 0 && (t->ul_flags & TASK_RUNNING))
			continue;
		
		os_unsync();

		if (!pthread_create (&t->pid, DEFAULT_ATTR_VAL, t->fn_handler, t->argv) &&
	    		 (!pthread_detach (t->pid))) {
			t->ul_flags |= TASK_RUNNING;
			t->ull_startup_time = time (NULL);
		}

		os_sync();
	}
	
	sleep(1);
	
	printf("\r\nTask(s) %d Preview\n", tm->ul_count);

	printf ("\t%32s%16s%16s%20s\n", "Alias", "taskID", "coreID", "Stime");
	list_for_each_entry_safe(t, p, &tm->head, list) {
		
			char tmstr[100];
			tm_format (t->ull_startup_time, "%Y-%m-%d,%H:%M:%S", (char *)&tmstr[0], 100);

			fprintf(stdout, "\t\"%32s\"%16lX%16u%20s\n", t->sc_alias, t->pid, t->ul_lcore, tmstr);
	}

	printf("\r\n\r\n");

	return;
}

void oryx_task_initialize (void)
{
	struct oryx_task_mgr_t *tm = &taskmgr;

	INIT_LIST_HEAD(&tm->head);
	os_sync_init();
}

