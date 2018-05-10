#ifndef __TMR_H__
#define __TMR_H__

typedef uint32_t oryx_tmr_id;

/**
 * default precision in msec.
 */
#define TMR_DEFAULT_PRECISION	1000

/** 
 * Timer can be recycled by OS 
 * if it allocated by such malloc like memory allocation function. 
 */
#define TMR_CAN_BE_RECYCLABLE	(1 << 0)

/**
 * Cyclical timer or not. 
 */
#define TMR_OPTIONS_PERIODIC	(1 << 1)

/**
 * Enable or disable this timer.
 */
#define TMR_OPTIONS_ENABLE		(1 << 2)

/**
 * Advanced timer has a high precision than non-advanced timer
 * Non-advanced timer only work for some simple and undemanded task.
 */
#define TMR_OPTIONS_ADVANCED	(1 << 3)

struct oryx_timer_t {

#define TMR_INVALID  -1

    int module;

    /** unique id */
    oryx_tmr_id tmr_id;

    /** Make sure that allocate desc member with malloc like function. */
    char *sc_alias;

    /** in secs, default is 3 seconds. */
    int64_t interval_ms;

    int64_t curr_ticks;

    void (*routine)(struct oryx_timer_t *, int, char **);
    
    int argc;
    
    char **argv;
	
    u32 ul_setting_flags;

	u32 ul_cyclical_times;
	
    struct list_head list;
}oryx_timer_t;


#define DEFAULT_TMR_MODULE	1


extern oryx_status_t oryx_tmr_initialize (void);
extern void oryx_tmr_start (struct oryx_timer_t *tmr);
extern void oryx_tmr_stop (struct oryx_timer_t *tmr);
extern struct oryx_timer_t *oryx_tmr_create (int module,
                const char *desc, u32 ul_setting_flags,
                void (*handler)(struct oryx_timer_t *, int, char **), int argc, char **argv, u32 msec);
extern void oryx_tmr_destroy (struct oryx_timer_t *tmr);

#endif

