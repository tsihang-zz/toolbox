#ifndef DP_MAIN_H
#define DP_MAIN_H

extern void
notify_dp(vlib_main_t *vm, int signum);
extern void
dp_start(struct vlib_main_t *vm);
extern void
dp_end(struct vlib_main_t *vm);

#endif