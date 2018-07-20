#include "oryx.h"


#define PTHREAD_MUTEX_DEFAULT	NULL
#define PTHREAD_COND_DEFAULT	NULL

oryx_status_t oryx_thread_mutex_create(oryx_thread_mutex_t **mptr)
{
	(*mptr) = NULL;

	oryx_thread_mutex_t *pthread_lock = kmalloc (sizeof(oryx_thread_mutex_t), MPF_CLR, __oryx_unused_val__);
	if (unlikely(!pthread_lock))
		return (-ERRNO_MUTEX_INIT);
	pthread_mutex_init (pthread_lock, PTHREAD_MUTEX_DEFAULT);
	(*mptr) = pthread_lock;

	return ORYX_SUCCESS;
}

oryx_status_t oryx_thread_cond_create(oryx_thread_cond_t **cond)
{
	(*cond) = NULL;

	oryx_thread_cond_t *pthread_cond = kmalloc (sizeof(oryx_thread_cond_t), MPF_CLR, __oryx_unused_val__);
	if (unlikely(!pthread_cond))
		return (-ERRNO_COND_INIT);
	pthread_cond_init (pthread_cond, PTHREAD_COND_DEFAULT);
	(*cond) = pthread_cond;

	return ORYX_SUCCESS;
}

oryx_status_t oryx_thread_mutex_destroy(oryx_thread_mutex_t *mptr) 
{	
	int retval = thread_destroy_lock(mptr);
	kfree (mptr);
	return retval;

}

