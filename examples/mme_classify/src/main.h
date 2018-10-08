#ifndef MAIN_H
#define MAIN_H

#define VLIB_ENQUEUE_HANDLER_EXITED		(1 << 0)
#define VLIB_ENQUEUE_HANDLER_STARTED	(1 << 1)

typedef struct vlib_main_t {
	int			argc;
	char			**argv;
	const char		*prgname; /* Name for e.g. syslog. */
	volatile uint32_t	ul_flags;
	int			nr_mmes;
	int			nr_threads;
	const char *mme_dictionary;
	const char *classify_warehouse;

	struct oryx_htable_t	*mme_htable;
} vlib_main_t;

extern vlib_main_t vlib_main;
extern int running;

#endif

