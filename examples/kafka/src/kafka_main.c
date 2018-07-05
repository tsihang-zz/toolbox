#include "oryx.h"
#include "kafka.h"

struct kafka_param_t kafka_c_param = {
	.topic		=	"NEWTOPIC",
	.brokers	=	"localhost:9092",
	.partition	=	0,
};


struct kafka_param_t kafka_p_param = {
	.topic		=	"NEWTOPIC",
	.brokers	=	"localhost:9092",
	.partition	=	0,
};

static struct oryx_task_t kafka_c =
{
	.module = THIS,
	.sc_alias = "Kafka Consumer Task",
	.fn_handler = kafka_consumer,
	.ul_lcore = INVALID_CORE,
	.ul_prio = KERNEL_SCHED,
	.argc = 1,
	.argv = &kafka_c_param,
	.ul_flags = 0,	/** Can not be recyclable. */
};

static struct oryx_task_t kafka_p =
{
	.module = THIS,
	.sc_alias = "Kafka Producer Task",
	.fn_handler = kafka_producer,
	.ul_lcore = INVALID_CORE,
	.ul_prio = KERNEL_SCHED,
	.argc = 1,
	.argv = &kafka_p_param,
	.ul_flags = 0,	/** Can not be recyclable. */
};

int main(int argc, char ** argv)
{
	oryx_initialize();
	
	oryx_task_registry(&kafka_p);
	oryx_task_registry(&kafka_c);
	
	oryx_task_launch();
	
	FOREVER;

	
	return 0;
}
