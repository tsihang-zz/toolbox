#ifndef CLASSIFY_H
#define CLASSIFY_H

extern void classify_env_init(vlib_main_t *vm);
extern void classify_terminal(void);
extern void classify_runtime(void);
extern void classify_offline(const char *oldpath);

#endif

