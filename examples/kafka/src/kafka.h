#ifndef KAFKA_H
#define KAFKA_H

/* Typical include path would be <librdkafka/rdkafka.h>, but this program
 * is builtin from within the librdkafka source tree and thus differs. */
#include "rdkafka.h"

extern  int kafka_producer_init_done;

struct kafka_param_t {
	const char	*topic;			/* Argument: topic to produce to */
	const char	*brokers;	 	/* Argument: broker list */
	int32_t		partition;		/* ... */
};

struct kafka_producer_param_t {
	uint64_t bytes;
	uint64_t packets;
	uint64_t errors;
};
struct kafka_producer_param_t kpp;

static __oryx_always_inline__
void kafka_conf_dump(rd_kafka_conf_t *conf, rd_kafka_topic_conf_t *topic_conf)
{
	const char **arr;
	size_t cnt;
	int pass;
	
	for (pass = 0 ; pass < 2 ; pass++) {
		int i;
	
		if (pass == 0) {
			arr = rd_kafka_conf_dump(conf, &cnt);
			printf("# Global config\n");
		} else {
			printf("# Topic config\n");
			arr = rd_kafka_topic_conf_dump(topic_conf,
							   &cnt);
		}
	
		for (i = 0 ; i < (int)cnt ; i += 2)
			printf("%s = %s\n",
				   arr[i], arr[i+1]);
	
		printf("\n");
	
		rd_kafka_conf_dump_free(arr, cnt);
	}

}

/**
 * Kafka logger callback (optional)
 */
static void logger (const rd_kafka_t *rk, int level,
		    const char *fac, const char *buf) {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	fprintf(stderr, "%u.%03u RDKAFKA-%i-%s: %s: %s\n",
		(int)tv.tv_sec, (int)(tv.tv_usec / 1000),
		level, fac, rk ? rd_kafka_name(rk) : NULL, buf);
}

/**
* Message delivery report callback.
* Called once for each message.
* See rdkafka.h for more information.
*/
static void dr_cb (rd_kafka_t *rk,
			  void *payload, size_t len,
			  int error_code,
			  void *opaque, void *msg_opaque) {
	struct kafka_producer_param_t *k = (struct kafka_producer_param_t *)msg_opaque;
	if (error_code) {
		k->errors ++;
	   fprintf(stderr, "%% Message delivery failed: %s\n",
		   rd_kafka_err2str(error_code));
	}
	else {
		k->bytes += len;
		k->packets ++;
	}
	/* The rkmessage is destroyed automatically by librdkafka */
}


static __oryx_always_inline__
int kafka_init_env(rd_kafka_type_t type,
			struct kafka_param_t *kp,
			rd_kafka_t **rk_out, rd_kafka_conf_t **conf_out,
			rd_kafka_topic_t **rkt_out)
{
		rd_kafka_t *rk; 	   /* Producer instance handle */
		rd_kafka_topic_t *rkt;	/* Topic object */
		rd_kafka_conf_t *conf;	/* Temporary configuration object */
		rd_kafka_topic_conf_t *topic_conf;
		char errstr[512];		/* librdkafka API error reporting buffer */
		const char *debug = "XX";
		
		BUG_ON(rk_out == NULL);
		BUG_ON(conf_out == NULL);

		/*
         * Create Kafka client configuration place-holder
         */
		conf = rd_kafka_conf_new();
		if (!conf) {
				fprintf(stderr,
						"%% Failed to create new kafka conf: %s\n",
								rd_kafka_err2str(rd_kafka_last_error()));
				return -1;	
		}

		/* Topic configuration */
		topic_conf = rd_kafka_topic_conf_new();
		if (!topic_conf) {
				fprintf(stderr,
						"%% Failed to create new topic conf: %s\n",
								rd_kafka_err2str(rd_kafka_last_error()));
			  rd_kafka_conf_destroy(conf);
				return -1;	
		}

		if (type == RD_KAFKA_PRODUCER) {
			/* Set bootstrap broker(s) as a comma-separated list of
			 * host or host:port (default port 9092).
			 * librdkafka will use the bootstrap brokers to acquire the full
			 * set of brokers from the cluster. */
			if (rd_kafka_conf_set(conf, "bootstrap.servers", kp->brokers,
									errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
					fprintf(stderr, "%s\n", errstr);
					rd_kafka_topic_conf_destroy(topic_conf);
					rd_kafka_conf_destroy(conf);
					return -1;
			}
			/* Set the delivery report callback.
			 * This callback will be called once per message to inform
			 * the application if delivery succeeded or failed.
			 * See dr_msg_cb() above. */
			//rd_kafka_conf_set_dr_msg_cb(conf, dr_msg_cb);
			rd_kafka_conf_set_dr_cb(conf, dr_cb);

		} 

		if (type == RD_KAFKA_CONSUMER) {

			char tmp[16];
			/* Set logger */
			rd_kafka_conf_set_log_cb(conf, logger);
			/* Quick termination */
			snprintf(tmp, sizeof(tmp), "%i", SIGIO);
			if (rd_kafka_conf_set(conf, "internal.termination.signal", tmp, 
									errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
				fprintf(stderr, "%s\n", errstr);
				rd_kafka_topic_conf_destroy(topic_conf);
				rd_kafka_conf_destroy(conf);
				return -1;
			}
		}

		rk = rd_kafka_new(type, conf, errstr, sizeof(errstr));
		if (!rk) {
				fprintf(stderr,
						"%% Failed to create new %s: %s\n", 
								type == RD_KAFKA_PRODUCER ? "producer" : "consumer", errstr);
				rd_kafka_topic_conf_destroy(topic_conf);
				rd_kafka_conf_destroy(conf);
				return -1;
		}

		/* Create topic object that will be reused for each message
		 * produced.
		 *
		 * Both the producer instance (rd_kafka_t) and topic objects (topic_t)
		 * are long-lived objects that should be reused as much as possible.
		 */
		rkt = rd_kafka_topic_new(rk, kp->topic, topic_conf);
		if (!rkt) {
			  fprintf(stderr, "%% Failed to create topic object: %s\n",
					  rd_kafka_err2str(rd_kafka_last_error()));
			  rd_kafka_destroy(rk);
			  rd_kafka_topic_conf_destroy(topic_conf);
			  rd_kafka_conf_destroy(conf);
			  return -1;
		}

		(*rk_out)	= rk;
		(*rkt_out)	= rkt;
		(*conf_out) = conf;
		
		return 0;
}

void *kafka_consumer (void *argv);
void *kafka_producer (void *argv);


#endif
