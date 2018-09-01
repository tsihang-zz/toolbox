#ifndef ICMP_H_H
#define ICMP_H_H

#ifndef IPPROTO_ICMP
#define IPPROTO_ICMP 1
#endif

#define ICMPV4_HEADER_LEN       8

#ifndef ICMP_ECHOREPLY
#define ICMP_ECHOREPLY          0       /* Echo Reply                   */
#endif
#ifndef ICMP_DEST_UNREACH
#define ICMP_DEST_UNREACH       3       /* Destination Unreachable      */
#endif
#ifndef ICMP_SOURCE_QUENCH
#define ICMP_SOURCE_QUENCH      4       /* Source Quench                */
#endif
#ifndef ICMP_REDIRECT
#define ICMP_REDIRECT           5       /* Redirect (change route)      */
#endif
#ifndef ICMP_ECHO
#define ICMP_ECHO               8       /* Echo Request                 */
#endif
#ifndef ICMP_TIME_EXCEEDED
#define ICMP_TIME_EXCEEDED      11      /* Time Exceeded                */
#endif
#ifndef ICMP_PARAMETERPROB
#define ICMP_PARAMETERPROB      12      /* Parameter Problem            */
#endif
#ifndef ICMP_TIMESTAMP
#define ICMP_TIMESTAMP          13      /* Timestamp Request            */
#endif
#ifndef ICMP_TIMESTAMPREPLY
#define ICMP_TIMESTAMPREPLY     14      /* Timestamp Reply              */
#endif
#ifndef ICMP_INFO_REQUEST
#define ICMP_INFO_REQUEST       15      /* Information Request          */
#endif
#ifndef ICMP_INFO_REPLY
#define ICMP_INFO_REPLY         16      /* Information Reply            */
#endif
#ifndef ICMP_ADDRESS
#define ICMP_ADDRESS            17      /* Address Mask Request         */
#endif
#ifndef ICMP_ADDRESSREPLY
#define ICMP_ADDRESSREPLY       18      /* Address Mask Reply           */
#endif
#ifndef NR_ICMP_TYPES
#define NR_ICMP_TYPES           18
#endif


/* Codes for UNREACH. */
#ifndef ICMP_NET_UNREACH
#define ICMP_NET_UNREACH        0       /* Network Unreachable          */
#endif
#ifndef ICMP_HOST_UNREACH
#define ICMP_HOST_UNREACH       1       /* Host Unreachable             */
#endif
#ifndef ICMP_PROT_UNREACH
#define ICMP_PROT_UNREACH       2       /* Protocol Unreachable         */
#endif
#ifndef ICMP_PORT_UNREACH
#define ICMP_PORT_UNREACH       3       /* Port Unreachable             */
#endif
#ifndef ICMP_FRAG_NEEDED
#define ICMP_FRAG_NEEDED        4       /* Fragmentation Needed/DF set  */
#endif
#ifndef ICMP_SR_FAILED
#define ICMP_SR_FAILED          5       /* Source Route failed          */
#endif
#ifndef ICMP_NET_UNKNOWN
#define ICMP_NET_UNKNOWN        6
#endif
#ifndef ICMP_HOST_UNKNOWN
#define ICMP_HOST_UNKNOWN       7
#endif
#ifndef ICMP_HOST_ISOLATED
#define ICMP_HOST_ISOLATED      8
#endif
#ifndef ICMP_NET_ANO
#define ICMP_NET_ANO            9
#endif
#ifndef ICMP_HOST_ANO
#define ICMP_HOST_ANO           10
#endif
#ifndef ICMP_NET_UNR_TOS
#define ICMP_NET_UNR_TOS        11
#endif
#ifndef ICMP_HOST_UNR_TOS
#define ICMP_HOST_UNR_TOS       12
#endif
#ifndef ICMP_PKT_FILTERED
#define ICMP_PKT_FILTERED       13      /* packet_t filtered */
#endif
#ifndef ICMP_PREC_VIOLATION
#define ICMP_PREC_VIOLATION     14      /* Precedence violation */

#endif
#ifndef ICMP_PREC_CUTOFF
#define ICMP_PREC_CUTOFF        15      /* Precedence cut off */
#endif
#ifndef NR_ICMP_UNREACH
#define NR_ICMP_UNREACH         15      /* instead of hardcoding immediate value */
#endif

/* Codes for REDIRECT. */
#ifndef ICMP_REDIR_NET
#define ICMP_REDIR_NET          0       /* Redirect Net                 */
#endif
#ifndef ICMP_REDIR_HOST
#define ICMP_REDIR_HOST         1       /* Redirect Host                */
#endif
#ifndef ICMP_REDIR_NETTOS
#define ICMP_REDIR_NETTOS       2       /* Redirect Net for TOS         */
#endif
#ifndef ICMP_REDIR_HOSTTOS
#define ICMP_REDIR_HOSTTOS      3       /* Redirect Host for TOS        */
#endif

/* Codes for TIME_EXCEEDED. */
#ifndef ICMP_EXC_TTL
#define ICMP_EXC_TTL            0       /* TTL count exceeded           */
#endif
#ifndef ICMP_EXC_FRAGTIME
#define ICMP_EXC_FRAGTIME       1       /* Fragment Reass time exceeded */
#endif

/* ICMPv4 header structure */
typedef struct ICMPV4Hdr_
{
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
} __attribute__((__packed__)) ICMPV4Hdr;

/* ICMPv4 header structure */
typedef struct ICMPV4ExtHdr_
{
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
    uint16_t id;
    uint16_t seq;
} ICMPV4ExtHdr;

/* ICMPv4 vars */
typedef struct ICMPV4Vars_
{
    uint16_t  id;
    uint16_t  seq;

    /** Pointers to the embedded packet headers */
    /** Pointers to the embedded packet headers */
    IPv4Hdr *emb_ipv4h;
    TCPHdr *emb_tcph;
    UDPHdr *emb_udph;
    ICMPV4Hdr *emb_icmpv4h;


    /** IPv4 src and dst address */
    struct in_addr emb_ip4_src;
    struct in_addr emb_ip4_dst;
    uint8_t emb_ip4_hlen;
    uint8_t emb_ip4_proto;

    /** TCP/UDP ports */
    uint16_t emb_sport;
    uint16_t emb_dport;
} ICMPV4Vars;


#ifndef IPPROTO_ICMPv6
#define IPPROTO_ICMPv6 58
#endif

#define ICMPV6_HEADER_LEN       8
#define ICMPV6_HEADER_PKT_OFFSET 8

/** ICMPV6 Message Types: */
/** Error Messages: (type <128) */
#define ICMP6_DST_UNREACH             1
#define ICMP6_PACKET_TOO_BIG          2
#define ICMP6_TIME_EXCEEDED           3
#define ICMP6_PARAM_PROB              4

/** Informational Messages (type>=128) */
#define ICMP6_ECHO_REQUEST          128
#define ICMP6_ECHO_REPLY            129

#define MLD_LISTENER_QUERY          130
#define MLD_LISTENER_REPORT         131
#define MLD_LISTENER_REDUCTION      132

#define ND_ROUTER_SOLICIT           133
#define ND_ROUTER_ADVERT            134
#define ND_NEIGHBOR_SOLICIT         135
#define ND_NEIGHBOR_ADVERT          136
#define ND_REDIRECT                 137

#define ICMP6_RR                    138
#define ICMP6_NI_QUERY              139
#define ICMP6_NI_REPLY              140
#define ND_INVERSE_SOLICIT          141
#define ND_INVERSE_ADVERT           142
#define MLD_V2_LIST_REPORT          143
#define HOME_AGENT_AD_REQUEST       144
#define HOME_AGENT_AD_REPLY         145
#define MOBILE_PREFIX_SOLICIT       146
#define MOBILE_PREFIX_ADVERT        147
#define CERT_PATH_SOLICIT           148
#define CERT_PATH_ADVERT            149
#define ICMP6_MOBILE_EXPERIMENTAL   150
#define MC_ROUTER_ADVERT            151
#define MC_ROUTER_SOLICIT           152
#define MC_ROUTER_TERMINATE         153
#define FMIPv6_MSG                  154
#define RPL_CONTROL_MSG             155
#define LOCATOR_UDATE_MSG           156
#define DUPL_ADDR_REQUEST           157
#define DUPL_ADDR_CONFIRM           158
#define MPL_CONTROL_MSG             159

/** Destination Unreachable Message (type=1) Code: */

#define ICMP6_DST_UNREACH_NOROUTE       0 /* no route to destination */
#define ICMP6_DST_UNREACH_ADMIN         1 /* communication with destination */
                                          /* administratively prohibited */
#define ICMP6_DST_UNREACH_BEYONDSCOPE   2 /* beyond scope of source address */
#define ICMP6_DST_UNREACH_ADDR          3 /* address unreachable */
#define ICMP6_DST_UNREACH_NOPORT        4 /* bad port */
#define ICMP6_DST_UNREACH_FAILEDPOLICY  5 /* Source address failed ingress/egress policy */
#define ICMP6_DST_UNREACH_REJECTROUTE   6 /* Reject route to destination */


/** Time Exceeded Message (type=3) Code: */
#define ICMP6_TIME_EXCEED_TRANSIT     0 /* Hop Limit == 0 in transit */
#define ICMP6_TIME_EXCEED_REASSEMBLY  1 /* Reassembly time out */

/** Parameter Problem Message (type=4) Code: */
#define ICMP6_PARAMPROB_HEADER        0 /* erroneous header field */
#define ICMP6_PARAMPROB_NEXTHEADER    1 /* unrecognized Next Header */
#define ICMP6_PARAMPROB_OPTION        2 /* unrecognized IPv6 option */

typedef struct ICMPV6Info_
{
    uint16_t  id;
    uint16_t  seq;
} ICMPV6Info;

/** ICMPv6 header structure */
typedef struct ICMPV6Hdr_
{
    uint8_t  type;
    uint8_t  code;
    uint16_t csum;

    union {
        ICMPV6Info icmpv6i; /** Informational message */
        union
        {
            uint32_t  unused; /** for types 1 and 3, should be zero */
            uint32_t  error_ptr; /** for type 4, pointer to the octet that originate the error */
            uint32_t  mtu; /** for type 2, the Maximum Transmission Unit of the next-hop link */
        } icmpv6e;   /** Error Message */
    } icmpv6b;
} ICMPV6Hdr;

/** Data available from the decoded packet */
typedef struct ICMPV6Vars_ {
    /* checksum of the icmpv6 packet */
    uint16_t  id;
    uint16_t  seq;
    uint32_t  mtu;
    uint32_t  error_ptr;

    /** Pointers to the embedded packet headers */
    IPv6Hdr *emb_ipv6h;
    TCPHdr *emb_tcph;
    UDPHdr *emb_udph;
    ICMPV6Hdr *emb_icmpv6h;


    /** IPv6 src and dst address */
    uint32_t emb_ip6_src[4];
    uint32_t emb_ip6_dst[4];
    uint8_t emb_ip6_proto_next;

    /** TCP/UDP ports */
    uint16_t emb_sport;
    uint16_t emb_dport;

} ICMPV6Vars;

#endif
