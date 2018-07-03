#ifndef __TASK_H__
#define __TASK_H__

#define DEFAULT_ATTR_VAL	NULL

struct oryx_task_t {
#define THIS 0
    const int			module;	/** Not used, setup with macro THIS. */

#define PID_INVALID (~0)
	oryx_os_thread_t	pid;

#define TASK_NAME_SIZE 128
	char				sc_alias[TASK_NAME_SIZE + 1];

#define INVALID_CORE (-1)
	uint32_t			ul_lcore;	/** lcore, kernel schedule if eque INVALID_CORE */

#define KERNEL_SCHED    (~0)
	uint32_t			ul_prio;/** priority of current task */
	int					argc;	/** argument count for this task. */
	void				*argv;	/** arguments array for this task. */
	void 				*(*fn_handler)(void *);	/** Executive entry */

	/** allowed or forbidden */
#define TASK_CAN_BE_RECYCLABLE	(1 << 0)
#define TASK_RUNNING	(1 << 1)
	uint32_t			ul_flags;

	struct list_head	list;

	uint64_t			ull_startup_time;
	uint64_t			ull_stop_time;

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
