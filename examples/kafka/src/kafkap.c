#include "oryx.h"
#include "kafka.h"

static int run = 1;

struct kafka_producer_param_t kpp = {
	.bytes = 0,
	.packets = 0,
	.errors = 0,
};

/**
 * @brief Message delivery report callback.
 *
 * This callback is called exactly once per message, indicating if
 * the message was succesfully delivered
 * (rkmessage->err == RD_KAFKA_RESP_ERR_NO_ERROR) or permanently
 * failed delivery (rkmessage->err != RD_KAFKA_RESP_ERR_NO_ERROR).
 *
 * The callback is triggered from rd_kafka_poll() and executes on
 * the application's thread.
 */
static void dr_msg_cb (rd_kafka_t *rk,
                       const rd_kafka_message_t *rkmessage, void __oryx_unused_param__ *opaque) {
        struct kafka_producer_param_t *k = &kpp;
		if (rkmessage->err)
			k->errors ++;
		else {
			k->bytes += rkmessage->len;
			k->packets ++;
		}
        /* The rkmessage is destroyed automatically by librdkafka */
}


void *kafka_producer (void *argv) {

        rd_kafka_t *rk;         /* Producer instance handle */
        rd_kafka_topic_t *rkt;  /* Topic object */
        rd_kafka_conf_t *conf;  /* Temporary configuration object */
        char buf[512];          /* Message value temporary buffer */
		struct kafka_param_t *kp = (struct kafka_param_t *)argv;
		
		if (kafka_init_env(RD_KAFKA_PRODUCER, kp, &rk, &conf, &rkt))
			return NULL;

        fprintf(stderr,
                "%% Type some text and hit enter to produce message\n"
                "%% Or just hit enter to only serve delivery reports\n"
                "%% Press Ctrl-C or Ctrl-D to exit\n");
        //while (run && fgets(buf, sizeof(buf), stdin)) {
        while (run) {
				usleep (1000);
				size_t len = oryx_pattern_generate(buf, 511);
                /*
                 * Send/Produce message.
                 * This is an asynchronous call, on success it will only
                 * enqueue the message on the internal producer queue.
                 * The actual delivery attempts to the broker are handled
                 * by background threads.
                 * The previously registered delivery report callback
                 * (dr_msg_cb) is used to signal back to the application
                 * when the message has been delivered (or failed).
                 */
        retry:
                if (rd_kafka_produce(
                            /* Topic object */
                            rkt,
                            /* Use builtin partitioner to select partition*/
                            RD_KAFKA_PARTITION_UA,
                            /* Make a copy of the payload. */
                            RD_KAFKA_MSG_F_COPY,
                            /* Message payload (value) and length */
                            buf, len,
                            /* Optional key and its length */
                            NULL, 0,
                            /* Message opaque, provided in
                             * delivery report callback as
                             * msg_opaque. */
                            (void *)&kpp) == -1) {
                        /**
                         * Failed to *enqueue* message for producing.
                         */
                        fprintf(stderr,
                                "%% Failed to produce to topic %s: %s\n",
                                rd_kafka_topic_name(rkt),
                                rd_kafka_err2str(rd_kafka_last_error()));

                        /* Poll to handle delivery reports */
                        if (rd_kafka_last_error() ==
                            RD_KAFKA_RESP_ERR__QUEUE_FULL) {
                                /* If the internal queue is full, wait for
                                 * messages to be delivered and then retry.
                                 * The internal queue represents both
                                 * messages to be sent and messages that have
                                 * been sent or failed, awaiting their
                                 * delivery report callback to be called.
                                 *
                                 * The internal queue is limited by the
                                 * configuration property
                                 * queue.buffering.max.messages */
                                rd_kafka_poll(rk, 1000/*block for max 1000ms*/);
                                goto retry;
                        }
                } 

                /* A producer application should continually serve
                 * the delivery report queue by calling rd_kafka_poll()
                 * at frequent intervals.
                 * Either put the poll call in your main loop, or in a
                 * dedicated thread, or call it after every
                 * rd_kafka_produce() call.
                 * Just make sure that rd_kafka_poll() is still called
                 * during periods where you are not producing any messages
                 * to make sure previously produced messages have their
                 * delivery report callback served (and any other callbacks
                 * you register). */
                rd_kafka_poll(rk, 0/*non-blocking*/);

				if ((kpp.packets % 10000) == 0) {
	                fprintf(stdout, "%% Delivered messages (%zd bytes, %zd packets) "
	                    "for topic %s\n",
	                    kpp.bytes, kpp.packets, rd_kafka_topic_name(rkt));
				}
        }


        /* Wait for final messages to be delivered or fail.
         * rd_kafka_flush() is an abstraction over rd_kafka_poll() which
         * waits for all messages to be delivered. */
        fprintf(stderr, "%% Flushing final messages..\n");
        rd_kafka_flush(rk, 10*1000 /* wait for max 10 seconds */);

        /* Destroy topic object */
        rd_kafka_topic_destroy(rkt);

        /* Destroy the producer instance */
        rd_kafka_destroy(rk);

        return NULL;
}

