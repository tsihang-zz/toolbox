#ifndef UDP_H_H
#define UDP_H_H

#ifndef IPPROTO_UDP
#define IPPROTO_UDP 17
#endif

#define UDP_HEADER_LEN         8

/* XXX RAW* needs to be really 'raw', so no ntohs there */
#define UDP_GET_RAW_LEN(udph)                ntohs((udph)->uh_len)
#define UDP_GET_RAW_SRC_PORT(udph)           ntohs((udph)->uh_sport)
#define UDP_GET_RAW_DST_PORT(udph)           ntohs((udph)->uh_dport)
#define UDP_GET_RAW_SUM(udph)                ntohs((udph)->uh_sum)

/* UDP header structure */
typedef struct UDPHdr_
{
	uint16_t uh_sport;  /* source port */
	uint16_t uh_dport;  /* destination port */
	uint16_t uh_len;    /* length */
	uint16_t uh_sum;    /* checksum */
} __attribute__((__packed__)) UDPHdr;


#endif
