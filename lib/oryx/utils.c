#include "oryx.h"

#define COLOR_TYPES {\
    [COLOR_FIN]		= "\e[0m",\
    [COLOR_BLACK]	= "\e[0;30m",\
    [COLOR_RED]		= "\e[0;31m",\
    [COLOR_GREEN]	= "\e[0;32m",\
    [COLOR_YELLOW]	= "\e[0;33m",\
    [COLOR_BLUE]	= "\e[0;34m",\
    [COLOR_PURPLE]	= "\e[0;35m",\
    [COLOR_CYAN]	= "\e[0;36m",\
    [COLOR_WHITE]	= "\e[0;37m",\
    [COLORS]		= "skip-draw-color"\
}

static uint32_t __os_rand;
static const char* colors[] = COLOR_TYPES;

static const char *___builtin_itoa (int value, char * str, int base)
{	
	BUG_ON (str == NULL);
	
	switch (base) {
		case 8:
			sprintf(str,"%o", value);
			break;
		case 10:
			sprintf(str,"%d", value);
			break;
		case 16:
			sprintf(str,"%x", value);
			break;
		default:
			break;
	}

	return str;
}

__oryx_always_extern__
int oryx_path_exsit
(
	IN const char *path
)
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
	int s = 0;

	ASSERT (path);
	
	s = remove (path);
	if (likely(s)) {
		s = 1;
	}
	
	return s;
}

__oryx_always_extern__
int oryx_mkdir 
(
	IN const char *dir,
	OUT DIR **d
)
{

	char cmd[256] = {0};
	int s = 0;

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
int oryx_mkfile 
(
	IN const char *file,
	IN const char *mode,
	OUT FILE **fp
)
{

	char cmd[256] = {0};
	int s = 0;

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
void oryx_file_clear(const char *f) {
	char emptycmd[128] = "cat /dev/null > ";

	strcat(emptycmd, f);
	do_system(emptycmd);
}

/** A random Pattern generator.*/
__oryx_always_extern__
int oryx_pattern_generate 
(
	IN char *pattern,
	IN size_t l
)
{
	int pl = 0, j;
	uint32_t rand = __os_rand;
	
	memset (pattern, 0, l);

	do {
		oryx_next_rand(&rand);
		pl = rand % l;
	} while (pl < 4 /** Smallest Pattern */ || 
			pl > (int)(l - 1)  /** Largest Pattern */);

	for (j = 0; j < (int)pl; j ++) {
		char c;
		do {
			oryx_next_rand(&rand);
			c = rand % 255;
		} while (!isalpha(c));
		
		pattern[j] = c;
	}

	pattern[j] = '\0';
	
	return pl;
}

/** A random IPv4 address generator.*/
__oryx_always_extern__
void oryx_ipaddr_generate
(
	IN char *ipv4
)
{
	int		a = 0,
			b = 0,
			c = 0,
			d = 0;
	uint32_t rand = __os_rand;
	char	aa[4],
			bb[4],
			cc[4],
			dd[4],
			maskp[4];
	
	uint8_t mask;
	
	a = oryx_next_rand (&rand) % 256;
	b = oryx_next_rand (&rand) % 256;
	c = oryx_next_rand (&rand) % 256;
	d = oryx_next_rand (&rand) % 256;
	mask = oryx_next_rand (&rand) % 32;
	
	___builtin_itoa (a, aa, 10);
	___builtin_itoa (b, bb, 10);
	___builtin_itoa (c, cc, 10);
	___builtin_itoa (d, dd, 10);
	___builtin_itoa (mask, maskp, 10);

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
void oryx_l4_port_generate 
(
	IN char *sp,
	IN char *dp
)
{
	uint32_t rand = __os_rand;
	uint16_t psrc, pdst;
	char	aa[5],
			bb[5];
	
	psrc = oryx_next_rand (&rand) % 65535;
	pdst = oryx_next_rand (&rand) % 65535;
	
	___builtin_itoa (psrc, aa, 10);
	___builtin_itoa (pdst, bb, 10);

	strcpy (sp, aa);
	strcpy (dp, bb);
}

__oryx_always_extern__
char * oryx_fmt_program_counter (uint64_t fmt_val, char *fmt_buffer, int fixed_width , int no_scale)
{
	uint64_t _1KB = 1LL << 10;
	uint64_t _1MB = 1LL << 20;
	uint64_t _1GB = 1LL << 30;
	uint64_t _1TB = 1LL << 40;
	
	char *str		= fmt_buffer;
	double f		= fmt_val;

	BUG_ON (fmt_buffer == NULL);

	if (no_scale) {
		snprintf(str, 64, "%llu ", (long long unsigned)fmt_val);
		return str;
	}

	if (fmt_val >= _1TB) {
		if (fixed_width) 
			snprintf(str, 31, "%5.2f T", f /(double) _1TB );
		else 
			snprintf(str, 31, "%.2f T", f / (double) _1TB );
	} else if (fmt_val >= _1GB) {
		if (fixed_width) 
			snprintf(str, 31, "%5.2f G", f / (double) _1GB );
		else 
			snprintf(str, 31, "%.2f G", f / (double) _1GB );
	} else if (fmt_val >= _1MB) {
		if (fixed_width) 
			snprintf(str, 31, "%5.2f M", f / (double) _1MB );
		else 
			snprintf(str, 31, "%.2f M", f / (double) _1MB );
	} else if (fmt_val >= _1KB) {
		if (fixed_width) 
			snprintf(str, 31, "%5.2f K", f / (double) _1KB );
		else 
			snprintf(str, 31, "%.2f K", f / (double) _1KB );
	} else {
		if (fixed_width) 
			snprintf(str, 31, "%4.0f ", f );
		else 
			snprintf(str, 31, "%.0f ", f );
	} 

	return str;
}

__oryx_always_extern__
void oryx_register_sighandler
(
	IN int signal,
	IN void (*handler)(IN int)
)
{
	sigset_t block_mask;
	struct sigaction saction;

	sigfillset(&block_mask);

	saction.sa_handler = handler;
	saction.sa_mask = block_mask;
	saction.sa_flags = SA_RESTART;

	sigaction(signal, &saction, NULL);
}

__oryx_always_extern__
uint64_t oryx_elapsed_us
(
	IN struct  timeval *start,
	IN struct  timeval *end
)
{
	return (1000000 * (end->tv_sec - start->tv_sec) + end->tv_usec - start->tv_usec);
}

__oryx_always_extern__
int oryx_get_prgname
(
	IN pid_t pid,
	OUT char *prgname
)
{
	 char pidpath[1024];
	 char buf[1024];

	 sprintf(pidpath, "/proc/%d/status", pid);
	 FILE* fp = fopen(pidpath, "r");
	 if(fp == NULL) {
	 	fprintf(stdout, "fopen: %s\n", oryx_safe_strerror(errno));
		return -EINVAL;
	 } else {
	     if (fgets(buf, 1024 - 1, fp) != NULL)
		     sscanf(buf, "%*s %s", prgname);
	     fclose(fp);
		 return 0;
	 }
}

__oryx_always_extern__
void oryx_fmt_time 
(
	IN uint64_t ts, 
	IN const char *fmt,
	OUT char *date,
	IN size_t len
)
{
	BUG_ON (date == NULL);
	BUG_ON (fmt == NULL);

	memset(date, 0, len);

	/** Convert ts to localtime with local time-zone */
    const struct tm *tm = localtime((time_t *)&ts);
    strftime(date, len - 1, fmt, tm);
}

__oryx_always_extern__
int do_system
(
	IN const char *cmd
)
{
    if (likely(cmd)) {
	    return system(cmd);
    }

	return -1;
}

__oryx_always_extern__
bool is_number
(
	IN const char *s,
	IN size_t l
)
{
	char *p;

	if (!l) return false;
	
	for (p = (char *)&s[0];
				*p != '\0' && *p != '\n'; ++ p) {
		if(*p != '+' && *p != '-' 
					&& (*p < '0' || *p > '9')
					&& (*p < 'A' || *p > 'F')
					&& (*p < 'a' || *p > 'f'))
			return false;
	}
	return true;
}


__oryx_always_extern__
bool is_numerical 
(
	IN const char* s,
	IN size_t __oryx_unused__ l
)
{
	bool	in		= false,	/* have integer */
			fr		= false,	/* have fraction */
			ein		= false,	/* left of e */
			eout 	= false;	/* right of e */

	/* 1. skip space */
	while(isspace(*s))
		s ++;

	/* 2. after space */
	if((!isdigit(*s)) && (*s != 'e') && (*s != '.') && (*s != '+') && (*s != '-'))
		return false;
	
	/* 3. + - */
	if((*s == '+') || (*s == '-'))
		s++;
	
	/* 4. integer part */
	if(isdigit(*s)) {
		in = true;
		ein = true;
	}
	
	while(isdigit(*s))
		s++;
	
	/* 5. '.' and 'e' */
	if((*s) == '.') {
		s++;
		if(isdigit(*s))
			fr = true;
		if((!fr) && (!in))
			return false;
		while(isdigit(*s))
			s++;
		ein = true;
	}
	
	if(*s == 'e') {
		s++;
		
		if((*s == '-') || (*s == '+'))
			s++;
		if(isdigit(*s))
			eout = true;
		if((!ein) || (!eout))
			return false;
		while(isdigit(*s))
			s++;
	}
  
	/* 6. ending space */
	while(isspace(*s))
		s++;
		
	return (*s == '\0') ? true : false;
	
}

__oryx_always_extern__
int kvpair
(
	IN char *str,
	OUT char **k,
	OUT char **v
)
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
const char *draw_color
(
	IN color_t color
)
{
	return colors[color % COLORS];
}

__oryx_always_extern__
uint32_t oryx_next_rand 
(
	uint32_t *p
)
{
	uint32_t seed = *p;

	seed = 1103515145 * seed + 12345;
	*p = seed;
	__os_rand = *p;
	return seed;
}

/**
 *  \brief wrapper for OS/compiler specific atomic compare and swap (CAS)
 *         function.
 *
 *  \param @string A range in string mode
 *  \param @lim Limit of the maximum number
 *  \param @base 
 *	\param @ul_start The number before @lim
 *	\param @ul_end The number after @lim
 *
 *  \retval 0 format successed
 *  \retval -EINVAL format failed
 *
 *	\example: oryx_formatted_range("1:100", UINT16_MAX, 0, ':', &val_start, &val_end)
 *   val_start will be number 1 and val_end will be number 100.
 */

__oryx_always_extern__
int oryx_formatted_range
(
	IN const char *str,
	IN uint32_t lim,
	IN int base,
	IN char dlm,
	OUT uint32_t *ul_start,
	OUT uint32_t *ul_end
)
{
	uint32_t val_start, val_end;
	char *end;
	char *next;

	BUG_ON(ul_start == NULL || ul_end == NULL);
	(*ul_start) = (*ul_end) = 0;
	
	val_start = strtoul((const char *)str, &end, base);
	if (errno == ERANGE || end[0] != dlm || val_start > lim){
		oryx_logn("%d %c %d lim %u (%s)", errno, end[0], val_start, lim, str);
		return -EINVAL;
	}

	next = end + 1; /** skip the dlm */
	
	val_end = strtoul((const char *)next, &end, base);
	if (errno == ERANGE || val_end > lim){
		oryx_logn("%d %c %d lim %u (%s)", errno, end[0], val_end, lim, next);
		return -EINVAL;
	}

	*ul_start = val_start;
	*ul_end = val_end;

	return 0;
}


__oryx_always_extern__
int oryx_foreach_directory_file 
(
		IN char *dir_name,
		IN int (*f) (IN void *arg, IN char * path_name,
			IN char * file_name), 
		IN void *arg,
		IN int scan_dirs
)
{
	DIR *d;
	struct dirent *e;
	int error = 0;
	char s[1024], t[1024];

  d = opendir (dir_name);
  if (!d)
	{
	  if (errno == ENOENT)
			return 0;
	  else {
	  	oryx_logn ("open %s, %d", dir_name, errno);
		return 0;
	  }
	}

  while (1)
	{
	  e = readdir (d);
	  if (!e)
	break;
	  if (scan_dirs)
	{
	  if (e->d_type == DT_DIR
		  && (!strcmp (e->d_name, ".") || !strcmp (e->d_name, "..")))
		continue;
	}
	  else
	{
	  if (e->d_type == DT_DIR)
		continue;
	}

	  sprintf (s, "%s/%s", dir_name, e->d_name);
	  sprintf (t, "%s", e->d_name);
	
	  error = f (arg, s, t);
	  if (error)
		break;
	}

  closedir (d);

  return error;
}

#ifndef HAVE_STRLCPY
/**
 * Like strncpy but does not 0 fill the buffer and always null 
 * terminates.
 *
 * @param l is the size of the destination buffer.
 *
 * @return index of the terminating byte.
 **/
size_t
strlcpy(char *d, const char *s, size_t l)
{
	size_t len = strlen(s);
	size_t ret = len;
	if (l > 0) {
		if (len >= l)
			len = l -1;
		memcpy(d, s, len);
		d[len] = 0;
	}
	return ret;
}
#endif
			
#ifndef HAVE_STRLCAT
/**
 * Like strncat() but does not 0 fill the buffer and always null 
 * terminates.
 *
 * @param l length of the buffer, which should be one more than
 * the maximum resulting string length.
 **/
size_t
strlcat(char *d, const char *s, size_t l)
{
	size_t len1 = strlen(d);
	size_t len2 = strlen(s);
	size_t ret = len1 + len2;

	if (len1 < l - 1) {
		if (len2 >= l - len1)
			len2 = l - len1 - 1;
		memcpy(d+len1, s, len2);
		d[len1+len2] = 0;
	}
	return ret;
}
#endif


/**
 * \internal
 * \brief Does a memcpy of the input string to lowercase.
 *
 * \param d   Pointer to the target area for memcpy.
 * \param s   Pointer to the src string for memcpy.
 * \param len len of the string sent in s.
 */
__oryx_always_extern__
void memcpy_tolower
(
	OUT uint8_t *d,
	IN uint8_t *s,
	IN uint16_t len
)
{
    uint16_t i;
	
    for (i = 0; i < len; i++)
        d[i] = u8_tolower(s[i]);

    return;
}

