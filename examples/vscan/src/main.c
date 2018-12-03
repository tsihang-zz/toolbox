#include "oryx.h"
#include "mpm.h"
#include "http.h"

static int pattern_id;
volatile bool force_quit = false;
extern struct oryx_task_t rx_netdev_task;

mpm_ctx_t mpm_ctx;
mpm_threadctx_t mpm_thread_ctx;
PrefilterRuleStore pmq;

struct http_content_type_handler_t {
	mpm_ctx_t mpm_ctx;
	mpm_threadctx_t mpm_thread_ctx;
	PrefilterRuleStore pmq;
};

static int mpm_test(void)
{
	mpm_ctx_t mpm_ctx;
	mpm_threadctx_t mpm_thread_ctx;
	PrefilterRuleStore pmq;

	int result = 0;
	mpm_table_setup();
	memset(&mpm_ctx, 0, sizeof(mpm_ctx_t));
	memset(&mpm_thread_ctx, 0, sizeof(mpm_threadctx_t));
	
	mpm_ctx_init(&mpm_ctx, MPM_HS);
	/* 1 match */
	mpm_pattern_add_cs(&mpm_ctx, (uint8_t *)"abcd", 4, 0, 0, 0, 0, 0);
	mpm_pattern_add_ci(&mpm_ctx, (uint8_t *)"ABCD", 4, 0, 0, 0, 0, 0);
	mpm_pmq_setup(&pmq);
	mpm_pattern_prepare(&mpm_ctx);
	mpm_threadctx_init(&mpm_thread_ctx, MPM_HS);

	const char *buf = "efghjiklabcdmnopqrstuvwxyz";

	uint32_t cnt;

	cnt = mpm_pattern_search(&mpm_ctx, &mpm_thread_ctx, &pmq, (uint8_t *)buf,
							  strlen(buf));

	if (cnt == 1)
		result = 1;
	else
		fprintf(stdout, "1 != %" PRIu32 " ", cnt);

	const char *buf1 = "abc";

	cnt = mpm_pattern_search(&mpm_ctx, &mpm_thread_ctx, &pmq, (uint8_t *)buf1,
							  strlen(buf1));

	if (cnt == 1)
		result = 1;
	else
		fprintf(stdout, "1 != %" PRIu32 " ", cnt);

	mpm_ctx_destroy(&mpm_ctx);
	mpm_threadctx_destroy(&mpm_ctx, &mpm_thread_ctx);
	mpm_pmq_free(&pmq);
	
	return result;
}

static void mpm_install_content_type(mpm_ctx_t *mpm_ctx)
{
	mpm_pattern_add_ci(mpm_ctx, (uint8_t *)"video/flv", strlen("video/flv"),
			0, 0, pattern_id ++, 0, 0);
	mpm_pattern_add_ci(mpm_ctx, (uint8_t *)"video/x-flv", strlen("video/x-flv"),
			0, 0, pattern_id ++, 0, 0);
	mpm_pattern_add_ci(mpm_ctx, (uint8_t *)"video/mp4", strlen("video/mp4"),
			0, 0, pattern_id ++, 0, 0);
	mpm_pattern_add_ci(mpm_ctx, (uint8_t *)"video/mp2t", strlen("video/mp2t"),
			0, 0, pattern_id ++, 0, 0);
	mpm_pattern_add_ci(mpm_ctx, (uint8_t *)"application/mp4", strlen("application/mp4"),
			0, 0, pattern_id ++, 0, 0);
	mpm_pattern_add_ci(mpm_ctx, (uint8_t *)"application/octet-stream", strlen("application/octet-stream"),
			0, 0, pattern_id ++, 0, 0);
	mpm_pattern_add_ci(mpm_ctx, (uint8_t *)"flv-application/octet-stream", strlen("flv-application/octet-stream"),
			0, 0, pattern_id ++, 0, 0);	
}

int http_match(mpm_ctx_t *mpm_ctx, mpm_threadctx_t *mpm_thread_ctx, PrefilterRuleStore *pmq, struct http_keyval_t *v)
{
	return mpm_pattern_search(mpm_ctx, mpm_thread_ctx, pmq,
				(uint8_t *)&v->val[0], v->valen);
}

static void
sig_handler(int signum) {
	
	if (signum == SIGINT || signum == SIGTERM) {
		fprintf (stdout, "\n\nSignal %d received, preparing to exit...\n", signum);
		force_quit = true;
	}
}

int main (
	int		__oryx_unused__	argc,
	char	__oryx_unused__	** argv
)
{
	//mpm_test();
#if 1
int result = 0;	
	/* video/flv */
    uint8_t http_buf0[] =
        "GET /index.html HTTP/1.0\r\n"
        "Host: www.openinfosecfoundation.org\r\n"
        "User-Agent: Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.9.1.7) Gecko/20091221 Firefox/3.5.7\r\n"
        "Content-Type: video/flv\r\n"
        "Content-Length: 26\r\n"
        "\r\n"
        "This is dummy message body\r\n";

	/* video/x-flv */
	uint8_t http_buf1[] =
		"GET /index.html HTTP/1.0\r\n"
		"Host: www.openinfosecfoundation.org\r\n"
		"User-Agent: Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.9.1.7) Gecko/20091221 Firefox/3.5.7\r\n"
		"Content-Type: video/x-flv\r\n"
		"Content-Length: 26\r\n"
		"\r\n"
		"This is dummy message body\r\n";

	/* video/mp4 */
	uint8_t http_buf2[] =
		"GET /index.html HTTP/1.0\r\n"
		"Host: www.openinfosecfoundation.org\r\n"
		"User-Agent: Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.9.1.7) Gecko/20091221 Firefox/3.5.7\r\n"
		"Content-Type: video/mp4\r\n"
		"Content-Length: 26\r\n"
		"\r\n"
		"This is dummy message body\r\n";

	/* application/mp4 */
	uint8_t http_buf3[] =
		"GET /index.html HTTP/1.0\r\n"
		"Host: www.openinfosecfoundation.org\r\n"
		"User-Agent: Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.9.1.7) Gecko/20091221 Firefox/3.5.7\r\n"
		"Content-Type: application/mp4\r\n"
		"Content-Length: 26\r\n"
		"\r\n"
		"This is dummy message body\r\n";

	/* application/octet-stream */
	uint8_t http_buf4[] =
		"GET /index.html HTTP/1.0\r\n"
		"Host: www.openinfosecfoundation.org\r\n"
		"User-Agent: Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.9.1.7) Gecko/20091221 Firefox/3.5.7\r\n"
		"Content-Type: application/octet-stream\r\n"
		"Content-Length: 26\r\n"
		"\r\n"
		"This is dummy message body\r\n";

	/* flv-application/octet-stream */
	uint8_t http_buf5[] =
		"GET /index.html HTTP/1.0\r\n"
		"Host: www.openinfosecfoundation.org\r\n"
		"User-Agent: Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.9.1.7) Gecko/20091221 Firefox/3.5.7\r\n"
		"Content-Type: flv-application/octet-stream\r\n"
		"Content-Length: 26\r\n"
		"\r\n"
		"This is dummy message body\r\n";

	struct http_ctx_t h0, h1, h2, h3, h4, h5;

	memset (&h0, 0, sizeof (h0));
	http_parse (&h0, (const char *)&http_buf0[0], strlen((const char *)&http_buf0[0]));
	http_dump (&h0, (const char *)&http_buf0[0], strlen((const char *)&http_buf0[0]));
	
	memset (&h1, 0, sizeof (h1));
	http_parse (&h1, (const char *)&http_buf1[0], strlen((const char *)&http_buf1[0]));
	http_dump (&h1, (const char *)&http_buf1[0], strlen((const char *)&http_buf1[0]));

	memset (&h2, 0, sizeof (h2));
	http_parse (&h2, (const char *)&http_buf2[0], strlen((const char *)&http_buf2[0]));
	http_dump (&h2, (const char *)&http_buf1[0], strlen((const char *)&http_buf2[0]));

	memset (&h3, 0, sizeof (h3));
	http_parse (&h3, (const char *)&http_buf3[0], strlen((const char *)&http_buf3[0]));
	http_dump (&h3, (const char *)&http_buf3[0], strlen((const char *)&http_buf3[0]));

	memset (&h4, 0, sizeof (h4));
	http_parse (&h4, (const char *)&http_buf4[0], strlen((const char *)&http_buf4[0]));
	http_dump (&h4, (const char *)&http_buf4[0], strlen((const char *)&http_buf4[0]));

	memset (&h5, 0, sizeof (h5));
	http_parse (&h5, (const char *)&http_buf5[0], strlen((const char *)&http_buf5[0]));
	http_dump (&h5, (const char *)&http_buf5[0], strlen((const char *)&http_buf5[0]));

    uint32_t cnt;
	oryx_initialize();
	oryx_register_sighandler(SIGINT, sig_handler);
	oryx_register_sighandler(SIGTERM, sig_handler);

	mpm_table_setup();

    memset(&mpm_ctx, 0, sizeof(mpm_ctx_t));
    memset(&mpm_thread_ctx, 0, sizeof(mpm_threadctx_t));	
	mpm_ctx_init(&mpm_ctx, MPM_HS);
	mpm_install_content_type(&mpm_ctx);
    mpm_pmq_setup(&pmq);
	mpm_pattern_prepare(&mpm_ctx);
	mpm_threadctx_init(&mpm_thread_ctx, MPM_HS);

	cnt = http_match(&mpm_ctx, &mpm_thread_ctx, &pmq, &h0.ct);
	if (cnt == 1)
	  result = 1;
	else
	  fprintf(stdout, "1 != %" PRIu32 " \n", cnt);

	cnt = http_match(&mpm_ctx, &mpm_thread_ctx, &pmq, &h1.ct);
	if (cnt == 1)
	  result = 1;
	else
	  fprintf(stdout, "1 != %" PRIu32 " \n", cnt);

	cnt = http_match(&mpm_ctx, &mpm_thread_ctx, &pmq, &h2.ct);
	if (cnt == 1)
	  result = 1;
	else
	  fprintf(stdout, "1 != %" PRIu32 " \n", cnt);

	cnt = http_match(&mpm_ctx, &mpm_thread_ctx, &pmq, &h3.ct);
	if (cnt == 1)
	  result = 1;
	else
	  fprintf(stdout, "1 != %" PRIu32 " \n", cnt);

	cnt = http_match(&mpm_ctx, &mpm_thread_ctx, &pmq, &h4.ct);
	if (cnt == 1)
	  result = 1;
	else
	  fprintf(stdout, "1 != %" PRIu32 " \n", cnt);

	cnt = http_match(&mpm_ctx, &mpm_thread_ctx, &pmq, &h5.ct);
	if (cnt == 1)
	  result = 1;
	else
	  fprintf(stdout, "1 != %" PRIu32 " \n", cnt);
	
	oryx_task_registry(&rx_netdev_task);
	oryx_task_launch();
	FOREVER {
		/* wait for quit. */
		if(force_quit)
			break;

		usleep(10000);
	}

    mpm_ctx_destroy(&mpm_ctx);
    mpm_threadctx_destroy(&mpm_ctx, &mpm_thread_ctx);
    mpm_pmq_free(&pmq);
#endif	
    return 0;
}

