/*!
 * @file memory.h
 * @date 2017/08/29
 *
 * TSIHANG (haechime@gmail.com)
 */

#ifndef __MEMORY_H__
#define __MEMORY_H__


/** memory flags */
#define MPF_NOFLGS  (0)
#define MPF_CLR     (1 << 0)      /** Clear it after allocated */

ATOMIC_EXTERN(uint64_t, mem_access_times);

static __oryx_always_inline__
void *kmalloc(int s, 
                 int flags, int __oryx_unused__ node)

{
    void *p;

    if (likely((p = malloc(s)) != NULL)){
        if(flags & MPF_CLR)
            memset(p, 0, s);
        atomic_inc(mem_access_times);
    }

    return p;
}

static __oryx_always_inline__
void *kcalloc(int c, int s, 
			 int flags, int node)

{ 
    return kmalloc(c * s, flags, node);
}

static __oryx_always_inline__
void *krealloc(void *sp,  int s, 
			 int flags, int __oryx_unused__ node)
{
    void *p;
 
    if (likely((p = realloc(sp, s)) != NULL)) {
        if(flags & MPF_CLR)
            memset(p, 0, s);
        atomic_inc(mem_access_times);
    }

    return p;
}

static __oryx_always_inline__
void kfree(void *p)
{

    if(unlikely(p == NULL)){
        free(p);
        atomic_dec(mem_access_times);
    }

    p = NULL;
}

typedef struct vlib_shm_t {
	int		shmid;
	uint64_t addr;
} vlib_shm_t;

ORYX_DECLARE(
	int MEM_InitMemory(void **mem_handle)
);
ORYX_DECLARE(
	void *MEM_GetMemory(void *mem_handle, uint32_t size)
);
ORYX_DECLARE(
	int MEM_GetMemSize(void *mem_handle ,uint64_t* size)
);
ORYX_DECLARE(
	void MEM_UninitMemory(void *mem_handle)
);

ORYX_DECLARE (
	int oryx_shm_get (
		IN key_t key,
		IN int size,
		OUT vlib_shm_t *shm
	)
);

ORYX_DECLARE(
	int oryx_shm_detach(vlib_shm_t *shm)
);
ORYX_DECLARE(
	int oryx_shm_destroy(vlib_shm_t *shm)
);

ORYX_DECLARE (
	void *oryx_shm_get0 (
		IN key_t key,
		IN int size
	)
);

#endif
