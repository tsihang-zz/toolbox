#ifndef DP_MAIN_H
#define DP_MAIN_H

extern void
notify_dp(vlib_main_t *vm, int signum);
extern void
dp_start(vlib_main_t *vm);
extern void
dp_end(vlib_main_t *vm);

#endif