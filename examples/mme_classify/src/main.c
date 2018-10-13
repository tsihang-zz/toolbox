#include "oryx.h"
#include "file.h"
#include "tg.h"
#include "mme.h"
#include "main.h"
#include "classify.h"


#define MME_CSV_FILE \
	"/home/tsihang/vbx_share/class/DataExport.s1mmeSAMPLEMME_1538102100.csv"
#define ENQUEUE_LCORE_ID 0

int running = 1;

vlib_main_t vlib_main = {
	.argc		=	0,
	.argv		=	NULL,
	.prgname	=	"mme_classify",
	.nr_mmes	=	0,
	.mme_htable	=	NULL,
};

static void lq_sigint(int sig)
{
	fprintf (stdout, "signal %d ...\n", sig);
	running = 0;
}

static
void * dequeue_handler (void __oryx_unused_param__ *r)
{
		struct lq_element_t *lqe;
		vlib_threadvar_ctx_t *vtc = (vlib_threadvar_ctx_t *)r;
		struct oryx_lq_ctx_t *lq = vtc->lq;
		vlib_main_t *vm = &vlib_main;
		
		/* wait for enqueue handler start. */
		while (vm->ul_flags & VLIB_ENQUEUE_HANDLER_STARTED)
			break;

		fprintf(stdout, "Starting dequeue handler(%lu): vtc=%p, lq=%p\n", pthread_self(), vtc, lq);		
		FOREVER {
			lqe = NULL;
			
			if (!running) {
				/* wait for enqueue handler exit. */
				while (vm->ul_flags & VLIB_ENQUEUE_HANDLER_EXITED)
					break;
				fprintf (stdout, "Thread(%lu): dequeue exiting ... ", pthread_self());
				/* drunk out all elements before thread quit */					
				while (NULL != (lqe = oryx_lq_dequeue(lq))){
					do_classify(vtc, lqe);
				}
				fprintf (stdout, " exited!\n");
				break;
			}
			
			lqe = oryx_lq_dequeue(lq);
			if (lqe != NULL) {
				do_classify(vtc, lqe);
			}
		}

		oryx_task_deregistry_id(pthread_self());
		return NULL;
}

struct oryx_task_t dequeue = {
		.module 		= THIS,
		.sc_alias		= "Dequeue Task0",
		.fn_handler 	= dequeue_handler,
		.lcore_mask		= 0x08,
		.ul_prio		= KERNEL_SCHED,
		.argc			= 1,
		.argv			= NULL,
		.ul_flags		= 0,	/** Can not be recyclable. */
};

static __oryx_always_inline__
void update_event_start_time(char *value, size_t valen)
{
	char *p;
	int sep_refcnt = 0;
	const char sep = ',';
	size_t step;
	bool event_start_time_cpy = 0, find_imsi = 0;
	char *ps = NULL, *pe = NULL;
	char ntime[32] = {0};
	struct timeval t;
	uint64_t time;

	gettimeofday(&t, NULL);
	time = (t.tv_sec * 1000 + (t.tv_usec / 1000));
	
	for (p = (const char *)&value[0], step = 0, sep_refcnt = 0;
				*p != '\0' && *p != '\n'; ++ p, step ++) {			

		/* skip entries without IMSI */
		if (!find_imsi && sep_refcnt == 5) {
			if (*p == sep) 
				continue;
			else
				find_imsi = 1;
		}

		/* skip first 2 seps and copy event_start_time */
		if (sep_refcnt == 2) {
			event_start_time_cpy = 1;
		}

		/* valid entry dispatch ASAP */
		//if (sep_refcnt == 45 && find_imsi)
		if (sep_refcnt == 45)
			goto update_time;

		if (*p == sep)
			sep_refcnt ++;

		/* stop copy */
		if (sep_refcnt == 3) {
			event_start_time_cpy = 0;
		}

		/* soft copy.
		 * Time stamp is 13 bytes-long */
		if (event_start_time_cpy) {
			if (ps == NULL)
				ps = p;
			pe = p;
		}

	}

	/* Wrong CDR entry, go back to caller ASAP. */
	//fprintf (stdout, "not update time\n");
	//return;

update_time:

	sprintf (ntime, "%lu", time);
	strncpy (ps, ntime, (pe - ps + 1));
	//fprintf (stdout, "(len %lu, %lu), %s", (pe - ps + 1), time, value);
	return;
}

static
void * enqueue_handler (void __oryx_unused_param__ *r)
{
		vlib_main_t *vm = &vlib_main;

		static FILE *fp = NULL;
		const char *file = MME_CSV_FILE;
		static char line[LINE_LENGTH] = {0};
		int nr_lines = 0;
		size_t llen = 0;
		
		if (!fp) {
			fp = fopen(file, "r");
			if(!fp) {
        		fprintf (stdout, "Cannot open %s \n", file);
        		exit(0);
        	}
		}
		
		vm->ul_flags |= VLIB_ENQUEUE_HANDLER_STARTED;
		FOREVER {
			if (!running) {
				fprintf (stdout, "Thread(%lu): enqueue exited!\n", pthread_self());
				vm->ul_flags |= VLIB_ENQUEUE_HANDLER_EXITED;
				break;
			}
			
			while (fgets (line, LINE_LENGTH, fp)) {
				llen = strlen(line);
				nr_lines ++;
				/* skip first line. */
				if (nr_lines == 1)
					continue;
				/* update event start time to make sure that
				 * classify PRGRM. */
				update_event_start_time(line, llen);
				do_dispatch(line, llen);
				memset (line, 0, LINE_LENGTH);
				if(nr_lines % 50000 == 0)
					usleep(100000);
			}
			/* break after end of file. */
			fprintf (stdout, "Finish read %s, %d line(s), break down!\n", file, nr_lines);
			break;
		}

		oryx_task_deregistry_id(pthread_self());
		return NULL;
}

struct oryx_task_t enqueue = {
		.module 		= THIS,
		.sc_alias		= "Enqueue Task",
		.fn_handler 	= enqueue_handler,
		.lcore_mask		= (1 << ENQUEUE_LCORE_ID),
		.ul_prio		= KERNEL_SCHED,
		.argc			= 0,
		.argv			= NULL,
		.ul_flags		= 0,	/** Can not be recyclable. */
};

int main (
        int     __oryx_unused_param__   argc,
        char    __oryx_unused_param__   ** argv
)

{
	vlib_main_t *vm = &vlib_main;
	oryx_initialize();
	
	vm->argc = argc;
	vm->argv = argv;
	vm->nr_threads = 1;
	vm->dictionary = "src/cfg/dictionary";
	vm->classdir = "/home/tsihang/vbx_share/class/formated";
	vm->savdir = "/home/tsihang/vbx_share/class/sav";
	vm->max_entries_per_file = 80000;
	vm->threshold = 5;
	vm->dispatch_mode = HASH;

	oryx_register_sighandler(SIGINT, lq_sigint);
	oryx_register_sighandler(SIGTERM, lq_sigint);

	classify_env_init(vm);
	oryx_task_registry(&enqueue);
	oryx_task_launch();
	FOREVER {
		sleep (1);
		if (!running) {
			/* wait for handlers of enqueue and dequeue finish. */
			sleep (3);
			classify_runtime();
			break;
		}
	};
		
	classify_terminal();
	return 0;
}
