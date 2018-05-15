#ifndef IP_H_H
#define IP_H_H

#ifndef IPPROTO_IP
#define IPPROTO_IP 0
#endif

#ifndef IPPROTO_IPIP
#define IPPROTO_IPIP 4
#endif

/* older libcs don't contain a def for IPPROTO_DCCP
 * inside of <netinet/in.h>
 * if it isn't defined let's define it here.
 */
#ifndef IPPROTO_DCCP
#define IPPROTO_DCCP 33
#endif

#define IPV4_HEADER_LEN           20    /**< Header length */
#define IPV4_OPTMAX               40    /**< Max options length */
#define	IPV4_MAXPACKET_LEN        65535 /**< Maximum packet size */

/** IP Option Types */
#define IPV4_OPT_EOL              0x00  /**< Option: End of List */
#define IPV4_OPT_NOP              0x01  /**< Option: No op */
#define IPV4_OPT_RR               0x07  /**< Option: Record Route */
#define IPV4_OPT_QS               0x19  /**< Option: Quick Start */
#define IPV4_OPT_TS               0x44  /**< Option: Timestamp */
#define IPV4_OPT_SEC              0x82  /**< Option: Security */
#define IPV4_OPT_LSRR             0x83  /**< Option: Loose Source Route */
#define IPV4_OPT_CIPSO            0x86  /**< Option: Commercial IP Security */
#define IPV4_OPT_SID              0x88  /**< Option: Stream Identifier */
#define IPV4_OPT_SSRR             0x89  /**< Option: Strict Source Route */
#define IPV4_OPT_RTRALT           0x94  /**< Option: Router Alert */

/** IP Option Lengths (fixed) */
#define IPV4_OPT_SEC_LEN          11    /**< SEC Option Fixed Length */
#define IPV4_OPT_SID_LEN          4     /**< SID Option Fixed Length */
#define IPV4_OPT_RTRALT_LEN       4     /**< RTRALT Option Fixed Length */

/** IP Option Lengths (variable) */
#define IPV4_OPT_ROUTE_MIN        3     /**< RR, SRR, LTRR Option Min Length */
#define IPV4_OPT_QS_MIN           8     /**< QS Option Min Length */
#define IPV4_OPT_TS_MIN           5     /**< TS Option Min Length */
#define IPV4_OPT_CIPSO_MIN        10    /**< CIPSO Option Min Length */


/* helper structure with parsed ipv4 info */
typedef struct IPV4Vars_
{
    int32_t comp_csum;     /* checksum computed over the ipv4 packet */

    uint16_t opt_cnt;
    uint16_t opts_set;
} IPV4Vars;

typedef struct IPV4Opt_ {
    /** \todo We may want to break type up into its 3 fields
     *        as the reassembler may want to know which options
     *        must be copied to each fragment.
     */
    uint8_t type;         /**< option type */
    uint8_t len;          /**< option length (type+len+data) */
    uint8_t *data;        /**< option data */
} IPV4Opt;

typedef struct IPV4Hdr_
{
    uint8_t ip_verhl;     /**< version & header length */
    uint8_t ip_tos;       /**< type of service */
    uint16_t ip_len;      /**< length */
    uint16_t ip_id;       /**< id */
    uint16_t ip_off;      /**< frag offset */
    uint8_t ip_ttl;       /**< time to live */
    uint8_t ip_proto;     /**< protocol (tcp, udp, etc) */
    uint16_t ip_csum;     /**< checksum */
    union {
        struct {
            struct in_addr ip_src;/**< source address */
            struct in_addr ip_dst;/**< destination address */
        } ip4_un1;
        uint16_t ip_addrs[4];
    } ip4_hdrun1;
} __attribute__((__packed__)) IPV4Hdr;

/** IP Option fields */
#define IPV4_OPTS                 ip4vars.ip_opts
#define IPV4_OPTS_CNT             ip4vars.ip_opt_cnt

#define s_ip_src                          ip4_hdrun1.ip4_un1.ip_src
#define s_ip_dst                          ip4_hdrun1.ip4_un1.ip_dst
#define s_ip_addrs                        ip4_hdrun1.ip_addrs

#define IPV4_GET_RAW_VER(ip4h)            (((ip4h)->ip_verhl & 0xf0) >> 4)
#define IPV4_GET_RAW_HLEN(ip4h)           ((ip4h)->ip_verhl & 0x0f)
#define IPV4_GET_RAW_IPTOS(ip4h)          ((ip4h)->ip_tos)
#define IPV4_GET_RAW_IPLEN(ip4h)          ((ip4h)->ip_len)
#define IPV4_GET_RAW_IPID(ip4h)           ((ip4h)->ip_id)
#define IPV4_GET_RAW_IPOFFSET(ip4h)       ((ip4h)->ip_off)
#define IPV4_GET_RAW_IPTTL(ip4h)          ((ip4h)->ip_ttl)
#define IPV4_GET_RAW_IPPROTO(ip4h)        ((ip4h)->ip_proto)
#define IPV4_GET_RAW_IPSRC(ip4h)          ((ip4h)->s_ip_src)
#define IPV4_GET_RAW_IPDST(ip4h)          ((ip4h)->s_ip_dst)

/** return the raw (directly from the header) src ip as uint32_t */
#define IPV4_GET_RAW_IPSRC_U32(ip4h)      (uint32_t)((ip4h)->s_ip_src.s_addr)
/** return the raw (directly from the header) dst ip as uint32_t */
#define IPV4_GET_RAW_IPDST_U32(ip4h)      (uint32_t)((ip4h)->s_ip_dst.s_addr)

/* we need to change them as well as get them */
#define IPV4_SET_RAW_VER(ip4h, value)     ((ip4h)->ip_verhl = (((ip4h)->ip_verhl & 0x0f) | (value << 4)))
#define IPV4_SET_RAW_HLEN(ip4h, value)    ((ip4h)->ip_verhl = (((ip4h)->ip_verhl & 0xf0) | (value & 0x0f)))
#define IPV4_SET_RAW_IPTOS(ip4h, value)   ((ip4h)->ip_tos = value)
#define IPV4_SET_RAW_IPLEN(ip4h, value)   ((ip4h)->ip_len = value)
#define IPV4_SET_RAW_IPPROTO(ip4h, value) ((ip4h)->ip_proto = value)


/* helper structure with parsed ipv6 info */
typedef struct IPV6Vars_
{
    uint8_t ip_opts_len;
    uint8_t l4proto;      /* the proto after the extension headers
                            * store while decoding so we don't have
                            * to loop through the exthdrs all the time */
} IPV6Vars;

typedef struct IPV6ExtHdrs_
{
    _Bool rh_set;
    uint8_t rh_type;

    _Bool fh_set;
    _Bool fh_more_frags_set;
    uint8_t fh_nh;

    uint8_t fh_prev_nh;
    uint16_t fh_prev_hdr_offset;

    uint16_t fh_header_offset;
    uint16_t fh_data_offset;
    uint16_t fh_data_len;

    /* In fh_offset we store the offset of this extension into the packet past
     * the ipv6 header. We use it in defrag for creating a defragmented packet
     * without the frag header */
    uint16_t fh_offset;
    uint32_t fh_id;

} IPV6ExtHdrs;

/* Fragment header */
typedef struct IPV6FragHdr_
{
    uint8_t  ip6fh_nxt;             /* next header */
    uint8_t  ip6fh_reserved;        /* reserved field */
    uint16_t ip6fh_offlg;           /* offset, reserved, and flag */
    uint32_t ip6fh_ident;           /* identification */
} __attribute__((__packed__)) IPV6FragHdr;

#define IPV6_EXTHDR_GET_FH_NH(p)            (p)->ip6eh.fh_nh
#define IPV6_EXTHDR_GET_FH_OFFSET(p)        (p)->ip6eh.fh_offset
#define IPV6_EXTHDR_GET_FH_FLAG(p)          (p)->ip6eh.fh_more_frags_set
#define IPV6_EXTHDR_GET_FH_ID(p)            (p)->ip6eh.fh_id

/* rfc 1826 */
typedef struct IPV6AuthHdr_
{
    uint8_t ip6ah_nxt;              /* next header */
    uint8_t ip6ah_len;              /* header length in units of 8 bytes, not
                                        including first 8 bytes. */
    uint16_t ip6ah_reserved;        /* reserved for future use */
    uint32_t ip6ah_spi;             /* SECURITY PARAMETERS INDEX (SPI) */
    uint32_t ip6ah_seq;             /* sequence number */
} __attribute__((__packed__)) IPV6AuthHdr;

typedef struct IPV6EspHdr_
{
    uint32_t ip6esph_spi;           /* SECURITY PARAMETERS INDEX (SPI) */
    uint32_t ip6esph_seq;           /* sequence number */
} __attribute__((__packed__)) IPV6EspHdr;

typedef struct IPV6RouteHdr_
{
    uint8_t ip6rh_nxt;               /* next header */
    uint8_t ip6rh_len;               /* header length in units of 8 bytes, not
                                        including first 8 bytes. */
    uint8_t ip6rh_type;              /* routing type */
    uint8_t ip6rh_segsleft;          /* segments left */
} __attribute__((__packed__)) IPV6RouteHdr;


/* Hop-by-Hop header and Destination Options header use options that are
 * defined here. */

#define IPV6OPT_PAD1                  0x00
#define IPV6OPT_PADN                  0x01
#define IPV6OPT_RA                    0x05
#define IPV6OPT_JUMBO                 0xC2
#define IPV6OPT_HAO                   0xC9

/* Home Address Option */
typedef struct IPV6OptHAO_
{
    uint8_t ip6hao_type;             /* Option type */
    uint8_t ip6hao_len;              /* Option Data len (excludes type and len) */
    struct in6_addr ip6hao_hoa;       /* Home address. */
} IPV6OptHAO;

/* Router Alert Option */
typedef struct IPV6OptRA_
{
    uint8_t ip6ra_type;             /* Option type */
    uint8_t ip6ra_len;              /* Option Data len (excludes type and len) */
    uint16_t ip6ra_value;           /* Router Alert value */
} IPV6OptRA;

/* Jumbo Option */
typedef struct IPV6OptJumbo_
{
    uint8_t ip6j_type;             /* Option type */
    uint8_t ip6j_len;              /* Option Data len (excludes type and len) */
    uint32_t ip6j_payload_len;     /* Jumbo Payload Length */
} IPV6OptJumbo;

typedef struct IPV6HopOptsHdr_
{
    uint8_t ip6hh_nxt;              /* next header */
    uint8_t ip6hh_len;              /* header length in units of 8 bytes, not
                                       including first 8 bytes. */
} __attribute__((__packed__)) IPV6HopOptsHdr;

typedef struct IPV6DstOptsHdr_
{
    uint8_t ip6dh_nxt;              /* next header */
    uint8_t ip6dh_len;              /* header length in units of 8 bytes, not
                                       including first 8 bytes. */
} __attribute__((__packed__)) IPV6DstOptsHdr;

typedef struct IPV6GenOptHdr_
{
    uint8_t type;
    uint8_t next;
    uint8_t len;
    uint8_t *data;
}   IPV6GenOptHdr;


typedef struct IPV6Hdr_
{
    union {
        struct ip6_un1_ {
            uint32_t ip6_un1_flow; /* 20 bits of flow-ID */
            uint16_t ip6_un1_plen; /* payload length */
            uint8_t  ip6_un1_nxt;  /* next header */
            uint8_t  ip6_un1_hlim; /* hop limit */
        } ip6_un1;
        uint8_t ip6_un2_vfc;   /* 4 bits version, top 4 bits class */
    } ip6_hdrun;

    union {
        struct {
            uint32_t ip6_src[4];
            uint32_t ip6_dst[4];
        } ip6_un2;
        uint16_t ip6_addrs[16];
    } ip6_hdrun2;
} __attribute__((__packed__)) IPV6Hdr;

#define s_ip6_src                       ip6_hdrun2.ip6_un2.ip6_src
#define s_ip6_dst                       ip6_hdrun2.ip6_un2.ip6_dst
#define s_ip6_addrs                     ip6_hdrun2.ip6_addrs

#define s_ip6_vfc                       ip6_hdrun.ip6_un2_vfc
#define s_ip6_flow                      ip6_hdrun.ip6_un1.ip6_un1_flow
#define s_ip6_plen                      ip6_hdrun.ip6_un1.ip6_un1_plen
#define s_ip6_nxt                       ip6_hdrun.ip6_un1.ip6_un1_nxt
#define s_ip6_hlim                      ip6_hdrun.ip6_un1.ip6_un1_hlim

#define IPV6_GET_RAW_VER(ip6h)          (((ip6h)->s_ip6_vfc & 0xf0) >> 4)
#define IPV6_GET_RAW_CLASS(ip6h)        ((ntohl((ip6h)->s_ip6_flow) & 0x0FF00000) >> 20)
#define IPV6_GET_RAW_FLOW(ip6h)         (ntohl((ip6h)->s_ip6_flow) & 0x000FFFFF)
#define IPV6_GET_RAW_NH(ip6h)           ((ip6h)->s_ip6_nxt)
#define IPV6_GET_RAW_PLEN(ip6h)         (ntohs((ip6h)->s_ip6_plen))
#define IPV6_GET_RAW_HLIM(ip6h)         ((ip6h)->s_ip6_hlim)

#define IPV6_SET_RAW_VER(ip6h, value)   ((ip6h)->s_ip6_vfc = (((ip6h)->s_ip6_vfc & 0x0f) | (value << 4)))
#define IPV6_SET_RAW_NH(ip6h, value)    ((ip6h)->s_ip6_nxt = (value))

#endif

