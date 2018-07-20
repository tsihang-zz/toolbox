#ifndef __IPC_H__
#define __IPC_H__


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

typedef pthread_mutex_t	os_mutex_t;
#define do_mutex_init(lock)\
	pthread_mutex_init(lock, PTHREAD_MUTEX_DEFAULT)
#define do_mutex_lock(lock)\
	oryx_thread_mutex_lock(lock)
#define do_mutex_unlock(lock)\
	oryx_thread_mutex_unlock(lock)
#define do_mutex_trylock(lock)\
	oryx_thread_mutex_trylock(lock)
#define do_mutex_destroy(lock)\
	pthread_mutex_destroy(lock)

typedef pthread_cond_t	os_cond_t;
#define do_cond_signal(cond)\
	oryx_thread_cond_signal(cond)
#define do_cond_destroy(cond)\
	pthread_cond_destroy(cond)
#define do_cond_wait(cond, m)\
	pthread_cond_wait

/* rwlocks */
typedef pthread_rwlock_t	os_rwlock_t;
#define do_rwlock_init(rwl, rwlattr)\
	pthread_rwlock_init(rwl, rwlattr)
#define do_rwlock_lock_wr(rwl)\
	pthread_rwlock_wrlock(rwl)
#define do_rwlock_lock_rd(rwl)\
	pthread_rwlock_rdlock(rwl)
#define do_rwlock_trylock_wr(rwl)\
	pthread_rwlock_trywrlock(rwl)
#define do_rwlock_trylock_rd(rwl)\
	pthread_rwlock_tryrdlock(rwl)
#define do_rwlock_unlock(rwl)\
	pthread_rwlock_unlock(rwl)
#define do_rwlock_destroy(rwl)\
	pthread_rwlock_destroy(rwl)

/* spinlocks */
typedef pthread_spinlock_t	os_spinlock_t;
#define do_spin_lock(spin)\
	pthread_spin_lock(spin)
#define do_spin_trylock(spin)\
	pthread_spin_trylock(spin)
#define do_spin_unlock(spin)\
	pthread_spin_unlock(spin)
#define do_spin_init(spin, spin_attr)\
	pthread_spin_init(spin, spin_attr)
#define do_spin_destroy(spin)\
	pthread_spin_destroy(spin)


#endif
