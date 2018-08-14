#ifndef __TMR_H__
#define __TMR_H__

#define DEFAULT_TMR_MODULE	1

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
    int				module;
    oryx_tmr_id		tmr_id;			/** unique id */
    const char		*sc_alias;		/** make sure that allocate desc member with malloc like function. */
    int64_t			interval_ms;	/** in secs, default is 3 seconds. */
    int64_t			curr_ticks;

	int				argc;
	char			**argv;
	void			(*routine)(struct oryx_timer_t *, int, char **);
	
    uint32_t		ul_setting_flags;
	uint32_t		ul_cyclical_times;
	
    struct list_head list;
};

#endif

