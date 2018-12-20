#ifndef CLASSIFY_H
#define CLASSIFY_H

extern void classify_initialization(vlib_main_t *vm);
extern void classify_terminal(void);
extern void classify_runtime(void);

ORYX_DECLARE (
	int classify_offline (
		IN const char *oldpath,
		IN vlib_conf_t *conf
	)
);


#endif

