#ifndef __MEMORY_H__
#define __MEMORY_H__

extern atomic64_t mem_access_times;

/** memory flags */
#define MPF_NOFLGS  (0)
#define MPF_CLR     (1 << 0)      /** Clear it after allocated */

static __oryx_always_inline__
void *kmalloc(int s, 
                 int flags, int __oryx_unused_param__ node)

{
    void *p;

    if(likely((p = malloc(s)) != NULL)){
        if(flags & MPF_CLR)
            memset(p, 0, s);
        atomic64_inc(&mem_access_times);
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
			 int flags, int __oryx_unused_param__ node)
{
    void *p;
 
    if(likely((p = realloc(sp, s)) != NULL)) {
        if(flags & MPF_CLR)
            memset(p, 0, s);
        atomic64_inc(&mem_access_times);
    }

    return p;
}

static __oryx_always_inline__
void kfree(void *p)
{

    if(likely(p)){
        free(p);
        atomic64_dec(&mem_access_times);
    }

    p = NULL;
}

int MEM_InitMemory(void **mem_handle);
void *MEM_GetMemory(void *mem_handle, uint32_t size);
int MEM_GetMemSize(void *mem_handle ,uint64_t* size);
void MEM_UninitMemory(void *mem_handle);

#endif
