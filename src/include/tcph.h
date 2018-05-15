#ifndef TCP_H_H
#define TCP_H_H

#ifndef IPPROTO_TCP
#define IPPROTO_TCP 6
#endif

#define TCP_HEADER_LEN                       20
#define TCP_OPTLENMAX                        40
#define TCP_OPTMAX                           20 /* every opt is at least 2 bytes
                                                 * (type + len), except EOL and NOP */

/* TCP flags */

#define TH_FIN                               0x01
#define TH_SYN                               0x02
#define TH_RST                               0x04
#define TH_PUSH                              0x08
#define TH_ACK                               0x10
#define TH_URG                               0x20
/** Establish a new connection reducing window */
#define TH_ECN                               0x40
/** Echo Congestion flag */
#define TH_CWR                               0x80

/* tcp option codes */
#define TCP_OPT_EOL                          0x00
#define TCP_OPT_NOP                          0x01
#define TCP_OPT_MSS                          0x02
#define TCP_OPT_WS                           0x03
#define TCP_OPT_SACKOK                       0x04
#define TCP_OPT_SACK                         0x05
#define TCP_OPT_TS                           0x08

#define TCP_OPT_SACKOK_LEN                   2
#define TCP_OPT_WS_LEN                       3
#define TCP_OPT_TS_LEN                       10
#define TCP_OPT_MSS_LEN                      4
#define TCP_OPT_SACK_MIN_LEN                 10 /* hdr 2, 1 pair 8 = 10 */
#define TCP_OPT_SACK_MAX_LEN                 34 /* hdr 2, 4 pair 32= 34 */

/** Max valid wscale value. */
#define TCP_WSCALE_MAX                       14

#define TCP_GET_RAW_OFFSET(tcph)             (((tcph)->th_offx2 & 0xf0) >> 4)
#define TCP_GET_RAW_X2(tcph)                 (unsigned char)((tcph)->th_offx2 & 0x0f)
#define TCP_GET_RAW_SRC_PORT(tcph)           ntohs((tcph)->th_sport)
#define TCP_GET_RAW_DST_PORT(tcph)           ntohs((tcph)->th_dport)

#define TCP_SET_RAW_TCP_OFFSET(tcph, value)  ((tcph)->th_offx2 = (unsigned char)(((tcph)->th_offx2 & 0x0f) | (value << 4)))
#define TCP_SET_RAW_TCP_X2(tcph, value)      ((tcph)->th_offx2 = (unsigned char)(((tcph)->th_offx2 & 0xf0) | (value & 0x0f)))

#define TCP_GET_RAW_SEQ(tcph)                ntohl((tcph)->th_seq)
#define TCP_GET_RAW_ACK(tcph)                ntohl((tcph)->th_ack)

#define TCP_GET_RAW_WINDOW(tcph)             ntohs((tcph)->th_win)
#define TCP_GET_RAW_URG_POINTER(tcph)        ntohs((tcph)->th_urp)
#define TCP_GET_RAW_SUM(tcph)                ntohs((tcph)->th_sum)

typedef struct TCPOpt_ {
    uint8_t type;
    uint8_t len;
    uint8_t *data;
} TCPOpt;

typedef struct TCPHdr_
{
    uint16_t th_sport;  /**< source port */
    uint16_t th_dport;  /**< destination port */
    uint32_t th_seq;    /**< sequence number */
    uint32_t th_ack;    /**< acknowledgement number */
    uint8_t th_offx2;   /**< offset and reserved */
    uint8_t th_flags;   /**< pkt flags */
    uint16_t th_win;    /**< pkt window */
    uint16_t th_sum;    /**< checksum */
    uint16_t th_urp;    /**< urgent pointer */
} __attribute__((__packed__)) TCPHdr;

typedef struct TCPVars_
{
    /* commonly used and needed opts */
    _Bool ts_set;
    uint32_t ts_val;    /* host-order */
    uint32_t ts_ecr;    /* host-order */
    TCPOpt sack;
    TCPOpt sackok;
    TCPOpt ws;
    TCPOpt mss;
} TCPVars;

#endif

