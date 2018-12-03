#include "oryx.h"
#include "classify.h"

int main(
	int 	__oryx_unused__	argc,
	char	__oryx_unused__	** argv
)
{
	oryx_initialize();

	nb_load_sample ("./pattern.txt");
	nb_post_proba ();
	nb_test ();

	return 0;
}

