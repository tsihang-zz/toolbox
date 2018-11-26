#include "oryx.h"

#define PTHREAD_MUTEX_DEFAULT	NULL
#define PTHREAD_COND_DEFAULT	NULL

int
oryx_tm_create
(
	OUT os_mutex_t **m
)
{
	(*m) = NULL;

	os_mutex_t *new = kmalloc (sizeof(os_mutex_t), MPF_CLR, __oryx_unused_val__);
	if (unlikely(!new))
		oryx_panic(-1,
			"kmalloc: %s", oryx_safe_strerror(errno));

	pthread_mutex_init (new, PTHREAD_MUTEX_DEFAULT);
	(*m) = new;

	return 0;
}

int
oryx_tm_destroy
(
	IN os_mutex_t *m
) 
{	
	int retval = thread_destroy_lock(m);
	kfree (m);
	return retval;

}

int
oryx_tc_create
(
	OUT os_cond_t **c
)
{
	(*c) = NULL;

	os_cond_t *new = kmalloc (sizeof(os_cond_t), MPF_CLR, __oryx_unused_val__);
	if (unlikely(!new))
		oryx_panic(-1,
			"kmalloc: %s", oryx_safe_strerror(errno));

	pthread_cond_init (new, PTHREAD_COND_DEFAULT);
	(*c) = new;

	return 0;
}



