#ifndef TR_DECODE_SCTP_H
#define TR_DECODE_SCTP_H

#if !defined(HAVE_SURICATA)
#ifndef IPPROTO_SCTP
#define IPPROTO_SCTP 132
#endif

/** size of the packet header without any chunk headers */
#define SCTP_HEADER_LEN                       12

/* XXX RAW* needs to be really 'raw', so no ntohs there */
#define SCTP_GET_RAW_SRC_PORT(sctph)          ntohs((sctph)->sh_sport)
#define SCTP_GET_RAW_DST_PORT(sctph)          ntohs((sctph)->sh_dport)

#define SCTP_GET_SRC_PORT(p)                  SCTP_GET_RAW_SRC_PORT(p->sctph)
#define SCTP_GET_DST_PORT(p)                  SCTP_GET_RAW_DST_PORT(p->sctph)

typedef struct SCTPHdr_
{
    uint16_t sh_sport;     /* source port */
    uint16_t sh_dport;     /* destination port */
    uint32_t sh_vtag;      /* verification tag, defined per flow */
    uint32_t sh_sum;       /* checksum, computed via crc32 */
} __attribute__((__packed__)) SCTPHdr;

#define CLEAR_SCTP_PACKET(p) { \
    (p)->sctph = NULL; \
} while (0)
#endif


int DecodeSCTP0(ThreadVars *tv, DecodeThreadVars *dtv, Packet *p, uint8_t *pkt, uint16_t len, PacketQueue *pq);

#endif

