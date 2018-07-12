#ifndef MDX_H
#define MDX_H

#define ORYX_MD5_BUFLEN	64

typedef struct {
	union {
		uint32_t	md5_state32[4];
		uint8_t	md5_state8[16];
	} md5_st;

#define md5_sta		md5_st.md5_state32[0]
#define md5_stb		md5_st.md5_state32[1]
#define md5_stc		md5_st.md5_state32[2]
#define md5_std		md5_st.md5_state32[3]
#define md5_st8		md5_st.md5_state8

	union {
		uint64_t	md5_count64;
		uint8_t	md5_count8[8];
	} md5_count;
#define md5_n	md5_count.md5_count64
#define md5_n8	md5_count.md5_count8

	uint	md5_i;
	uint8_t	md5_buf[ORYX_MD5_BUFLEN];
} oryx_md5_ctxt;

extern void oryx_md5_init (oryx_md5_ctxt *);
extern void oryx_md5_loop (oryx_md5_ctxt *, const void *, u_int);
extern void oryx_md5_pad (oryx_md5_ctxt *);
extern void oryx_md5_result (uint8_t *, oryx_md5_ctxt *);
extern void oryx_hmac_md5(
			unsigned char*  text,			/* pointer to data stream */
			int             text_len,		/* length of data stream */
			unsigned char*  key,			/* pointer to authentication key */
			int             key_len,		/* length of authentication key */
			uint8_t *       digest			/* caller digest to be filled in */
);

/* compatibility */
#define MD5_CTX		oryx_md5_ctxt
#define MD5Init(x)	oryx_md5_init((x))
#define MD5Update(x, y, z)	oryx_md5_loop((x), (y), (z))
#define MD5Final(x, y) \
do {				\
	oryx_md5_pad((y));		\
	oryx_md5_result((x), (y));	\
} while (0)

#endif

