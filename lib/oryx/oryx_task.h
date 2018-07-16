#ifndef ORYX_TASK_H
#define ORYX_TASK_H

#include "task.h"

extern struct oryx_task_t * oryx_task_spawn(
		const char		__oryx_unused_param__*alias, 
		const uint32_t	__oryx_unused_param__ ul_prio,
		void		__oryx_unused_param__*attr,
		void *		(*handler)(void *),
		void		*argv);

extern void oryx_task_registry(struct oryx_task_t *);
extern void oryx_task_deregistry_id(oryx_os_thread_t);
extern void oryx_task_launch (void);
extern void oryx_task_initialize (void);

#endif
