#include "oryx.h"

int main (
	IN int	__oryx_unused__	argc,
	IN char	__oryx_unused__	** argv
)
{
	struct oryx_file_t f;
	int err;
	const char *data = "aafdsfdsfdsfdsfds";

	oryx_initialize();

	ORYX_FILE_INIT(&f);
	err = oryx_fopen("./txt", "a+", 1024, &f);
	if (err) {
		return -1;
	}

	fprintf(stdout, "write %u bytes\n", oryx_fwrite(&f, data, strlen(data)));
	oryx_fclose(&f);
	oryx_fdestroy(&f);

	return 0;
}

