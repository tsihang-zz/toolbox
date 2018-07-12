#include "oryx.h"
#include "geo_capture.h"

int main (
	int __oryx_unused_param__ argc,
	char __oryx_unused_param__ ** argv
)
{
	argc = argc;
	argv = argv;
	
	oryx_initialize();
	
	geo_start_pcap();

	oryx_task_launch();

	FOREVER;
}

