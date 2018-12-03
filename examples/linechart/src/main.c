#include "oryx.h"

#define VLIB_BUFSIZE	1024
int running = 1;

typedef struct vlib_entry_t {
	char		name[1024];
	time_t		tv_usec;
	uint64_t	entries;
	float		speed;
}vlib_entry_t;

static void sigint_handler(int sig) {
	fprintf (stdout, "signal %d ...\n", sig);
	running = 0;
}

static __oryx_always_inline__
void parse_name(const char *value, char *name)
{
	char	*p;
	size_t	n;

	for (p = strstr(value, "DataExport"), n = 0;
			*p != '\0'; ++ p) {
		if (*p == ' ')
			break;
		name[n ++] = *p;
	}
}

static __oryx_always_inline__
void parse_entries(const char *cur_pos, uint64_t *entries)
{
	char	*p;
	size_t	n;
	char	integer[32] = {0};

	for (p = cur_pos + 1, n = 0;
			*p != '\0'; ++ p) {
		if (*p == ' ')
			break;
		integer[n ++] = *p;
	}

	if (integer[0] != '\0') {
		char *checkptr;
		(*entries) = strtol(integer, &checkptr, 10);	
	}
}


static __oryx_always_inline__
void parse_cost(const char *cur_pos, uint64_t *tv_usec)
{
	char	*p;
	size_t	n;
	char	integer[32] = {0};

	for (p = cur_pos + 7, n = 0;
			*p != '\0'; ++ p) {
		if (*p == ' ')
			break;
		integer[n ++] = *p;
	}

	if (integer[0] != '\0') {
		char *checkptr;
		(*tv_usec) = strtol(integer, &checkptr, 10);
	}
}

static __oryx_always_inline__
void parser(const char *value, size_t valen, vlib_entry_t *entry)
{
	char	*p,
			clons = ':',
			comma = ',',
			parenthesis = '(',
			pps_str0[20] = {0};
	int		nr_clons,
			nr_comma;

	for (p = &value[0]; *p != '\0'; ++ p) {
		if (*p == comma)
			nr_comma ++;
		if (*p == clons)
			nr_clons ++;

		/* file name */
		if (nr_clons == 3 && (entry->name[0] == '\0')) {
			parse_name(p, (char *)&entry->name[0]);
		}
		
		/* number of entries */
		if (*p == parenthesis && entry->entries == 0) {
			parse_entries(p, &entry->entries);
		}
		
		/* cost tv_usec */
		if (nr_comma == 2 && entry->tv_usec == 0) {
			parse_cost(p, &entry->tv_usec);
		}
	}

	fprintf(stdout, "%s, %lu entries, cost %lu usec, speed %s\n",
		entry->name, entry->entries, entry->tv_usec,
		oryx_fmt_program_counter(fmt_pps(entry->tv_usec, entry->entries), pps_str0, 0, 0));

}

int main(
	int 	__oryx_unused__	argc,
	char	__oryx_unused__	** argv
)
{
	char	value[VLIB_BUFSIZE];
	size_t	valen;
	FILE	*fp,
			*fpw;
	vlib_entry_t entry;
	
	oryx_initialize();
	oryx_register_sighandler(SIGINT,	sigint_handler);
	oryx_register_sighandler(SIGTERM,	sigint_handler);


	fp = fopen("classify_result.log", "r");
	if (!fp) {
		fprintf(stdout, "fopen: %s\n", oryx_safe_strerror(errno));
		return 0;
	}

	if(oryx_path_exsit("classify_result.csv")) {
		unlink("classify_result.csv");
	}
	fpw = fopen("classify_result.csv", "a+");
	if (!fpw) {
		fprintf(stdout, "fopen: %s\n", oryx_safe_strerror(errno));
		return 0;
	}
	
	/* A loop read the part of lines. */
	FOREVER {
		memset (value, 0, VLIB_BUFSIZE);
		memset (&entry, 0, sizeof(vlib_entry_t));
		
		if(!fgets (value, VLIB_BUFSIZE, fp) || !running)
			break;

		valen = strlen(value);
		parser(value, valen, &entry);
		fprintf(fpw, "%lu,%lu\n", entry.entries, entry.tv_usec);
	}
	
	fclose(fp);
	fclose(fpw);

	
	return 0;
}

