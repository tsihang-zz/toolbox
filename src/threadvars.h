
#ifndef THREADVARS_H
#define THREADVARS_H

#include "counters.h"

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

    /** public counter store: counter syncs update this */
    StatsPublicThreadContext perf_public_ctx;

    /** private counter store: counter updates modify this */
    StatsPrivateThreadContext perf_private_ctx;

	struct ThreadVars_ *next;
    struct ThreadVars_ *prev;

}ThreadVars;

#endif
