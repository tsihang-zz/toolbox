#ifndef CLASSIFY_H
#define CLASSIFY_H

extern void classify_initialization(vlib_main_t *vm);
extern void classify_terminal(void);
extern void classify_runtime(void);
extern int classify_offline(const char *oldpath, vlib_fkey_t *fkey);

#endif

