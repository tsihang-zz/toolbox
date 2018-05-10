#ifndef __TASK_H__
#define __TASK_H__


struct oryx_task_t {

#define THIS 0

    /** Not used, setup with macro THIS. */
    const int module;

#define PID_INVALID (~0)
	oryx_os_thread_t pid;

#define TASK_NAME_SIZE 128
	char sc_alias[TASK_NAME_SIZE + 1];

#define INVALID_CORE (-1)
	/** lcore, kernel schedule if eque INVALID_CORE */
	u32 ul_lcore;

#define DEFAULT_ATTR_VAL	NULL
	/** attr of current task */
	//oryx_thread_attr_t *attr;

#define KERNEL_SCHED    (~0)
	/** priority of current task */
	u32 ul_prio;

	/** argument count */
	int argc;
	/** arguments */
	void *argv;

	/** Executive entry */
	void * (*fn_handler)(void *);

	/** allowed or forbidden */
#define TASK_CAN_BE_RECYCLABLE	(1 << 0)
#define TASK_RUNNING	(1 << 1)
	u32 ul_flags;

	struct list_head   list;

	u64 ull_startup_time;
	u64 ull_stop_time;

};

extern struct oryx_task_t * oryx_task_spawn(char *, uint32_t, void *, void * (*handler)(void *), void *);

void oryx_task_registry(struct oryx_task_t *);
void oryx_task_deregistry_id(oryx_os_thread_t);
void oryx_task_launch (void);
void oryx_task_initialize (void);

#define	DECLARE_TASK(x,...)   \
    __VA_ARGS__ struct oryx_task_t x;                             \
static void __oryx_task_registration__##x (void)                     \
    __attribute__((__constructor__)) ;                                  \
static void __oryx_task_registration__##x (void)                     \
{                                                                       \
    oryx_task_registry (&x);\
}                                                                       \
__VA_ARGS__ struct oryx_task_t x

#define TASK_DEFAULT_SETUP\
	.module = THIS,\
    .ul_lcore = INVALID_CORE,\
    .ul_prio = KERNEL_SCHED,\
    .argv = NULL,\
	.ul_flags = 0,\
	
#endif