/*!
 * @file task.h
 * @date 2017/08/29
 *
 * TSIHANG (haechime@gmail.com)
 */

#ifndef __TASK_H__
#define __TASK_H__

#define THIS 0

#if defined(HAVE_SCHED_H)
#ifndef __CPU_SETSIZE
#define __CPU_SETSIZE	1024
#endif

#ifndef __NCPUBITS
#define __NCPUBITS		(8 * sizeof(__cpu_mask))
#endif

/* Type for array elements in ‘cpu_set‘.  */
typedef unsigned long int __cpu_mask;

typedef struct {
	__cpu_mask __bits[__CPU_SETSIZE / __NCPUBITS];
}cpu_set_t;
#endif

#define INVALID_CORE		((typeof(__cpu_mask))(~0))
#define KERNEL_SCHED    	(~0)

/* task.ul_flags. */
#define TASK_CAN_BE_RECYCLABLE	(1 << 0)	/** allowed or forbidden */
#define TASK_RUNNING			(1 << 1)

struct oryx_task_t {
    const int			module;					/** Not used, setup with macro THIS. */
	oryx_os_thread_t	pid;
	const char			*sc_alias;
	__cpu_mask			lcore_mask;			/** lcore, kernel schedule if eque INVALID_CORE */
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
		.lcore_mask		=	INVALID_CORE,\
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
