#include "oryx.h"
#include "geo_capture.h"

int main(int argc, char ** argv)
{
	argc = argc;
	argv = argv;
	
	oryx_initialize();
	
	geo_start_pcap();

	oryx_task_launch();

	FOREVER;
}

