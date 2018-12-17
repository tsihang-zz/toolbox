#include "oryx.h"

#if defined(HAVE_OPTION)
struct option {
	const char *name;
	int 		has_arg;
	int 	   *flag;
	int 		val;
};
#endif

static char *progname;
const char macprefix[6] = {
	0x02,
	0x0F,
	0xB7,
	0,
	0,
	0
};

typedef uint32_t __bt;
typedef uint32_t __sn;

/**
 * Prints out usage information to stdout
 */
static void
usage(void)
{
	fprintf (stdout, 
	    "%s [-f file] [-S SN.] [-B BT.]\n"
	    " -f file: Read set of entries from a file\n"
	    " -S Serial Number\n"
	    " -B Board Type\n"
	    , progname);
}

static const char * safe_strerror (int err)
{
	const char *s = strerror(err);
	return (s != NULL) ? s : "Unknown error";
}

static uint16_t bt2i (const char *bt)
{
	size_t	i,
			l;
	uint16_t v;

	for (i = 0, l = strlen(bt); i < l; i ++)
		v += bt[i];

	return v;
}

/**
 *  \brief Generate MAC address by given board type and serial number.
 *
 *  \param bt Board type of SF1500
 *  \param sn Serial number of SF1500
 *
 *  \retval 0  succeeded
 *  \retval -1 failure
 */
static inline
int macgen(const __bt bt, const __sn sn)
{
	int i;
	char mac[6] = {0};

	mac[0] = macprefix[0];
	mac[1] = macprefix[1];
	mac[2] = macprefix[2];

}

static inline char *printTimeSCTS(const char *caller, uint64_t u_Time, char* time_str)
{
	char *p = (char *)&u_Time;

	if (u_Time == 0) {
		sprintf(time_str, "1970-01-01 00:00:00");
	} else {
		sprintf(time_str, "%02x-%02x-%02x %02x:%02x:%02x (%lu, %08x)",
			p[7], p[6], p[5], p[4], p[3], p[2], u_Time, u_Time);
	}
	return time_str;
}


int main (
	int 	argc,
	char	** argv
)
{
	int opt;
	char buf[1024] = {0};
	uint64_t val = 1514924800027537920;
	printTimeSCTS("xxx", val, buf);
	fprintf(stdout, "%s\n", buf);
	
	char *f = NULL;
	FILE *fp;
	
	__bt bt = 0;
	__sn sn = 0;
	
	while ((opt = getopt(argc, argv, "B:S:f:")) != -1) {
		switch (opt) {
		case 'B':
			fprintf(stdout, "Board Type '%s'\n", optarg);
			bt = atoi(optarg);
			break;
		case 'S':
			fprintf(stdout, "Serial Number '%s'\n", optarg);
			sn = atoi(optarg);
			break;
		case 'f':
			f = optarg;
			fprintf(stdout, "File '%s'\n", f);
			break;
		default: /* '?' */
			usage();
			exit(EXIT_FAILURE);
		}
	}

	/* Other code omitted */


	if (f) {	
		/* File exist. */
		if (!access (f, F_OK)) {
			/* Try to open this file with RDONLY mode. */
			fp = fopen(f, "r");
			if (!fp) {
				fprintf (stdout, "fopen: %s\n", safe_strerror(errno));
				exit(0);
			}
		}
	} else {
		/* Only caculate by SN & BT */
		macgen(bt, sn);
	}

	return 0;
}


