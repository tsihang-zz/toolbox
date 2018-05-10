#include "oryx.h"

#ifdef HAVE_APR
static oryx_pool_t *sync_pool = NULL;
#endif

#define PTHREAD_MUTEX_DEFAULT	NULL
#define PTHREAD_COND_DEFAULT	NULL

oryx_status_t oryx_thread_mutex_create(oryx_thread_mutex_t **mptr)
{
	(*mptr) = NULL;
#ifdef HAVE_APR
	if (!sync_pool) {
		apr_pool_create(&sync_pool, NULL);
		ABTS_PTR_NOTNULL (sync_pool);
		printf ("Sync pool allocate success\n");
	}

	apr_thread_mutex_create (mptr, APR_THREAD_MUTEX_DEFAULT, sync_pool);
	ABTS_PTR_NOTNULL (*mumptrtex);
#else
	oryx_thread_mutex_t *pthread_lock = kmalloc (sizeof(oryx_thread_mutex_t), MPF_CLR, __oryx_unused_val__);
	if (unlikely(!pthread_lock))
		return (-ERRNO_MUTEX_INIT);
	pthread_mutex_init (pthread_lock, PTHREAD_MUTEX_DEFAULT);
	(*mptr) = pthread_lock;
#endif
	return ORYX_SUCCESS;
}

oryx_status_t oryx_thread_cond_create(oryx_thread_cond_t **cond)
{
	(*cond) = NULL;
#ifdef HAVE_APR
	if (!sync_pool) {
		apr_pool_create(&sync_pool, NULL);
		ABTS_PTR_NOTNULL (sync_pool);
	}

	apr_thread_cond_create (cond, sync_pool);
	ABTS_PTR_NOTNULL (*cond);
#else
	oryx_thread_cond_t *pthread_cond = kmalloc (sizeof(oryx_thread_cond_t), MPF_CLR, __oryx_unused_val__);
	if (unlikely(!pthread_cond))
		return (-ERRNO_COND_INIT);
	pthread_cond_init (pthread_cond, PTHREAD_COND_DEFAULT);
	(*cond) = pthread_cond;

#endif
	return ORYX_SUCCESS;
}

oryx_status_t oryx_thread_mutex_destroy(oryx_thread_mutex_t *mptr) 
{	
#ifdef HAVE_APR
	return apr_thread_mutex_destroy(mptr);
#else

	int retval = thread_destroy_lock(mptr);
	kfree (mptr);
	return retval;
#endif
}

