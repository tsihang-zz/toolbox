#include "oryx.h"


#define PTHREAD_MUTEX_DEFAULT	NULL
#define PTHREAD_COND_DEFAULT	NULL

oryx_status_t oryx_thread_mutex_create(oryx_thread_mutex_t **m)
{
	(*m) = NULL;

	oryx_thread_mutex_t *new = kmalloc (sizeof(oryx_thread_mutex_t), MPF_CLR, __oryx_unused_val__);
	if (unlikely(!new))
		oryx_panic(-1,
			"kmalloc: %s", oryx_safe_strerror(errno));

	pthread_mutex_init (new, PTHREAD_MUTEX_DEFAULT);
	(*m) = new;

	return 0;
}

oryx_status_t oryx_thread_cond_create(oryx_thread_cond_t **c)
{
	(*c) = NULL;

	oryx_thread_cond_t *new = kmalloc (sizeof(oryx_thread_cond_t), MPF_CLR, __oryx_unused_val__);
	if (unlikely(!new))
		oryx_panic(-1,
			"kmalloc: %s", oryx_safe_strerror(errno));

	pthread_cond_init (new, PTHREAD_COND_DEFAULT);
	(*c) = new;

	return 0;
}

oryx_status_t oryx_thread_mutex_destroy(oryx_thread_mutex_t *m) 
{	
	int retval = thread_destroy_lock(m);
	kfree (m);
	return retval;

}

