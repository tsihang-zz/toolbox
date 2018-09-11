#ifndef DP_DECODE_HTTP_H
#define DP_DECODE_HTTP_H

#ifndef CR
#define CR '\r'
#endif

#ifndef LF
#define LF '\n'
#endif

#ifndef SP
#define SP ' '
#endif

#define HTTP_PROTOCOL_0_9             9		/* HTTP/0.9 */
#define HTTP_PROTOCOL_1_0             100	/* HTTP/1.0 */
#define HTTP_PROTOCOL_1_1             101	/* HTTP/1.1 */

/**
 * HTTP methods.
 */
enum http_method_t {
    HTTP_METHOD_UNKNOWN = 0,
    HTTP_METHOD_HEAD = 1,
    HTTP_METHOD_GET = 2,
    HTTP_METHOD_PUT = 3,
    HTTP_METHOD_POST = 4,
    HTTP_METHOD_DELETE = 5,
    HTTP_METHOD_CONNECT = 6,
    HTTP_METHOD_OPTIONS = 7,
    HTTP_METHOD_TRACE = 8,
    HTTP_METHOD_PATCH = 9,
    HTTP_METHOD_PROPFIND = 10,
    HTTP_METHOD_PROPPATCH = 11,
    HTTP_METHOD_MKCOL = 12,
    HTTP_METHOD_COPY = 13,
    HTTP_METHOD_MOVE = 14,
    HTTP_METHOD_LOCK = 15,
    HTTP_METHOD_UNLOCK = 16,
    HTTP_METHOD_VERSION_CONTROL = 17,
    HTTP_METHOD_CHECKOUT = 18,
    HTTP_METHOD_UNCHECKOUT = 19,
    HTTP_METHOD_CHECKIN = 20,
    HTTP_METHOD_UPDATE = 21,
    HTTP_METHOD_LABEL = 22,
    HTTP_METHOD_REPORT = 23,
    HTTP_METHOD_MKWORKSPACE = 24,
    HTTP_METHOD_MKACTIVITY = 25,
    HTTP_METHOD_BASELINE_CONTROL = 26,
    HTTP_METHOD_MERGE = 27,
    HTTP_METHOD_RESPONSE = 28,
    HTTP_METHOD_INVALID = 29

};

#define	HTTP_APPEAR_CONTENT_TYPE	(1 << 0)
#define HTTP_APPEAR_USER_AGENT		(1 << 1)
#define HTTP_APPEAR_HOST			(1 << 2)
#define HTTP_APPEAR_URI				(1 << 3)

struct http_keyval_t {
	char	val[1024];
	size_t	valen;
};

struct http_ctx_t {

	size_t size;
	
	uint8_t method;
	
	struct http_keyval_t uri;
	struct http_keyval_t host;
	struct http_keyval_t ua;
	struct http_keyval_t ct;

	uint32_t	ul_flags;
};

static __oryx_always_inline__
void __parse_uri(struct http_ctx_t *ctx, const char *buf, size_t buflen, size_t off, size_t *pos)
{
	
	struct http_keyval_t *kv = &ctx->uri;
	kv->valen = 0;

	(*pos) += off;
	
	while ((*pos) < (buflen - off)) {
		/* an example of get method like below.
		 * GET /vod/115/945/4ac58e3/bc312960_4588 HTTP/1.1\r\n */
		if(buf[(*pos)] == SP) {
			ctx->ul_flags |= HTTP_APPEAR_URI;
			return;
		}
		kv->val[kv->valen ++] =  buf[(*pos)];
		(*pos) ++;
	}

}

static __oryx_always_inline__
void __parse_content_type(struct http_ctx_t *ctx, const char *buf, size_t buflen, size_t off, size_t *pos)
{
	struct http_keyval_t *kv;

	/* to Content Type */
	kv = &ctx->ct;
	kv->valen = 0;

	ctx->ul_flags |= HTTP_APPEAR_CONTENT_TYPE;

	(*pos) += off;
	while ((*pos) < buflen) {
		if (buf[(*pos)] == CR && buf[(*pos) + 1] == LF)
			return;
		else
			kv->val[kv->valen ++] = buf[(*pos)];
		(*pos) ++;
	}
}

static __oryx_always_inline__
void __parse_user_agent(struct http_ctx_t *ctx, const char *buf, size_t buflen, size_t off, size_t *pos)
{
	struct http_keyval_t *kv;

	/* to User Agent */
	kv = &ctx->ua;
	kv->valen = 0;

	ctx->ul_flags |= HTTP_APPEAR_USER_AGENT;

	(*pos) += off;
	while ((*pos) < buflen) {
		if (buf[(*pos)] == CR && buf[(*pos) + 1] == LF)
			return;
		else
			kv->val[kv->valen ++] = buf[(*pos)];
		(*pos) ++;
	}
}

static __oryx_always_inline__
void __parse_host(struct http_ctx_t *ctx, const char *buf, size_t buflen, size_t off, size_t *pos)
{
	struct http_keyval_t *kv;

	/* to Host */
	kv = &ctx->host;
	kv->valen = 0;

	ctx->ul_flags |= HTTP_APPEAR_HOST;

	(*pos) += off;
	while ((*pos) < buflen) {
		if (buf[(*pos)] == CR && buf[(*pos) + 1] == LF)
			return;
		else
			kv->val[kv->valen ++] = buf[(*pos)];
		(*pos) ++;
	}
}

#define METHOD_KEYWORD_GET		"GET "
#define METHOD_KEYWORD_POST		"POST "
#define METHOD_KEYWORD_RESPONSE	"HTTP"

static __oryx_always_inline__
int __is_wanted_http(struct http_ctx_t *ctx, const char *buf, size_t buflen)
{
	ctx->method = HTTP_METHOD_UNKNOWN;
	
	/** only compare first n bytes with http method.
	 *  this may be not very accurate, but it is efficient for most part of frames. */
	if (strncmp(buf, METHOD_KEYWORD_GET, 4) == 0) {
		ctx->method = HTTP_METHOD_GET;
	} else if (strncmp(buf, METHOD_KEYWORD_POST, 5) == 0) {
		ctx->method = HTTP_METHOD_POST;
	} else if (strncmp (buf, METHOD_KEYWORD_RESPONSE, 4) == 0) {
		/* A http response from server. */
		ctx->method = HTTP_METHOD_RESPONSE;
	} 

	return (ctx->method != HTTP_METHOD_UNCHECKOUT);
}
static __oryx_always_inline__
void __http_kv_dump(const char *prefix, struct http_keyval_t *kv)
{
	int valen = kv->valen;

	fprintf (stdout, "%16s:", prefix);
	while (valen) {
		fprintf (stdout, "%c", kv->val[kv->valen - valen]);
		 valen --;
	}
	fprintf (stdout, "%s", "\n");
}

static __oryx_always_inline__
int http_parse(struct http_ctx_t *ctx, const char *buf, size_t buflen)
{
	size_t pos = 0;
	size_t pos0 = 0;

	if (!__is_wanted_http (ctx, buf, buflen)) {
		return -1;
	}

	if (ctx->method == HTTP_METHOD_GET) {
		/* only http GET method has URI. */
		__parse_uri(ctx, buf, buflen, 4, &pos0);

	} else {
		/* back to the start */
		while (pos ++ < buflen) {
			if (buf[pos] == CR) {
				pos0 = pos + 1;
				if (buf[pos0] == LF) {
					pos0 += 1;
					if (strncmp ((const char *)&buf[pos0], "Content-Type: ", 14) == 0) {
						__parse_content_type(ctx, buf, buflen, 14, &pos0);
						break;
					}
				}
			}
		}
	}
	return 0;
}

static __oryx_always_inline__
void http_dump(struct http_ctx_t *ctx, const char *buf, size_t buflen)
{
	struct http_keyval_t *v;

	if(ctx->ul_flags & HTTP_APPEAR_CONTENT_TYPE) {
		v = &ctx->ct;
		__http_kv_dump("Content-Type", v);
	}
	if(ctx->ul_flags & HTTP_APPEAR_URI) {
		v = &ctx->uri;
		__http_kv_dump("URI", v);
	}
#if 0
	if(ctx->ul_flags & HTTP_APPEAR_USER_AGENT) {
		v = &ctx->ua;
		fprintf (stdout, "User-Agent: %s (%lu)\n", &v->val[0], v->valen);
	}
	
	if(ctx->ul_flags & HTTP_APPEAR_HOST) {
		v = &ctx->host;
		fprintf (stdout, "Host: %s (%lu)\n", &v->val[0], v->valen);
	}
#endif
}




#endif
