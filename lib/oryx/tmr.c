/*!
 * @file tmr.h
 * @date 2017/08/29
 *
 * TSIHANG (haechime@gmail.com)
 */

#include "oryx.h"

typedef struct oryx_tmr_mgr_t {
	/** sigtmr head */
	struct list_head sigtmr_head;

	sys_mutex_t ol_sigtmr_lock;
	ATOMIC_DECLARE(uint64_t, sigtmr_cur_ticks);
	
	/** advanced tmr head */
	struct list_head tmr_head;
	sys_mutex_t ol_tmr_lock;
	ATOMIC_DECLARE(uint64_t, tmr_cur_ticks);

	uint32_t ul_tmr_cnt;

	uint32_t precision;
} oryx_tmr_mgr_t;

struct oryx_tmr_mgr_t tmrmgr = {
	.precision			= TMR_DEFAULT_PRECISION,
};

void
oryx_tmr_default_handler
(
	IN struct oryx_timer_t *tmr,
	IN int __oryx_unused__	argc,
	IN char __oryx_unused__	**argv
)
{
    fprintf (stdout, "default %s-timer routine has occured on [%s, %u, %d]\n",
		tmr->cfg & TMR_OPTIONS_ADVANCED ? "advanced" : "sig",
		tmr->sc_alias, tmr->tmr_id, tmr->ul_cyclical_times);
}

static void*
tmr_alloc(void)
{
    struct oryx_timer_t *new;

    new = kmalloc(sizeof(struct oryx_timer_t), MPF_CLR, -1);
	if (unlikely(!new))
		oryx_panic(-1,
			"kmalloc: %s", oryx_safe_strerror(errno));
	
    new->tmr_id			= TMR_INVALID;
   	new->routine		= oryx_tmr_default_handler;
    new->interval		= (3 * 1000);
    new->cfg = TMR_CAN_BE_RECYCLABLE;
			
    return new;
}

static void
tmr_free
(
	IN struct oryx_timer_t *t
)
{
    if(t->cfg & TMR_CAN_BE_RECYCLABLE){
        kfree(t);
    }
}

static oryx_tmr_id
tmr_id_alloc
(
	IN int __oryx_unused__ module,
	IN const char *sc_alias,
	IN size_t s
)
{
	BUG_ON (sc_alias == NULL);
	BUG_ON (s < 1);
    return oryx_js_hash((void *)sc_alias, s);
}

static __oryx_always_inline__
void tmr_enable
(
	IN struct oryx_timer_t *t
)
{
    t->cfg |= TMR_OPTIONS_ENABLE;
}

static __oryx_always_inline__
void tmr_disable
(
	IN struct oryx_timer_t *t
)
{
    t->cfg &= ~TMR_OPTIONS_ENABLE;
}

static __oryx_always_inline__
void tmr_delete
(
	IN struct oryx_timer_t *t
)
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
	
    old_ticks = atomic_inc(tm->sigtmr_cur_ticks);
    oryx_sys_mutex_lock(&tm->ol_sigtmr_lock);
        list_for_each_entry_safe (_this, p, &tm->sigtmr_head, list) {
            if (likely(_this->cfg & TMR_OPTIONS_ENABLE)) {
                if (likely(old_ticks >= _this->curr_ticks)) {
                    _this->routine (_this, _this->argc, _this->argv);
					++ _this->ul_cyclical_times;
                    if (unlikely(!(_this->cfg & TMR_OPTIONS_PERIODIC))) {
						fprintf (stdout, "sigtmr[%s, %u] stopped after running %u times\n", _this->sc_alias, _this->tmr_id, _this->ul_cyclical_times);
                        _this->cfg &= ~TMR_OPTIONS_ENABLE;
					}
                    else {
                        _this->curr_ticks = old_ticks + _this->interval;
                    }
                }
            }
        }
	oryx_sys_mutex_unlock(&tm->ol_sigtmr_lock);
        
}

static __oryx_always_inline__ 
void tmr_handler(void)
{
	struct oryx_timer_t *_this = NULL, *p;
	struct oryx_tmr_mgr_t *tm = &tmrmgr;
	struct timeval now;
	int64_t old_ticks_us = 0, now_tick_us = 0;
	
    old_ticks_us = atomic_read(tm->tmr_cur_ticks);
	
	/** current ticks */
	gettimeofday(&now, NULL);
	now_tick_us = now.tv_sec * 1000000 + now.tv_usec;

	/** update tick. */
	atomic_set(tm->tmr_cur_ticks, now_tick_us);
	
	oryx_sys_mutex_lock(&tm->ol_tmr_lock);
        list_for_each_entry_safe (_this, p, &tm->tmr_head, list) {
            if (likely(_this->cfg & TMR_OPTIONS_ENABLE)) {
				/** arrival ??? */
                if (likely(old_ticks_us >= _this->curr_ticks)) {
                    _this->routine(_this, _this->argc, _this->argv);
                    ++ _this->ul_cyclical_times;
                    if (unlikely(!(_this->cfg & TMR_OPTIONS_PERIODIC))) {
                        _this->cfg &= ~TMR_OPTIONS_ENABLE;
					}
                    else {
                        _this->curr_ticks = old_ticks_us + _this->interval * 1000;
                    }
                }
            }
        }
	oryx_sys_mutex_unlock(&tm->ol_tmr_lock);
        
}

/** SIGTMR will NEVER used on a OS depended system 
 *  because it may change precision of ITIMER_REAL. */
static __oryx_always_inline__ 
void realtimer_init(void)
{
	struct oryx_tmr_mgr_t *tm = &tmrmgr;
	uint32_t sec = tm->precision / 1000;
	uint32_t msec = tm->precision % 1000;

	static struct itimerval tmr;
    
    oryx_register_sighandler(SIGALRM, (void *) sigtmr_handler);
    tmr.it_interval.tv_sec = sec;
    tmr.it_interval.tv_usec = msec * 1000;
    tmr.it_value.tv_sec = sec;
    tmr.it_value.tv_usec = msec * 1000;
	
    setitimer(ITIMER_REAL, &tmr, NULL);
    
}

static __oryx_always_inline__
void * tmr_daemon
(
	IN void __oryx_unused__*pv_par
)
{
	struct oryx_tmr_mgr_t *tm = &tmrmgr;
	
    fprintf (stdout, "  T M R   S T A R T E D ...\n\n\n\n");    

    FOREVER {
		/** sleep 1 sec. */
        usleep (tm->precision * 1000);
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

void
oryx_tmr_start 
(
	IN struct oryx_timer_t *tmr
)
{
	struct oryx_tmr_mgr_t *tm = &tmrmgr;
	sys_mutex_t *mtx = &tm->ol_sigtmr_lock;
	
	/** lookup alias. */
	if (tmr->cfg  & TMR_OPTIONS_ADVANCED)
		mtx = &tm->ol_tmr_lock;

    oryx_sys_mutex_lock(mtx);
	tmr_enable(tmr);
    oryx_sys_mutex_unlock(mtx);
}

void
oryx_tmr_stop
(
	struct oryx_timer_t *tmr
)
{
	struct oryx_tmr_mgr_t *tm = &tmrmgr;
	sys_mutex_t *mtx = &tm->ol_sigtmr_lock;
	
	/** lookup alias. */
	if (tmr->cfg  & TMR_OPTIONS_ADVANCED)
		mtx = &tm->ol_tmr_lock;

    oryx_sys_mutex_lock(mtx);
	tmr_disable(tmr);
    oryx_sys_mutex_unlock(mtx);
}

void
oryx_tmr_destroy
(
	struct oryx_timer_t *tmr
)
{
	struct oryx_tmr_mgr_t *tm = &tmrmgr;
	sys_mutex_t *mtx = &tm->ol_sigtmr_lock;
	
	/** lookup alias. */
	if (tmr->cfg  & TMR_OPTIONS_ADVANCED)
		mtx = &tm->ol_tmr_lock;

    oryx_sys_mutex_lock(mtx);
	tmr_delete(tmr);
    oryx_sys_mutex_unlock(mtx);
}

int oryx_tmr_initialize(void) 
{
	struct oryx_tmr_mgr_t *tm = &tmrmgr;

	INIT_LIST_HEAD (&tm->sigtmr_head);
	INIT_LIST_HEAD (&tm->tmr_head);
	ATOMIC_INIT(tm->sigtmr_cur_ticks);
	ATOMIC_INIT(tm->tmr_cur_ticks);

	oryx_sys_mutex_create(&tm->ol_sigtmr_lock);
	oryx_sys_mutex_create(&tm->ol_tmr_lock);

#if defined (HAVE_ADVANCED_TMR)
	oryx_task_registry (&advanced_tmr_task);
#else
	realtimer_init();
#endif

	return 0;
}

/**
* routine: must be a reentrant function.
* An unknown error occurs if a thread-safe function called as a routione.
*/
struct oryx_timer_t *
oryx_tmr_create
(
	IN int module,
	IN const char *sc_alias, uint32_t cfg,
	IN void (*handler)(struct oryx_timer_t *, int, char **),
	IN int argc,
	IN char **argv,
    IN uint32_t nr_micro_sec
)
{
    struct oryx_timer_t *_this = NULL, *p;
	struct oryx_tmr_mgr_t *tm = &tmrmgr;
    struct list_head *h;
	sys_mutex_t *mtx;

	BUG_ON (sc_alias == NULL);

	/** lookup alias. */
	if (cfg & TMR_OPTIONS_ADVANCED) {
		h = &tm->tmr_head;
		mtx = &tm->ol_tmr_lock;
	}
	else {
		h = &tm->sigtmr_head;
		mtx = &tm->ol_sigtmr_lock;
	}
	
	/** lookup advanced timer list. */
	oryx_sys_mutex_lock(mtx);
    list_for_each_entry_safe (_this, p, h, list){
        if(likely(!strcmp(sc_alias, _this->sc_alias)) &&
			(module == _this->module)){
            fprintf (stdout,  
                "The same timer (%s, %d)\n", _this->sc_alias, _this->tmr_id);
			oryx_sys_mutex_unlock(mtx);
            goto finish;
        }
    }
	
				
    _this = (struct oryx_timer_t *)tmr_alloc();
	_this->argc			= argc;
    _this->argv			= argv;
    _this->module		= module;
    _this->sc_alias		= sc_alias;
    _this->curr_ticks	= 0;
    _this->interval		= nr_micro_sec;
    _this->routine		= handler ? handler : oryx_tmr_default_handler;
    _this->tmr_id		= tmr_id_alloc(_this->module, _this->sc_alias, strlen(_this->sc_alias));
    _this->cfg = (cfg | TMR_CAN_BE_RECYCLABLE);

    list_add_tail(&_this->list, h);
	oryx_sys_mutex_unlock(mtx);

finish:    
    return _this;
}

struct oryx_timer_t *
oryx_tmr_create_loop
(
	IN int module,
	IN const char *sc_alias,
	IN void (*handler)(struct oryx_timer_t *, int, char **),
	IN int argc,
	IN char **argv,
	IN uint32_t nr_micro_sec)
{
	uint32_t cfg = (TMR_OPTIONS_PERIODIC | TMR_OPTIONS_ADVANCED);
	return oryx_tmr_create(module, sc_alias,
					cfg, handler, argc, argv, nr_micro_sec);
}

