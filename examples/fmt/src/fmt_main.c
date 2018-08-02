#include "oryx.h"

#if 0
#define max_dotfiles_per_line 5
char *dotfile_buff[max_dotfiles_per_line] = {NULL};
int dotfiles = 0;

void
format_dot_file(char *f, char *sep, void *fp)
{	
	int i;
	char format_buf [1024] = {0};
	int s = 0;
	
	if (strstr(f, sep)) {
		i = dotfiles ++;
		if (i == max_dotfiles_per_line) goto flush;
		dotfile_buff[i] = kmalloc(64, MPF_CLR, __oryx_unused_val__);
		sprintf(dotfile_buff[i], "%s", f);
		return;
	}
	else return;

flush:
    {
		for (i = 0; i < dotfiles; i ++) {
			if(dotfile_buff[i]) {
				s += sprintf(format_buf + s, "%s ", dotfile_buff[i]);		
				kfree(dotfile_buff[i]);
			}
		}
		fprintf (stdout, "%s\n", format_buf);

		fprintf ((oryx_file_t *)fp, "   %s\n", format_buf);
		dotfiles = 0;
	}
}

void
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
		fprintf ((oryx_file_t *)fp, "    %s", fs);
	}
}

void 
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
#endif

int main(int argc, char ** argv)
{
	uint32_t val_start, val_end;
	int ret;

	oryx_initialize();
	
#if defined(BUILD_DEBUG)
	fprintf (stdout, "RUN in debug mode\n");
#else
	fprintf (stdout, "RUN in release mode\n");
#endif

	ret = format_range("1:100", UINT16_MAX, 0, ':', &val_start, &val_end);
	fprintf (stdout, "ret = %d, start %d end %d\n",
		ret, val_start, val_end);

	ret = format_range("1:20", 2048, 0, ':', &val_start, &val_end);
	fprintf (stdout, "ret = %d, start %d end %d\n",
			ret, val_start, val_end);

	fprintf (stdout, "%.2f\n", ratio_of(1,2));
		

	return 0;
}

