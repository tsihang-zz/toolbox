#include "oryx.h"
#include "kafka.h"

extern void *kafka_consumer (void *argv);
extern void *kafka_producer (void *argv);

struct kafka_param_t kafka_c_param = {
	.topic		=	"NEWTOPIC",
	.brokers	=	"localhost:9092",	/* kafka/config/server.properties
										 * "listeners. */
	.partition	=	0,
};


struct kafka_param_t kafka_p_param = {
	.topic		=	"NEWTOPIC",
	.brokers	=	"[fe80::ad24:743c:3e95:557c]:9092",
	.partition	=	0,
};

static struct oryx_task_t kafka_c =
{
	.module		= THIS,
	.sc_alias	= "Kafka Consumer Task",
	.fn_handler	= kafka_consumer,
	.ul_lcore	= INVALID_CORE,
	.ul_prio	= KERNEL_SCHED,
	.argc		= 1,
	.argv		= &kafka_c_param,
	.ul_flags	= 0,	/** Can not be recyclable. */
};

static struct oryx_task_t kafka_p =
{
	.module		= THIS,
	.sc_alias	= "Kafka Producer Task",
	.fn_handler	= kafka_producer,
	.ul_lcore	= INVALID_CORE,
	.ul_prio	= KERNEL_SCHED,
	.argc		= 1,
	.argv		= &kafka_p_param,
	.ul_flags	= 0,	/** Can not be recyclable. */
};

int main(
	int 	__oryx_unused__	argc,
	char	__oryx_unused__	** argv
)
{
	oryx_initialize();
	
	oryx_task_registry(&kafka_p);
	//oryx_task_registry(&kafka_c);
	
	oryx_task_launch();
	
	FOREVER {
		{
			fprintf(stdout, "%% Delivered messages (%zd bytes, %zd packets)\n",
				kpp.bytes, kpp.packets);
		}
		sleep (1);
	};

	
	return 0;
}
