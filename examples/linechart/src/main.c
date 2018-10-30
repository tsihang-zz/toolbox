#include "oryx.h"

#define VLIB_BUFSIZE	1024

int running = 1;

static void sigint_handler(int sig) {
	fprintf (stdout, "signal %d ...\n", sig);
	running = 0;
}

static uint64_t count = 10;
int main(
	int 	__oryx_unused_param__	argc,
	char	__oryx_unused_param__	** argv
)
{
	char	value[VLIB_BUFSIZE];
	size_t	valen;
	FILE	*fp;
	
	oryx_initialize();
	oryx_register_sighandler(SIGINT,	sigint_handler);
	oryx_register_sighandler(SIGTERM,	sigint_handler);


	fp = fopen("classify_result.log", "r");
	if (!fp) {
		fprintf(stdout, "fopen: %s\n", oryx_safe_strerror(errno));
		return 0;
	}
	
	/* A loop read the part of lines. */
	FOREVER {		
		memset (value, 0, VLIB_BUFSIZE);
		if(!fgets (value, VLIB_BUFSIZE, fp) || !running)
			break;

		valen = strlen(value);
		fprintf(stdout, "%s\n", value);
	}
	
	fclose(fp);

	
	return 0;
}

