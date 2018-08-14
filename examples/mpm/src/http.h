#ifndef HTTP_H
#define HTTP_H

#ifndef CR
#define CR '\r'
#endif

#ifndef LF
#define LF '\n'
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
    HTTP_METHOD_INVALID = 28

};

#define	HTTP_APPEAR_CONTENT_TYPE	(1 << 0)
#define HTTP_APPEAR_USER_AGENT		(1 << 1)
#define HTTP_APPEAR_HOST			(1 << 2)

struct http_keyval_t {
	char	val[1024];
	size_t	valen;
};

struct http_hdr_t {

	size_t size;
	
	uint8_t method;
	struct http_keyval_t uri;
	struct http_keyval_t host;
	struct http_keyval_t ua;
	struct http_keyval_t ct;

	uint32_t	ul_flags;
};

static __oryx_always_inline__
void __parse_content_type(struct http_hdr_t *h, const char *buf, size_t buflen, size_t off, size_t *pos)
{
	struct http_keyval_t *kv;

	/* to Content Type */
	kv = &h->ct;
	kv->valen = 0;

	h->ul_flags |= HTTP_APPEAR_CONTENT_TYPE;

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
void __parse_user_agent(struct http_hdr_t *h, const char *buf, size_t buflen, size_t off, size_t *pos)
{
	struct http_keyval_t *kv;

	/* to User Agent */
	kv = &h->ua;
	kv->valen = 0;

	h->ul_flags |= HTTP_APPEAR_USER_AGENT;

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
void __parse_host(struct http_hdr_t *h, const char *buf, size_t buflen, size_t off, size_t *pos)
{
	struct http_keyval_t *kv;

	/* to Host */
	kv = &h->host;
	kv->valen = 0;

	h->ul_flags |= HTTP_APPEAR_HOST;

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
int __is_wanted_http(const char *buf, size_t buflen, uint8_t *method)
{
	(*method) = HTTP_METHOD_UNKNOWN;
	
	/** only compare first n bytes with http method.
	 *  this may be not very accurate, but it is efficient for most part of frames. */
	if (strncmp(buf, "GET ", 4) == 0) {
		(*method) = HTTP_METHOD_GET;
	} else if (strncmp (buf, "POST ", 5) == 0) {
		(*method) = HTTP_METHOD_POST;
	}

	return ((*method) != HTTP_METHOD_UNKNOWN);
}

#endif

