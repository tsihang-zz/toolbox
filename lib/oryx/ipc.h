/*!
 * @file ipc.h
 * @date 2017/08/29
 *
 * TSIHANG (haechime@gmail.com)
 */

#ifndef __IPC_H__
#define __IPC_H__

/* mutex */
typedef struct sys_mutex_t {
	void *bf_mutex;	/* OS abstracted context pointer */
} sys_mutex_t;

/* cond */
typedef struct sys_cond_t {
	void *bf_cond;
} sys_cond_t;

#endif
