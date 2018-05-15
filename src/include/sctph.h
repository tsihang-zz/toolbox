#ifndef SCTP_H_H
#define SCTP_H_H

#ifndef IPPROTO_SCTP
#define IPPROTO_SCTP 132
#endif

/** size of the packet header without any chunk headers */
#define SCTP_HEADER_LEN                       12

/* XXX RAW* needs to be really 'raw', so no ntohs there */
#define SCTP_GET_RAW_SRC_PORT(sctph)          ntohs((sctph)->sh_sport)
#define SCTP_GET_RAW_DST_PORT(sctph)          ntohs((sctph)->sh_dport)

typedef struct SCTPHdr_
{
    uint16_t sh_sport;     /* source port */
    uint16_t sh_dport;     /* destination port */
    uint32_t sh_vtag;      /* verification tag, defined per flow */
    uint32_t sh_sum;       /* checksum, computed via crc32 */
} __attribute__((__packed__)) SCTPHdr;

#endif
