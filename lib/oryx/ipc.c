/*!
 * @file ipc.c
 * @date 2017/08/29
 *
 * TSIHANG (haechime@gmail.com)
 */

#include "oryx.h"


__oryx_always_extern__
void oryx_sys_mutex_create
(
	IO sys_mutex_t *mtx
)
{
	int err;
	pthread_mutexattr_t a;
	
	err = pthread_mutexattr_init(&a);
	if (err) {
		oryx_panic(errno,
			"%s", oryx_safe_strerror(errno));
	}
	
	err = pthread_mutexattr_settype(&a, PTHREAD_MUTEX_ERRORCHECK);
	if (err) {
		oryx_panic(errno,
			"%s", oryx_safe_strerror(errno));		
	}
	
	pthread_mutex_t *new = kmalloc (sizeof(pthread_mutex_t), MPF_CLR, __oryx_unused_val__);
	if (unlikely(!new))
		oryx_panic(errno,
			"kmalloc: %s", oryx_safe_strerror(errno));
	
	err = pthread_mutex_init (new, &a);
	if (err) {
		kfree(new);
		oryx_panic(errno,
			"%s", oryx_safe_strerror(errno));
	}

	mtx->bf_mutex = new;
}

__oryx_always_extern__
int oryx_sys_mutex_destroy
(
	IN sys_mutex_t *mtx
) 
{	
	int err;
	
	err = pthread_mutex_destroy((pthread_mutex_t *)(mtx->bf_mutex));
	if (err) {
	  return err;
	} else {
	  kfree(mtx->bf_mutex);
	  mtx->bf_mutex = NULL;
	  return 0;
	}
}

__oryx_always_extern__
int oryx_sys_mutex_lock
(
	IN sys_mutex_t *mtx
)
{
  pthread_mutex_t *m = (pthread_mutex_t *)(mtx->bf_mutex);

  return (pthread_mutex_lock(m));
}

__oryx_always_extern__
int oryx_sys_mutex_trylock
(
	IN sys_mutex_t *mtx
)
{
  pthread_mutex_t *m = (pthread_mutex_t *)(mtx->bf_mutex);

  return (pthread_mutex_trylock(m));
}

__oryx_always_extern__
int oryx_sys_mutex_timedlock
(
	IN sys_mutex_t *mtx,
	IN long abs_sec,
	IN long abs_nsec
)
{
  pthread_mutex_t *m = (pthread_mutex_t *)(mtx->bf_mutex);
  struct timespec tm;
  tm.tv_sec = abs_sec;
  tm.tv_nsec = abs_nsec;

  return (pthread_mutex_timedlock(m, &tm));
}

__oryx_always_extern__
int oryx_sys_mutex_unlock
(
	IN sys_mutex_t *mtx
)
{
  pthread_mutex_t *m = (pthread_mutex_t *)(mtx->bf_mutex);

  return (pthread_mutex_unlock(m));
}


/*
 * Conditional Variable APIs.
 */

__oryx_always_extern__
void oryx_sys_cond_create
(
	OUT sys_cond_t *c
)
{
	int err;
	
	pthread_cond_t *new = kmalloc (sizeof(pthread_cond_t), MPF_CLR, __oryx_unused_val__);
	if (unlikely(!new))
		oryx_panic(errno,
			"kmalloc: %s", oryx_safe_strerror(errno));

	err = pthread_cond_init (new, NULL);
	if (err) {
		kfree(new);
		oryx_panic(errno,
			"%s", oryx_safe_strerror(errno));		
	}
	
	c->bf_cond = new;

}

__oryx_always_extern__
int oryx_sys_cond_destroy
(
	IN sys_cond_t *c
)
{
  int err;

  pthread_cond_t *cv = c->bf_cond;
  err = pthread_cond_destroy(cv);
  if (err) {
  	oryx_loge(errno,
		"%s\n", oryx_safe_strerror(errno));
  }

  return err;
}

__oryx_always_extern__
int oryx_sys_cond_wait
(
	IN sys_cond_t *c,
	IN sys_mutex_t *m
)
{
  pthread_cond_t *cv = c->bf_cond;
  pthread_mutex_t *mtx = m->bf_mutex;
  return pthread_cond_wait(cv, mtx);
}

__oryx_always_extern__
int oryx_sys_cond_wake
(
	IN sys_cond_t *c
)
{
  pthread_cond_t *cv = c->bf_cond;
  return pthread_cond_signal(cv);
}

__oryx_always_extern__
int oryx_sys_cond_broadcast
(
	IN sys_cond_t *c
)
{
  pthread_cond_t *cv = c->bf_cond;
  return pthread_cond_broadcast(cv);
}



