
#include "oryx.h"

typedef struct oryx_tmr_mgr_t {
	/** sigtmr head */
	struct list_head sigtmr_head;

	os_mutex_t ol_sigtmr_lock;
	atomic64_t  sigtmr_cur_ticks;
	
	/** advanced tmr head */
	struct list_head tmr_head;
	os_mutex_t ol_tmr_lock;
	atomic64_t  tmr_cur_ticks;

	uint32_t ul_tmr_cnt;

	uint32_t ul_precision_msec;
} oryx_tmr_mgr_t;

struct oryx_tmr_mgr_t tmrmgr = {
	.ol_sigtmr_lock		= INIT_MUTEX_VAL,
	.sigtmr_cur_ticks	= ATOMIC_INIT(0),

	.ol_tmr_lock		= INIT_MUTEX_VAL,
	.tmr_cur_ticks		= ATOMIC_INIT(0),

	.ul_precision_msec	= TMR_DEFAULT_PRECISION,
};

void oryx_tmr_default_handler(struct oryx_timer_t *tmr, int __oryx_unused_param__ argc, 
                char __oryx_unused_param__**argv)
{
    fprintf (stdout, "default %s-timer routine has occured on [%s, %u, %d]\n",
		tmr->ul_setting_flags & TMR_OPTIONS_ADVANCED ? "advanced" : "sig",
		tmr->sc_alias, tmr->tmr_id, tmr->ul_cyclical_times);
}

static void *tmr_alloc(void)
{
    struct oryx_timer_t *new;

    new = kmalloc(sizeof(struct oryx_timer_t), MPF_CLR, -1);
	if (unlikely(!new))
		oryx_panic(-1,
			"kmalloc: %s", oryx_safe_strerror(errno));
	
    new->tmr_id			= TMR_INVALID;
   	new->routine		= oryx_tmr_default_handler;
    new->interval_ms	= (3 * 1000);
    new->ul_setting_flags = TMR_CAN_BE_RECYCLABLE;
			
    return new;
}

static __oryx_always_inline__ 
void tmr_free(struct oryx_timer_t *t)
{
    if(t->ul_setting_flags & TMR_CAN_BE_RECYCLABLE){
        kfree(t);
    }
}

static __oryx_always_inline__ 
oryx_tmr_id tmr_id_alloc(int __oryx_unused_param__ module, 
                const char *sc_alias, 
                size_t s)
{
	BUG_ON (sc_alias == NULL);
	BUG_ON (s < 1);
    return oryx_js_hash((void *)sc_alias, s);
}

static __oryx_always_inline__
void tmr_enable(struct oryx_timer_t *t)
{
    t->ul_setting_flags |= TMR_OPTIONS_ENABLE;
}

static __oryx_always_inline__
void tmr_disable(struct oryx_timer_t *t)
{
    t->ul_setting_flags &= ~TMR_OPTIONS_ENABLE;
}

static __oryx_always_inline__
void tmr_delete(struct oryx_timer_t *t)
{
    if(likely(t)){
        list_del(&t->list);
        tmr_free(t);
    }
}

static __oryx_always_inline__ 
void sigtmr_handler(void)
{
	int64_t old_ticks = 0;
	struct oryx_timer_t *_this = NULL, *p;
	struct oryx_tmr_mgr_t *tm = &tmrmgr;
	
    old_ticks = atomic64_inc(&tm->sigtmr_cur_ticks);
    do_mutex_lock(&tm->ol_sigtmr_lock);
        list_for_each_entry_safe (_this, p, &tm->sigtmr_head, list) {
            if (likely(_this->ul_setting_flags & TMR_OPTIONS_ENABLE)) {
                if (likely(old_ticks >= _this->curr_ticks)) {
                    _this->routine (_this, _this->argc, _this->argv);
					++ _this->ul_cyclical_times;
                    if (unlikely(!(_this->ul_setting_flags & TMR_OPTIONS_PERIODIC))) {
						fprintf (stdout, "sigtmr[%s, %u] stopped after running %u times\n", _this->sc_alias, _this->tmr_id, _this->ul_cyclical_times);
                        _this->ul_setting_flags &= ~TMR_OPTIONS_ENABLE;
					}
                    else {
                        _this->curr_ticks = old_ticks + _this->interval_ms;
                    }
                }
            }
        }
	do_mutex_unlock(&tm->ol_sigtmr_lock);
        
}

static __oryx_always_inline__ 
void tmr_handler(void)
{
	struct oryx_timer_t *_this = NULL, *p;
	struct oryx_tmr_mgr_t *tm = &tmrmgr;
	struct timeval now;
	int64_t old_ticks_us = 0, now_tick_us = 0;
	
    old_ticks_us = atomic64_read(&tm->tmr_cur_ticks);
	
	/** current ticks */
	gettimeofday(&now, NULL);
	now_tick_us = now.tv_sec * 1000000 + now.tv_usec;

	/** update tick. */
	atomic64_set(&tm->tmr_cur_ticks, now_tick_us);
	
	do_mutex_lock(&tm->ol_tmr_lock);
        list_for_each_entry_safe (_this, p, &tm->tmr_head, list) {
            if (likely(_this->ul_setting_flags & TMR_OPTIONS_ENABLE)) {
				/** arrival ??? */
                if (likely(old_ticks_us >= _this->curr_ticks)) {
                    _this->routine(_this, _this->argc, _this->argv);
                    ++ _this->ul_cyclical_times;
                    if (unlikely(!(_this->ul_setting_flags & TMR_OPTIONS_PERIODIC))) {
                        _this->ul_setting_flags &= ~TMR_OPTIONS_ENABLE;
					}
                    else {
                        _this->curr_ticks = old_ticks_us + _this->interval_ms * 1000;
                    }
                }
            }
        }
	do_mutex_unlock(&tm->ol_tmr_lock);
        
}

/** SIGTMR will NEVER used on a OS depended system 
 *  because it may change precision of ITIMER_REAL. */
static __oryx_always_inline__ 
void realtimer_init(void)
{
	struct oryx_tmr_mgr_t *tm = &tmrmgr;
	uint32_t sec = tm->ul_precision_msec / 1000;
	uint32_t msec = tm->ul_precision_msec % 1000;

	static struct itimerval tmr;
    
    oryx_register_sighandler(SIGALRM, (void *) sigtmr_handler);
    tmr.it_interval.tv_sec = sec;
    tmr.it_interval.tv_usec = msec * 1000;
    tmr.it_value.tv_sec = sec;
    tmr.it_value.tv_usec = msec * 1000;
	
    setitimer(ITIMER_REAL, &tmr, NULL);
    
}

static __oryx_always_inline__
void * tmr_daemon (void __oryx_unused_param__*pv_par )
{
	struct oryx_tmr_mgr_t *tm = &tmrmgr;
	
    fprintf (stdout, "  T M R   S T A R T E D ...\n\n\n\n");    

    FOREVER {
		/** sleep 1 sec. */
        usleep (tm->ul_precision_msec * 1000);
		tmr_handler();
    }
    
    oryx_task_deregistry_id(pthread_self());
    return NULL;
}

static struct oryx_task_t advanced_tmr_task =
{
	.module			= THIS,
	.sc_alias		= "Advanced Timer Task",
	.fn_handler		= tmr_daemon,
	.lcore_mask		= INVALID_CORE,
	.ul_prio		= KERNEL_SCHED,
	.argc			= 0,
	.argv			= NULL,
	.ul_flags		= 0,	/** Can not be recyclable. */
};

void oryx_tmr_start (struct oryx_timer_t *tmr)
{
	struct oryx_tmr_mgr_t *tm = &tmrmgr;
	os_mutex_t *lock = &tm->ol_sigtmr_lock;
	
	/** lookup alias. */
	if (tmr->ul_setting_flags  & TMR_OPTIONS_ADVANCED)
		lock = &tm->ol_tmr_lock;

    do_mutex_lock(lock);
	tmr_enable(tmr);
    do_mutex_unlock(lock);
}

void oryx_tmr_stop (struct oryx_timer_t *tmr)
{
	struct oryx_tmr_mgr_t *tm = &tmrmgr;
	os_mutex_t *lock = &tm->ol_sigtmr_lock;
	
	/** lookup alias. */
	if (tmr->ul_setting_flags  & TMR_OPTIONS_ADVANCED)
		lock = &tm->ol_tmr_lock;

    do_mutex_lock(lock);
	tmr_disable(tmr);
    do_mutex_unlock(lock);
}

void oryx_tmr_destroy (struct oryx_timer_t *tmr)
{
	struct oryx_tmr_mgr_t *tm = &tmrmgr;
	os_mutex_t *lock = &tm->ol_sigtmr_lock;
	
	/** lookup alias. */
	if (tmr->ul_setting_flags  & TMR_OPTIONS_ADVANCED)
		lock = &tm->ol_tmr_lock;

    do_mutex_lock(lock);
	tmr_delete(tmr);
    do_mutex_unlock(lock);
}

int oryx_tmr_initialize(void) 
{
	struct oryx_tmr_mgr_t *tm = &tmrmgr;

	INIT_LIST_HEAD (&tm->sigtmr_head);
	INIT_LIST_HEAD (&tm->tmr_head);

#if defined (HAVE_ADVANCED_TMR)
	oryx_task_registry (&advanced_tmr_task);
#else
	realtimer_init();
#endif

	return ORYX_SUCCESS;
}

/**
* routine: must be a reentrant function.
* An unknown error occurs if a thread-safe function called as a routione.
*/
struct oryx_timer_t *oryx_tmr_create (int module,
                const char *sc_alias, uint32_t ul_setting_flags,
                void (*handler)(struct oryx_timer_t *, int, char **), int argc, char **argv,
                uint32_t n_mseconds)
{
    struct oryx_timer_t *_this = NULL, *p;
	struct oryx_tmr_mgr_t *tm = &tmrmgr;
    struct list_head *h;
	os_mutex_t *lock;

	BUG_ON (sc_alias == NULL);

	/** lookup alias. */
	if (ul_setting_flags & TMR_OPTIONS_ADVANCED) {
		h = &tm->tmr_head;
		lock = &tm->ol_tmr_lock;
	}
	else {
		h = &tm->sigtmr_head;
		lock = &tm->ol_sigtmr_lock;
	}
	
	/** lookup advanced timer list. */
	do_mutex_lock(lock);
    list_for_each_entry_safe (_this, p, h, list){
        if(likely(!strcmp(sc_alias, _this->sc_alias)) &&
			(module == _this->module)){
            fprintf (stdout,  
                "The same timer (%s, %d)\n", _this->sc_alias, _this->tmr_id);
			do_mutex_unlock(lock);
            goto finish;
        }
    }
	
				
    _this = (struct oryx_timer_t *)tmr_alloc();
	_this->argc			= argc;
    _this->argv			= argv;
    _this->module		= module;
    _this->sc_alias		= sc_alias;
    _this->curr_ticks	= 0;
    _this->interval_ms	= n_mseconds;
    _this->routine		= handler ? handler : oryx_tmr_default_handler;
    _this->tmr_id		= tmr_id_alloc(_this->module, _this->sc_alias, strlen(_this->sc_alias));
    _this->ul_setting_flags = (ul_setting_flags | TMR_CAN_BE_RECYCLABLE);

    list_add_tail(&_this->list, h);
	do_mutex_unlock(lock);

finish:    
    return _this;
}

struct oryx_timer_t *oryx_tmr_create_loop (int module,
				const char *sc_alias,
				void (*handler)(struct oryx_timer_t *, int, char **), int argc, char **argv,
				uint32_t n_mseconds)
{
	uint32_t ul_setting_flags = (TMR_OPTIONS_PERIODIC | TMR_OPTIONS_ADVANCED);
	return oryx_tmr_create(module, sc_alias,
					ul_setting_flags, handler, argc, argv, n_mseconds);
}

