#include "oryx.h"
#include "kafka.h"

static int run = 1;
static rd_kafka_t *rk;
static int exit_eof = 0;
static int quiet = 0;
static 	enum {
	OUTPUT_HEXDUMP,
	OUTPUT_RAW,
} output = OUTPUT_HEXDUMP;


static void hexdump (FILE *fp, const char *name, const void *ptr, size_t len) {
	const char *p = (const char *)ptr;
	size_t of = 0;


	if (name)
		fprintf(fp, "%s hexdump (%zd bytes):\n", name, len);

	for (of = 0 ; of < len ; of += 16) {
		char hexen[16*3+1];
		char charen[16+1];
		int hof = 0;

		int cof = 0;
		int i;

		for (i = of ; i < (int)of + 16 && i < (int)len ; i++) {
			hof += sprintf(hexen+hof, "%02x ", p[i] & 0xff);
			cof += sprintf(charen+cof, "%c",
				       isprint((int)p[i]) ? p[i] : '.');
		}
		fprintf(fp, "%08zx: %-48s %-16s\n",
			of, hexen, charen);
	}
}

static void msg_consume (rd_kafka_message_t *rkmessage,
			 void *opaque) {
#if 0
	if (rkmessage->err) {
		if (rkmessage->err == RD_KAFKA_RESP_ERR__PARTITION_EOF) {
			fprintf(stderr,
				"%% Consumer reached end of %s [%"PRId32"] "
			       "message queue at offset %"PRId64"\n",
			       rd_kafka_topic_name(rkmessage->rkt),
			       rkmessage->partition, rkmessage->offset);

			if (exit_eof)
				run = 0;

			return;
		}

		fprintf(stderr, "%% Consume error for topic \"%s\" [%"PRId32"] "
		       "offset %"PRId64": %s\n",
		       rd_kafka_topic_name(rkmessage->rkt),
		       rkmessage->partition,
		       rkmessage->offset,
		       rd_kafka_message_errstr(rkmessage));

                if (rkmessage->err == RD_KAFKA_RESP_ERR__UNKNOWN_PARTITION ||
                    rkmessage->err == RD_KAFKA_RESP_ERR__UNKNOWN_TOPIC)
                        run = 0;
		return;
	}

	if (!quiet) {
		rd_kafka_timestamp_type_t tstype;
		int64_t timestamp;
                rd_kafka_headers_t *hdrs;

		fprintf(stdout, "%% Message (offset %"PRId64", %zd bytes):\n",
			rkmessage->offset, rkmessage->len);

		timestamp = rd_kafka_message_timestamp(rkmessage, &tstype);
		if (tstype != RD_KAFKA_TIMESTAMP_NOT_AVAILABLE) {
			const char *tsname = "?";
			if (tstype == RD_KAFKA_TIMESTAMP_CREATE_TIME)
				tsname = "create time";
			else if (tstype == RD_KAFKA_TIMESTAMP_LOG_APPEND_TIME)
				tsname = "log append time";

			fprintf(stdout, "%% Message timestamp: %s %"PRId64
				" (%ds ago)\n",
				tsname, timestamp,
				!timestamp ? 0 :
				(int)time(NULL) - (int)(timestamp/1000));
		}

                if (!rd_kafka_message_headers(rkmessage, &hdrs)) {
                        size_t idx = 0;
                        const char *name;
                        const void *val;
                        size_t size;

                        fprintf(stdout, "%% Headers:");

                        while (!rd_kafka_header_get_all(hdrs, idx++,
                                                        &name, &val, &size)) {
                                fprintf(stdout, "%s%s=",
                                        idx == 1 ? " " : ", ", name);
                                if (val)
                                        fprintf(stdout, "\"%.*s\"",
                                                (int)size, (const char *)val);
                                else
                                        fprintf(stdout, "NULL");
                        }
                        fprintf(stdout, "\n");
                }
	}

	if (rkmessage->key_len) {
		if (output == OUTPUT_HEXDUMP)
			hexdump(stdout, "Message Key",
				rkmessage->key, rkmessage->key_len);
		else
			printf("Key: %.*s\n",
			       (int)rkmessage->key_len, (char *)rkmessage->key);
	}

	if (output == OUTPUT_HEXDUMP)
		hexdump(stdout, "Message Payload",
			rkmessage->payload, rkmessage->len);
	else
		printf("%.*s\n",
		       (int)rkmessage->len, (char *)rkmessage->payload);
#endif
}


static void metadata_print (const char *topic,
                            const struct rd_kafka_metadata *metadata) {
        int i, j, k;

        printf("Metadata for %s (from broker %"PRId32": %s):\n",
               topic ? : "all topics",
               metadata->orig_broker_id,
               metadata->orig_broker_name);


        /* Iterate brokers */
        printf(" %i brokers:\n", metadata->broker_cnt);
        for (i = 0 ; i < metadata->broker_cnt ; i++)
                printf("  broker %"PRId32" at %s:%i\n",
                       metadata->brokers[i].id,
                       metadata->brokers[i].host,
                       metadata->brokers[i].port);

        /* Iterate topics */
        printf(" %i topics:\n", metadata->topic_cnt);
        for (i = 0 ; i < metadata->topic_cnt ; i++) {
                const struct rd_kafka_metadata_topic *t = &metadata->topics[i];
                printf("  topic \"%s\" with %i partitions:",
                       t->topic,
                       t->partition_cnt);
                if (t->err) {
                        printf(" %s", rd_kafka_err2str(t->err));
                        if (t->err == RD_KAFKA_RESP_ERR_LEADER_NOT_AVAILABLE)
                                printf(" (try again)");
                }
                printf("\n");

                /* Iterate topic's partitions */
                for (j = 0 ; j < t->partition_cnt ; j++) {
                        const struct rd_kafka_metadata_partition *p;
                        p = &t->partitions[j];
                        printf("    partition %"PRId32", "
                               "leader %"PRId32", replicas: ",
                               p->id, p->leader);

                        /* Iterate partition's replicas */
                        for (k = 0 ; k < p->replica_cnt ; k++)
                                printf("%s%"PRId32,
                                       k > 0 ? ",":"", p->replicas[k]);

                        /* Iterate partition's ISRs */
                        printf(", isrs: ");
                        for (k = 0 ; k < p->isr_cnt ; k++)
                                printf("%s%"PRId32,
                                       k > 0 ? ",":"", p->isrs[k]);
                        if (p->err)
                                printf(", %s\n", rd_kafka_err2str(p->err));
                        else
                                printf("\n");
                }
        }
}


static void sig_usr1 (int sig) {
	rd_kafka_dump(stdout, rk);
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
			
static int kafka_init_consumer(
			struct kafka_param_t *kp,
			rd_kafka_t **rk_out, rd_kafka_conf_t **conf_out,
			rd_kafka_topic_t **rkt_out)
{
		rd_kafka_t *rk; 	   /* Producer instance handle */
		rd_kafka_topic_t *rkt;	/* Topic object */
		rd_kafka_conf_t *conf;	/* Temporary configuration object */
		rd_kafka_topic_conf_t *topic_conf;
		char errstr[512];		/* librdkafka API error reporting buffer */
		
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

		rk = rd_kafka_new(RD_KAFKA_CONSUMER, conf, errstr, sizeof(errstr));
		if (!rk) {
				fprintf(stderr,
						"%% Failed to create new %s: %s\n", 
								"consumer", errstr);
				rd_kafka_topic_conf_destroy(topic_conf);
				rd_kafka_conf_destroy(conf);
				return -1;
		}

		rd_kafka_set_log_level(rk, LOG_DEBUG);
		
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
void *kafka_consumer (void *argv) {

        rd_kafka_t *rk;         /* Producer instance handle */
        rd_kafka_topic_t *rkt;  /* Topic object */
        rd_kafka_conf_t *conf;  /* Temporary configuration object */
        char buf[512];          /* Message value temporary buffer */
		rd_kafka_resp_err_t err;
		struct kafka_param_t *kp = (struct kafka_param_t *)argv;
		
		if (kafka_init_consumer(kp, &rk, &conf, &rkt))
			return NULL;

		/* Add brokers */
        if (rd_kafka_brokers_add(rk, kp->brokers) == 0) {
            fprintf(stderr, "%% No valid brokers specified\n");
            exit(1);
        }
		
		/* Start consuming */
		if (rd_kafka_consume_start(rkt, kp->partition, RD_KAFKA_OFFSET_END) == -1){
			rd_kafka_resp_err_t err = rd_kafka_last_error();
			fprintf(stderr, "%% Failed to start consuming: %s\n",
				rd_kafka_err2str(err));
                        if (err == RD_KAFKA_RESP_ERR__INVALID_ARG)
                                fprintf(stderr,
                                        "%% Broker based offset storage "
                                        "requires a group.id, "
                                        "add: -X group.id=yourGroup\n");
			exit(1);
		}


        while (run) {
			rd_kafka_message_t *rkmessage;
			/* Poll for errors, etc. */
			 rd_kafka_poll(rk, 0);

			rkmessage = rd_kafka_consume(rkt, kp->partition, 1000);
			if (!rkmessage) /* timeout */
				continue;
			
			msg_consume(rkmessage, NULL);
			rd_kafka_message_destroy(rkmessage);
        }

		err = rd_kafka_consumer_close(rk);
		if (err)
				fprintf(stderr, "%% Failed to close consumer: %s\n",
						rd_kafka_err2str(err));
		else
				fprintf(stderr, "%% Consumer closed\n");
	
		/* Destroy handle */
		rd_kafka_destroy(rk);

		/* Let background threads clean up and terminate cleanly. */
		run = 5;
		while (run-- > 0 && rd_kafka_wait_destroyed(1000) == -1)
			printf("Waiting for librdkafka to decommission\n");
		if (run <= 0)
			rd_kafka_dump(stdout, rk);


		return NULL;
}



