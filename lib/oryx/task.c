/*!
 * @file task.c
 * @date 2017/08/29
 *
 * TSIHANG (haechime@gmail.com)
 */

#include "oryx.h"

struct oryx_task_mgr_t {	
	sys_mutex_t			mtx;				/** mutex. */
	struct list_head	head;				/** all tasks are hold by head. */
	uint32_t			ul_count;			/** tasks statistics. */
	uint32_t			cfg;	/** task manager settings. */
	int					nr_cpus;			/** total cpus. */
};

static struct oryx_task_mgr_t taskmgr = {
	.cfg	=	0,
	.ul_count			=	0,
	.nr_cpus			=	0,
};


/** For thread sync */
static sem_t oryx_task_sync_sem;

/** All system semphore is initialized in this api */
static void 
os_sync_init(void)
{
    sem_init(&oryx_task_sync_sem, 0, 1);
    /** Add other semphore here */
}

static void
os_unsync(void)
{
    sem_post(&oryx_task_sync_sem);
}

static void
os_sync(void)
{
    sem_wait(&oryx_task_sync_sem);
}

static void
task_deregistry 
(
	IN struct oryx_task_t *task
)
{
	struct oryx_task_mgr_t *tm = &taskmgr;

	oryx_sys_mutex_lock(&tm->mtx);
	list_del(&task->list);	
	tm->ul_count --;
	oryx_sys_mutex_unlock(&tm->mtx);

	if(task->ul_flags & TASK_CAN_BE_RECYCLABLE)
		kfree(task);
}

static void 
task_registry 
(
	IN struct oryx_task_t *task
)
{
	struct oryx_task_mgr_t *tm = &taskmgr;

	oryx_sys_mutex_lock(&tm->mtx);
	list_add_tail(&task->list, &tm->head);	
	tm->ul_count ++;
	oryx_sys_mutex_unlock(&tm->mtx);
}

static struct oryx_task_t*
oryx_task_query_id 
(
	IN sys_thread_t pid
)
{
	struct oryx_task_t *task = NULL, *p;
	struct oryx_task_mgr_t *tm = &taskmgr;

	oryx_sys_mutex_lock(&tm->mtx);
	list_for_each_entry_safe(task, p, &tm->head, list){
		if (pid == task->pid){
			oryx_sys_mutex_lock(&tm->mtx);
			return task;
		}
	}
	oryx_sys_mutex_unlock(&tm->mtx);
	return NULL;
}

static struct oryx_task_t*
oryx_task_query_alias
(
	IN char *sc_alias
)
{
	struct oryx_task_t *task = NULL, *p;
	struct oryx_task_mgr_t *tm = &taskmgr;

	oryx_sys_mutex_lock(&tm->mtx);
	list_for_each_entry_safe(task, p, &tm->head, list){
		if (!strcmp(sc_alias, task->sc_alias)){
			oryx_sys_mutex_lock(&tm->mtx);
			return task;
		}
	}
	oryx_sys_mutex_unlock(&tm->mtx);
	return NULL;
}

static struct oryx_task_t*
oryx_task_query 
(
	IN struct prefix_t *lp
)
{
	switch (lp->cmd) {
		case LOOKUP_ID:
			return oryx_task_query_id (*(sys_thread_t *)lp->v);
		case LOOKUP_ALIAS:
			return oryx_task_query_alias ((char *)lp->v);
		default:
			break;
	}

	return NULL;
}

void
oryx_task_registry 
(
	IN struct oryx_task_t *task
)
{	
	BUG_ON(task == NULL);

	struct prefix_t p = {
		.cmd	=	LOOKUP_ALIAS,
		.v		=	task->sc_alias,
		.s		=	strlen (task->sc_alias),
	};

	if (unlikely(!oryx_task_query (&p)))
		task_registry (task);
}

void
oryx_task_deregistry_id 
(
	IN sys_thread_t pid
)
{
	struct prefix_t p = {
		.cmd	=	LOOKUP_ID,
		.v		=	&pid,
		.s		=	__oryx_unused_val__,
	};

	struct oryx_task_t *t = oryx_task_query (&p);

	if (likely(t)) {
		task_deregistry (t);
	}
}


/** Thread description should not be same with registered one. */
struct oryx_task_t*
oryx_task_spawn
(
	IN const char		__oryx_unused__*alias, 
	IN const uint32_t	__oryx_unused__ ul_prio,
	IN void		__oryx_unused__*attr,
	IN void *		(*handler)(void *),
	IN void		*argv
)
{
	BUG_ON(alias == NULL);

	struct oryx_task_t *t;
	struct prefix_t p = {
		.cmd	=	LOOKUP_ALIAS,
		.v		=	alias,
		.s		=	strlen (alias),
	};

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
	
	t->ul_prio		=	ul_prio;
	t->fn_handler	=	handler;
	t->argv			=	argv;
	t->sc_alias		=	alias;
	INIT_LIST_HEAD(&t->list);
	
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

void 
oryx_task_launch(void)
{
	struct oryx_task_t *t = NULL, *p;
	struct oryx_task_mgr_t *tm = &taskmgr;
	cpu_set_t cpu_info;
	int lcore_index = 0;

	list_for_each_entry_safe(t, p, &tm->head, list) {
		if (task_is_running(t))
			continue;
		
		os_unsync();

		if (!pthread_create (&t->pid, NULL, t->fn_handler, t->argv) &&
	    		 (!pthread_detach (t->pid))) {
	    		 
	    	/* bind thread to cpu logic core */
	    	if (t->lcore_mask != INVALID_CORE) {
				CPU_ZERO(&cpu_info);
				for (lcore_index = 0; lcore_index < tm->nr_cpus; lcore_index ++) {
					if (t->lcore_mask & (1 << lcore_index)) {
						CPU_SET(lcore_index, &cpu_info);
					}
				}

				if (0 != pthread_setaffinity_np(t->pid, sizeof(cpu_set_t), &cpu_info)) {
					fprintf(stderr, "pthread_setaffinity_np: %s\n", oryx_safe_strerror(errno));
				}
			}

			t->ul_flags			|= TASK_RUNNING;
			t->ull_startup_time	= time (NULL);
#if 0
			/* Check the actual affinity mask assigned to the thread */
			if (pthread_getaffinity_np(t->pid, sizeof(cpu_set_t), &cpu_info) == 0) {
				int j;
				for (j = 0; j < CPU_SETSIZE; j++)
					if (CPU_ISSET(j, &cpu_info))
						fprintf (stdout, "	CPU %d\n", j);
			}
#endif
		}
		os_sync();
	}
	
	sleep(1);
	
	fprintf (stdout, "\r\nTask(s) %d Preview\n", tm->ul_count);
	fprintf (stdout, "%10s%32s%16s%20s%20s\n", " ", "Alias", "TaskID", "CoreMask", "StartTime");
	list_for_each_entry_safe(t, p, &tm->head, list) {
		char tmstr[100];
		oryx_fmt_time (t->ull_startup_time, "%Y-%m-%d,%H:%M:%S", (char *)&tmstr[0], 100);
		fprintf (stdout, "%10s%32s%16lX%20lX%20s\n", " ", t->sc_alias, t->pid, t->lcore_mask, tmstr);
	}

	fprintf (stdout, "\r\n\r\n");

	return;
}

void 
oryx_task_initialize (void)
{
	struct oryx_task_mgr_t *tm = &taskmgr;

	INIT_LIST_HEAD(&tm->head);
	tm->nr_cpus	=	(int)sysconf(_SC_NPROCESSORS_ONLN);
	oryx_sys_mutex_create(&tm->mtx);
	os_sync_init();
}

