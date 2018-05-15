#ifndef __MEMORY_H__
#define __MEMORY_H__

/** memory flags */
#define MPF_NOFLGS  (0)
#define MPF_CLR     (1 << 0)      /** Clear it after allocated */

void *kmalloc(int s, 
                 int flags, int __oryx_unused__ node);

void *kcalloc(int c, int s, 
				int flags, int node);

void *krealloc(void *sp,  int s, 
				int flags, int __oryx_unused__ node);

void kfree(void *p);


#endif
