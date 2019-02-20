#include "oryx.h"

struct oryx_lc_t *lc;

int main (
	int 	__oryx_unused__	argc,
	char	__oryx_unused__	** argv
)
{
	int i;

	oryx_initialize();

	oryx_lc_new("LC", 1024, 0, &lc);

	FOREVER;
}

