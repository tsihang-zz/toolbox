#include "oryx.h"
#include "mpm.h"
#include "http.h"

static int pattern_id;

static __oryx_always_inline__
void http_parse(struct http_hdr_t *hh, const char *http_buf, size_t http_buflen)
{
	size_t pos = 0;
	size_t pos0 = 0;

	if ((http_buf == NULL) ||
		!__is_wanted_http (http_buf, http_buflen, &hh->method))
		return;

	while (pos ++ < http_buflen) {
		if (http_buf[pos] == CR) {
			pos0 = pos + 1;
			if (http_buf[pos0] == LF) {
				pos0 += 1;
				//fprintf (stdout, "=> %s", (char *)&http_buf[pos0]);
				if (strncmp ((const char *)&http_buf[pos0], "Content-Type: ", 14) == 0) {
					__parse_content_type(hh, http_buf, http_buflen, 14, &pos0);
				} else if (strncmp ((const char *)&http_buf[pos0], "User-Agent: ", 12) == 0) {
					__parse_user_agent(hh, http_buf, http_buflen, 12, &pos0);
				} else if (strncmp ((const char *)&http_buf[pos0], "Host: ", 6) == 0) {
					__parse_host(hh, http_buf, http_buflen, 6, &pos0);
				}
			}
		}
	}
}

static __oryx_always_inline__
void http_dump(struct http_hdr_t *h, const char *http_buf, size_t http_buflen)
{
	struct http_keyval_t *v;

#if 0
	fprintf (stdout, "%s", "HTTP RAW --\n");
	fprintf (stdout, "%s", http_buf);
#endif

	if(h->ul_flags & HTTP_APPEAR_CONTENT_TYPE) {
		v = &h->ct;
		fprintf (stdout, "Content-Type: %s (%lu)\n", &v->val[0], v->valen);
	}

	if(h->ul_flags & HTTP_APPEAR_USER_AGENT) {
		v = &h->ua;
		fprintf (stdout, "User-Agent: %s (%lu)\n", &v->val[0], v->valen);
	}
	
	if(h->ul_flags & HTTP_APPEAR_HOST) {
		v = &h->host;
		fprintf (stdout, "Host: %s (%lu)\n", &v->val[0], v->valen);
	}

}

static void mpm_install_content_type(MpmCtx *mpm_ctx)
{
	MpmAddPatternCS(mpm_ctx, (uint8_t *)"video/flv", strlen("video/flv"),
			0, 0, pattern_id ++, 0, 0);
	MpmAddPatternCS(mpm_ctx, (uint8_t *)"video/x-flv", strlen("video/x-flv"),
			0, 0, pattern_id ++, 0, 0);
	MpmAddPatternCS(mpm_ctx, (uint8_t *)"video/mp4", strlen("video/mp4"),
			0, 0, pattern_id ++, 0, 0);
	MpmAddPatternCS(mpm_ctx, (uint8_t *)"application/mp4", strlen("application/mp4"),
			0, 0, pattern_id ++, 0, 0);
	MpmAddPatternCS(mpm_ctx, (uint8_t *)"application/octet-stream", strlen("application/octet-stream"),
			0, 0, pattern_id ++, 0, 0);
	MpmAddPatternCS(mpm_ctx, (uint8_t *)"flv-application/octet-stream", strlen("flv-application/octet-stream"),
			0, 0, pattern_id ++, 0, 0);
	
}

static __oryx_always_inline__
int http_match(MpmCtx *mpm_ctx, MpmThreadCtx *mpm_thread_ctx, PrefilterRuleStore *pmq, struct http_keyval_t *v)
{
	return MpmSearch(mpm_ctx, mpm_thread_ctx, pmq,
				(uint8_t *)&v->val[0], v->valen);
}

int main (
	int		__oryx_unused_param__	argc,
	char	__oryx_unused_param__	** argv
)
{
    int result = 0;
    MpmCtx mpm_ctx;
    MpmThreadCtx mpm_thread_ctx;
    PrefilterRuleStore pmq;

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

	struct http_hdr_t h0, h1, h2, h3, h4, h5;

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
	MpmTableSetup();

    memset(&mpm_ctx, 0, sizeof(MpmCtx));
    memset(&mpm_thread_ctx, 0, sizeof(MpmThreadCtx));	
	MpmInitCtx(&mpm_ctx, MPM_HS);
	mpm_install_content_type(&mpm_ctx);
    PmqSetup(&pmq);
	MpmPreparePatterns(&mpm_ctx);
	MpmInitThreadCtx(&mpm_thread_ctx, MPM_HS);

	cnt = http_match(&mpm_ctx, &mpm_thread_ctx, &pmq, &h0.ct);
	if (cnt == 1)
	  result = 1;
	else
	  printf("1 != %" PRIu32 " \n", cnt);

	cnt = http_match(&mpm_ctx, &mpm_thread_ctx, &pmq, &h1.ct);
	if (cnt == 1)
	  result = 1;
	else
	  printf("1 != %" PRIu32 " \n", cnt);

	cnt = http_match(&mpm_ctx, &mpm_thread_ctx, &pmq, &h2.ct);
	if (cnt == 1)
	  result = 1;
	else
	  printf("1 != %" PRIu32 " \n", cnt);

	cnt = http_match(&mpm_ctx, &mpm_thread_ctx, &pmq, &h3.ct);
	if (cnt == 1)
	  result = 1;
	else
	  printf("1 != %" PRIu32 " \n", cnt);

	cnt = http_match(&mpm_ctx, &mpm_thread_ctx, &pmq, &h4.ct);
	if (cnt == 1)
	  result = 1;
	else
	  printf("1 != %" PRIu32 " \n", cnt);

	cnt = http_match(&mpm_ctx, &mpm_thread_ctx, &pmq, &h5.ct);
	if (cnt == 1)
	  result = 1;
	else
	  printf("1 != %" PRIu32 " \n", cnt);


    MpmDestroyCtx(&mpm_ctx);
    MpmDestroyThreadCtx(&mpm_ctx, &mpm_thread_ctx);
    PmqFree(&pmq);
	
    return result;
}

