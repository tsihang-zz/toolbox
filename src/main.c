
#include "oryx.h"

vlib_main_t vlib_main;

static void
format_dot_file(char*f, char *sep, void *fp)
{	
	if (strstr(f, sep)) {
		fprintf((oryx_file_t *)fp, "   %s\n", f);
	}
}

static void
format_dota_file(char*f, char *sep, void *fp)
{
	char *s, *d;
	char fs[32];
	
	s = f;
	d = fs;
	
	if (strstr(f, sep)) {
		s += 3;
		do {
            if ((*d++ = *s++) == 0)
                break;
        } while (*s != '.');
			
		*d = '\0';
		fprintf((oryx_file_t *)fp, "    %s", fs);
	}
}

static void 
format_files(char *path, void(*format)(char*f, char *s, void *fp), char *sep)
{
	oryx_dir_t *d;
	struct dirent *p;
	oryx_file_t *fp;

	fp = fopen ("./file", "a");

	d = opendir (path);
	if (d) {
		while(likely((p = readdir(d)) != NULL)) {
			/** skip . and .. */
			if(!strcmp(p->d_name, ".") || !strcmp(p->d_name, ".."))
				continue;
			format(p->d_name, sep, fp);
		}
	}
}

int main (int argc, char **argv)
{
	vlib_main.argc = argc;
	vlib_main.argv = argv;
	
	format_files ("/home/tsihang/Source/et1500/lib", 
			format_dot_file, ".c");

	dpdk_init(&vlib_main);

	return 0;
}
