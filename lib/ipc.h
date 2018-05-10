#ifndef __IPC_H__
#define __IPC_H__

#ifdef HAVE_APR
#define INIT_MUTEX_VAL	{NULL, PTHREAD_MUTEX_INITIALIZER}
#define INIT_COND_VAL	{NULL, PTHREAD_COND_INITIALIZER}
#define oryx_thread_mutex_t	apr_pthread_mutex_t
#define oryx_thread_cond_t	apr_pthread_cond_t

#define thread_region_lock(lock)	apr_thread_mutex_lock(lock)
#define thread_region_unlock(lock)	apr_thread_mutex_unlock(lock)
#define thread_region_trylock(lock) apr_thread_mutex_trylock(lock)
#define thread_destroy_lock(lock)	apr_thread_mutex_destroy(lock)

#else
#define INIT_MUTEX_VAL	PTHREAD_MUTEX_INITIALIZER
#define INIT_COND_VAL	PTHREAD_COND_INITIALIZER
#define oryx_thread_mutex_t	pthread_mutex_t
#define oryx_thread_cond_t	pthread_cond_t

#define thread_region_lock(lock)	pthread_mutex_lock(lock)
#define thread_region_unlock(lock)	pthread_mutex_unlock(lock)
#define thread_region_trylock(lock) pthread_mutex_trylock(lock)
#define thread_destroy_lock(lock)	pthread_mutex_destroy(lock)

#define thread_cond_signal(s) pthread_cond_signal(s)
#define thread_cond_wait(s,m) pthread_cond_wait(s,m)
#endif

#define INIT_MUTEX(name)\
    oryx_thread_mutex_t name = INIT_MUTEX_VAL;

#define INIT_COND(name)\
    oryx_thread_cond_t name = INIT_COND_VAL;

extern oryx_status_t oryx_thread_mutex_create(oryx_thread_mutex_t **mptr);
extern oryx_status_t oryx_thread_cond_create(oryx_thread_cond_t **cond);
extern oryx_status_t oryx_thread_mutex_destroy(oryx_thread_mutex_t *mptr);
static __oryx_always_inline__
oryx_status_t oryx_thread_mutex_lock(oryx_thread_mutex_t *mptr) { return thread_region_lock(mptr); }
static __oryx_always_inline__
oryx_status_t oryx_thread_mutex_trylock(oryx_thread_mutex_t *mptr) { return thread_region_trylock(mptr); }
static __oryx_always_inline__
oryx_status_t oryx_thread_mutex_unlock(oryx_thread_mutex_t *mptr) { return thread_region_unlock(mptr); }

static __oryx_always_inline__
oryx_status_t oryx_thread_cond_signal(oryx_thread_cond_t *cptr) { return thread_cond_signal(cptr); }
static __oryx_always_inline__
oryx_status_t oryx_thread_cond_wait(oryx_thread_cond_t *cptr, oryx_thread_mutex_t *mptr) { return thread_cond_wait(cptr, mptr); }


typedef oryx_thread_mutex_t	os_lock_t;
typedef oryx_thread_cond_t	os_cond_t;

#define do_lock(lock)\
	oryx_thread_mutex_lock(lock)
#define do_unlock(lock)\
	oryx_thread_mutex_unlock(lock)

#endif
