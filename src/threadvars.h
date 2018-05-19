
#ifndef THREADVARS_H
#define THREADVARS_H

//#include "counters.h"

/** \brief Per thread variable structure */
typedef struct ThreadVars_ {
    pthread_t t;
    char name[16];
    char *printable_name;
    char *thread_group_name;
    SC_ATOMIC_DECLARE(unsigned int, flags);

    /** local id */
    int id;

    /* counters */
	struct CounterCtx perf_private_ctx0;
	
	struct ThreadVars_ *next;
    struct ThreadVars_ *prev;

}ThreadVars;

#endif
