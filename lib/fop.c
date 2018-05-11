#include "oryx.h"

static u32 __os_rand;
static char* colors[] = COLOR_TYPES;

__oryx_always_extern__
int oryx_path_exsit (const char *path)
{
	int exsit = 0;

	ASSERT (path);
	
	if (unlikely(!access (path, F_OK))) {
		exsit = 1;
	}

	return exsit;
}

__oryx_always_extern__
int oryx_path_remove (const char *path)
{
	oryx_status_t s = 0;

	ASSERT (path);
	
	s = remove (path);
	if (likely(s)) {
		s = 1;
	}
	
	return s;
}

__oryx_always_extern__
int oryx_mkdir (const char *dir, oryx_dir_t **d)
{

	char cmd[256] = {0};
	oryx_status_t s = 0;

	ASSERT (dir);
	
	(*d) = NULL;
	
	snprintf (cmd, 255, "mkdir -p %s", dir);
	do_system(cmd);

	snprintf (cmd, 255,  "chmod 775 %s", dir);
    do_system(cmd);

	(*d)  = opendir (dir);

	s = (dir && (*d)) ? 0 : 1;

	return s;
}

__oryx_always_extern__
int oryx_mkfile (const char *file, oryx_file_t **fp, char *mode)
{

	char cmd[256] = {0};
	oryx_status_t s = 0;

	ASSERT (file);
	
	(*fp) = NULL;

	if (oryx_path_exsit (file)) {
		(*fp)  = fopen (file, mode);
	} else {
		snprintf (cmd, 255, "touch %s", file);
		do_system(cmd);

		snprintf (cmd, 255,  "chmod 775 %s", file);
	    	do_system(cmd);

		(*fp)  = fopen (file, mode);
	}

	s = (file && (*fp)) ? 0 : 1;

	return s;
}

__oryx_always_extern__
int oryx_file_close (oryx_file_t *fp)
{

	oryx_status_t s = 0;

	ASSERT (fp);
	
	if (unlikely(!fclose (fp))) {
		s = 1;
	}
	
	return s;
}

__oryx_always_extern__
int oryx_file_read_write (oryx_file_t *fp, 
			struct oryx_file_rw_context_t *frw_ctx)
{

	oryx_size_t s = 0;

	ASSERT (frw_ctx);
	ASSERT (fp);

	if (frw_ctx->cmd & RW_MODE_READ) {
		if (frw_ctx->cmd & RW_MODE_READ_LINE) {
			char *v = fgets (frw_ctx->sc_buffer, frw_ctx->ul_size, fp);
			if (v) s = strlen (frw_ctx->sc_buffer);
		} else {
			/** Read blocks. */
			s = fread (frw_ctx->sc_buffer, sizeof (char), frw_ctx->ul_size, fp);
		}
		
	} else {
		if (fputs (frw_ctx->sc_buffer, fp) > 0) {
			s = strlen (frw_ctx->sc_buffer);
		}
	}

	frw_ctx->ul_rw_size = (s > 0) ? s : 0;

	return (s > 0) ? s : 0;
}

__oryx_always_extern__
void oryx_system_preview(void)
{
	struct passwd *pw;
	struct utsname uts;

	if (uname(&uts) < 0){
		fprintf(stderr, "Unable to get the host information.\n");
		return ;
	}

	printf("\r\nSystem Preview\n");
	printf("%30s:%60s\n", "Login User", getlogin());	
	printf("%30s:%60s\n", "Runtime User", (pw = getpwuid(getuid())) ? 
		pw ->pw_name : "Unknown");
	printf("%30s:%60s\n", "Host", uts.nodename);
	printf("%30s:%60s\n", "Arch", uts.machine);
	printf("%30s:%60d\n", "Bits/LONG", __BITS_PER_LONG);
	printf("%30s:%60s\n", "Platform", uts.sysname);
	printf("%30s:%60s\n", "Kernel", uts.release);
	printf("%30s:%60s\n", "OS", uts.version);

	printf("\r\n\n");
 
}

/** A random Pattern generator.*/
__oryx_always_extern__
int oryx_pattern_generate (char *pattern, size_t l)
{
	int pl = 0, j;
	u32 rand = __os_rand;
	
	memset (pattern, 0, l);

	do {
		next_rand_(&rand);
		pl = rand % l;
	} while (pl < 4 /** Smallest Pattern */ || 
			pl > (int)(l - 1)  /** Largest Pattern */);

	for (j = 0; j < (int)pl; j ++) {
		char c;
		do {
			next_rand_(&rand);
			c = rand % 255;
		} while (!isalpha(c));
		
		pattern[j] = c;
	}

	pattern[j] = '\n';
	
	return pl;
}

/** A random IPv4 address generator.*/
__oryx_always_extern__
void oryx_ipaddr_generate (char *ipv4)
{
#define itoa(a,s,t)\
	sprintf (s, "%d", a);
	u32 rand = __os_rand;
	int a = 0, b = 0, c = 0, d = 0;
	char aa[4], bb[4], cc[4], dd[4];
	u8 mask;
	char maskp[4];
	
	a = next_rand_ (&rand) % 256;
	b = next_rand_ (&rand) % 256;
	c = next_rand_ (&rand) % 256;
	d = next_rand_ (&rand) % 256;
	mask = next_rand_ (&rand) % 32;
	
	itoa (a, aa, 10);
	itoa (b, bb, 10);
	itoa (c, cc, 10);
	itoa (d, dd, 10);
	itoa (mask, maskp, 10);

	strcpy (ipv4, aa);
	strcat (ipv4, ".");
	strcat (ipv4, bb);
	strcat (ipv4, ".");
	strcat (ipv4, cc);
	strcat (ipv4, ".");
	strcat (ipv4, dd);
	strcat (ipv4, "/");
	strcat (ipv4, maskp);
	
}

__oryx_always_extern__
int isalldigit(const char *str)
{
    int i;
	
    if (!str) return 0;

	for (i = 0; i < (int)strlen(str); i++) {
        if (!isdigit(str[i]))
            return 0;
    }
	
    return 1;
}

__oryx_always_extern__
u64 tm_elapsed_us (struct  timeval *start, struct  timeval *end)
{
	return (1000000 * (end->tv_sec - start->tv_sec) + end->tv_usec - start->tv_usec);
}

__oryx_always_extern__
int tm_format(uint64_t ts, const char *tm_form, char *date, size_t len)
{
    struct tm *tm = NULL;

    assert(date);
    /** Convert ts to localtime with local time-zone */
    tm = localtime((time_t *)&ts);
    return (int)strftime(date, len - 1, tm_form, tm);
}

__oryx_always_extern__
void do_system(const char *cmd)
{
    if (likely(cmd)) {
	    __oryx_noreturn__(system(cmd));
    }
}

__oryx_always_extern__
int is_numerical (char* s)
{
	//in : have integer <==>fr : have fraction
	bool in = false, fr = false;
	//ein : left of e <==> eout : right of e
	bool ein = false, eout = false;
	//1. space
	while(isspace(*s))
	{
		s++;
	}
	//2. after space
	if((!isdigit(*s)) && (*s != 'e') && (*s != '.') && (*s != '+') && (*s != '-'))
	{
		return false;
	}
	//3. + -
	if((*s == '+') || (*s == '-'))
	{
		s++;
	}
	//4. integer part
	if(isdigit(*s))
	{
		in = true;
		ein = true;
	}
	while(isdigit(*s))
	{
		s++;
	}
	//5. '.' and 'e'
	if((*s) == '.')
	{
		s++;
		if(isdigit(*s))
		{
			fr = true;
		}
		if((!fr) && (!in))
		{
			return false;
		}
		while(isdigit(*s))
		{
			s++;
		}
		ein = true;
	}
	if(*s == 'e')
	{
		s++;
		if((*s == '-') || (*s == '+'))
		{
			s++;
		}
		if(isdigit(*s))
		{
			eout = true;
		}
		if((!ein) || (!eout))
		{
			return false;
		}
		while(isdigit(*s))
		{
			s++;
		}
	}
  
	//6. ending space
	while(isspace(*s))
	{
		s++;
	}
	return (*s == '\0') ? true : false;
	
}

__oryx_always_extern__
int kvpair(char *str, char **k, char **v)
{
    char seps[]   = " \t\r\n=:";
    *k = strtok(str, seps);
    if (*k != NULL)
    {
        *v = strtok(NULL, seps);
        if (*v != NULL)
        {
            return  0;
        }
    }
    return -1;
}

__oryx_always_extern__
char *draw_color(color_t color)
{
	return colors[color % COLORS];
}


