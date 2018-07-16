#ifndef __TASK_H__
#define __TASK_H__

#define THIS 0
#define INVALID_CORE		(-1)
#define KERNEL_SCHED    	(~0)

#define PID_INVALID (~0)

/* task.ul_flags. */
#define TASK_CAN_BE_RECYCLABLE	(1 << 0)	/** allowed or forbidden */
#define TASK_RUNNING			(1 << 1)

struct oryx_task_t {
    const int			module;					/** Not used, setup with macro THIS. */
	oryx_os_thread_t	pid;
	const char			*sc_alias;
	uint32_t			ul_lcore;				/** lcore, kernel schedule if eque INVALID_CORE */
	uint32_t			ul_prio;				/** priority of current task */
	int					argc;					/** argument count for this task. */
	void				*argv;					/** arguments array for this task. */
	void 				*(*fn_handler)(void *);	/** Executive entry */	
	uint32_t			ul_flags;
	struct list_head	list;
	uint64_t			ull_startup_time;
	uint64_t			ull_stop_time;

#define ORYX_TASK_INIT_VAL	{\
		.module			=	THIS,\
		.ul_lcore		=	INVALID_CORE,\
		.ul_prio		=	KERNEL_SCHED,\
		.argv			=	NULL,\
		.ul_flags		=	0,\
		.ull_startup_time	=	0,\
		.ull_stop_time		=	0,\
		.list				=	{NULL, NULL},\
	}

};

/** task has been startup, so skip it. */
#define task_is_running(t)\
	(((t)->pid > 0 && ((t)->ul_flags & TASK_RUNNING)))
	
#endif
