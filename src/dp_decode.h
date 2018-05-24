#ifndef DP_DECODE_H
#define DP_DECODE_H

#include "version.h"

#include "common_private.h"
#include "vars_private.h"
#include "queue_private.h"

#include "threadvars.h"
#include "event.h"
/* < protocol headers> */
#include "ethh.h"
#include "dsah.h"
#include "iph.h"
#include "tcph.h"
#include "udph.h"
#include "icmph.h"
#include "sctph.h"
#include "ppph.h"
#include "pppoeh.h"
#include "greh.h"
#include "mplsh.h"
#include "802xx.h"

#if defined(HAVE_FLOW_MGR)
#include "flow.h"
#endif

/*Error codes for the thread modules*/
typedef enum {
    TM_ECODE_OK = 0,    /**< Thread module exits OK*/
    TM_ECODE_FAILED,    /**< Thread module exits due to failure*/
    TM_ECODE_DONE,    /**< Thread module task is finished*/
} TmEcode;

/*Given a packet pkt offset to the start of the ip header in a packet
 *We determine the ip version. */
#define IP_GET_RAW_VER(pkt) ((((pkt)[0] & 0xf0) >> 4))

#define PKT_IS_IPV4(p)      (((p)->ip4h != NULL))
#define PKT_IS_IPV6(p)      (((p)->ip6h != NULL))
#define PKT_IS_TCP(p)       (((p)->tcph != NULL))
#define PKT_IS_UDP(p)       (((p)->udph != NULL))
#define PKT_IS_ICMPV4(p)    (((p)->icmpv4h != NULL))
#define PKT_IS_ICMPV6(p)    (((p)->icmpv6h != NULL))

#define IPH_IS_VALID(p) (PKT_IS_IPV4((p)) || PKT_IS_IPV6((p)))

/* Retrieve proto regardless of IP version */
#define IP_GET_IPPROTO(p) \
    (p->proto ? p->proto : \
    (PKT_IS_IPV4((p))? IPV4_GET_IPPROTO((p)) : (PKT_IS_IPV6((p))? IPV6_GET_L4PROTO((p)) : 0)))

/** After processing an alert by the thresholding module, if at
 *  last it gets triggered, we might want to stick the drop action to
 *  the flow on IPS mode */
#define PACKET_ALERT_FLAG_DROP_FLOW     0x01
/** alert was generated based on state */
#define PACKET_ALERT_FLAG_STATE_MATCH   0x02
/** alert was generated based on stream */
#define PACKET_ALERT_FLAG_STREAM_MATCH  0x04
/** alert is in a tx, tx_id set */
#define PACKET_ALERT_FLAG_TX            0x08
/** action was changed by rate_filter */
#define PACKET_ALERT_RATE_FILTER_MODIFIED   0x10

#define PACKET_ALERT_MAX 15
#if 0
typedef struct PacketAlerts_ {
    uint16_t cnt;
    PacketAlert alerts[PACKET_ALERT_MAX];
    /* single pa used when we're dropping,
     * so we can log it out in the drop log. */
    PacketAlert drop;
} PacketAlerts;
#endif

/* Set the IPv4 addresses into the Addrs of the Packet.
 * Make sure p->ip4h is initialized and validated.
 *
 * We set the rest of the struct to 0 so we can
 * prevent using memset. */
#define SET_IPV4_SRC_ADDR(p, a) do {                              \
        (a)->family = AF_INET;                                    \
        (a)->addr_data32[0] = (uint32_t)(p)->ip4h->s_ip_src.s_addr; \
        (a)->addr_data32[1] = 0;                                  \
        (a)->addr_data32[2] = 0;                                  \
        (a)->addr_data32[3] = 0;                                  \
    } while (0)

#define SET_IPV4_DST_ADDR(p, a) do {                              \
        (a)->family = AF_INET;                                    \
        (a)->addr_data32[0] = (uint32_t)(p)->ip4h->s_ip_dst.s_addr; \
        (a)->addr_data32[1] = 0;                                  \
        (a)->addr_data32[2] = 0;                                  \
        (a)->addr_data32[3] = 0;                                  \
    } while (0)

/* Set the IPv6 addressesinto the Addrs of the Packet.
 * Make sure p->ip6h is initialized and validated. */
#define SET_IPV6_SRC_ADDR(p, a) do {                    \
        (a)->family = AF_INET6;                         \
        (a)->addr_data32[0] = (p)->ip6h->s_ip6_src[0];  \
        (a)->addr_data32[1] = (p)->ip6h->s_ip6_src[1];  \
        (a)->addr_data32[2] = (p)->ip6h->s_ip6_src[2];  \
        (a)->addr_data32[3] = (p)->ip6h->s_ip6_src[3];  \
    } while (0)

#define SET_IPV6_DST_ADDR(p, a) do {                    \
        (a)->family = AF_INET6;                         \
        (a)->addr_data32[0] = (p)->ip6h->s_ip6_dst[0];  \
        (a)->addr_data32[1] = (p)->ip6h->s_ip6_dst[1];  \
        (a)->addr_data32[2] = (p)->ip6h->s_ip6_dst[2];  \
        (a)->addr_data32[3] = (p)->ip6h->s_ip6_dst[3];  \
    } while (0)

/* Set the TCP ports into the Ports of the Packet.
 * Make sure p->tcph is initialized and validated. */
#define SET_TCP_SRC_PORT(pkt, prt) do {            \
        SET_PORT(TCP_GET_SRC_PORT((pkt)), *(prt)); \
    } while (0)

#define SET_TCP_DST_PORT(pkt, prt) do {            \
        SET_PORT(TCP_GET_DST_PORT((pkt)), *(prt)); \
    } while (0)

/* Set the UDP ports into the Ports of the Packet.
 * Make sure p->udph is initialized and validated. */
#define SET_UDP_SRC_PORT(pkt, prt) do {            \
        SET_PORT(UDP_GET_SRC_PORT((pkt)), *(prt)); \
    } while (0)
#define SET_UDP_DST_PORT(pkt, prt) do {            \
        SET_PORT(UDP_GET_DST_PORT((pkt)), *(prt)); \
    } while (0)

/* Set the SCTP ports into the Ports of the Packet.
 * Make sure p->sctph is initialized and validated. */
#define SET_SCTP_SRC_PORT(pkt, prt) do {            \
        SET_PORT(SCTP_GET_SRC_PORT((pkt)), *(prt)); \
    } while (0)

#define SET_SCTP_DST_PORT(pkt, prt) do {            \
        SET_PORT(SCTP_GET_DST_PORT((pkt)), *(prt)); \
    } while (0)

#define GET_IPV4_SRC_ADDR_U32(p) ((p)->src.addr_data32[0])
#define GET_IPV4_DST_ADDR_U32(p) ((p)->dst.addr_data32[0])
#define GET_IPV4_SRC_ADDR_PTR(p) ((p)->src.addr_data32)
#define GET_IPV4_DST_ADDR_PTR(p) ((p)->dst.addr_data32)

#define GET_IPV6_SRC_ADDR(p) ((p)->src.addr_data32)
#define GET_IPV6_DST_ADDR(p) ((p)->dst.addr_data32)
#define GET_TCP_SRC_PORT(p)  ((p)->sp)
#define GET_TCP_DST_PORT(p)  ((p)->dp)

#define GET_PKT_LEN(p) ((p)->pktlen)
#define GET_PKT(p) ((p)->pkt)
#define GET_PKT_DATA(p) ((((p)->ext_pkt) == NULL ) ? (uint8_t *)((p) + 1) : (p)->ext_pkt)
#define GET_PKT_DIRECT_DATA(p) (uint8_t *)((p) + 1)
#define GET_PKT_DIRECT_MAX_SIZE(p) (default_packet_size)

#define GET_PKT_SRC_PHY(p)	((p)->phy_port[QUA_RX])
#define GET_PKT_DST_PHY(p)	((p)->phy_port[QUA_TX])


#define SET_PKT_LEN(p, len) do { \
    (p)->pktlen = (len); \
    } while (0)

#define SET_PKT(p, pkt) do { \
    (p)->pkt = (pkt); \
    } while (0)

#define SET_PKT_PHY(p, phy, qua) do { \
	(p)->phy_port[(qua)] = (phy); \
	} while (0)

/** Set where the packet comes from (which port) */
#define SET_PKT_SRC_PHY(p, phy) SET_PKT_PHY(p,phy,QUA_RX)
/** Set where the packet send to (which port) */
#define SET_PKT_DST_PHY(p, phy) SET_PKT_PHY(p,phy,QUA_TX)


/** number of decoder events we support per packet. Power of 2 minus 1
 *  for memory layout */
#define PACKET_ENGINE_EVENT_MAX 15

/** data structure to store decoder, defrag and stream events */
typedef struct PacketEngineEvents_ {
    uint8_t cnt;                                /**< number of events */
    uint8_t events[PACKET_ENGINE_EVENT_MAX];   /**< array of events */
} PacketEngineEvents;

/*Packet Flags*/
#define PKT_NOPACKET_INSPECTION         (1)         /**< Flag to indicate that packet header or contents should not be inspected*/
#define PKT_NOPAYLOAD_INSPECTION        (1<<2)      /**< Flag to indicate that packet contents should not be inspected*/
#define PKT_ALLOC                       (1<<3)      /**< Packet was alloc'd this run, needs to be freed */
#define PKT_HAS_TAG                     (1<<4)      /**< Packet has matched a tag */
#define PKT_STREAM_ADD                  (1<<5)      /**< Packet payload was added to reassembled stream */
#define PKT_STREAM_EST                  (1<<6)      /**< Packet is part of establised stream */
#define PKT_STREAM_EOF                  (1<<7)      /**< Stream is in eof state */
#define PKT_HAS_FLOW                    (1<<8)
#define PKT_PSEUDO_STREAM_END           (1<<9)      /**< Pseudo packet to end the stream */
#define PKT_STREAM_MODIFIED             (1<<10)     /**< Packet is modified by the stream engine, we need to recalc the csum and reinject/replace */
#define PKT_MARK_MODIFIED               (1<<11)     /**< Packet mark is modified */
#define PKT_STREAM_NOPCAPLOG            (1<<12)     /**< Exclude packet from pcap logging as it's part of a stream that has reassembly depth reached. */

#define PKT_TUNNEL                      (1<<13)
#define PKT_TUNNEL_VERDICTED            (1<<14)

#define PKT_IGNORE_CHECKSUM             (1<<15)     /**< Packet checksum is not computed (TX packet for example) */
#define PKT_ZERO_COPY                   (1<<16)     /**< Packet comes from zero copy (ext_pkt must not be freed) */

#define PKT_HOST_SRC_LOOKED_UP          (1<<17)
#define PKT_HOST_DST_LOOKED_UP          (1<<18)

#define PKT_IS_FRAGMENT                 (1<<19)     /**< Packet is a fragment */
#define PKT_IS_INVALID                  (1<<20)
#define PKT_PROFILE                     (1<<21)

/** indication by decoder that it feels the packet should be handled by
 *  flow engine: Packet::flow_hash will be set */
#define PKT_WANTS_FLOW                  (1<<22)

/** protocol detection done */
#define PKT_PROTO_DETECT_TS_DONE        (1<<23)
#define PKT_PROTO_DETECT_TC_DONE        (1<<24)

#define PKT_REBUILT_FRAGMENT            (1<<25)     /**< Packet is rebuilt from
                                                     * fragments. */
#define PKT_DETECT_HAS_STREAMDATA       (1<<26)     /**< Set by Detect() if raw stream data is available. */

#define PKT_PSEUDO_DETECTLOG_FLUSH      (1<<27)     /**< Detect/log flush for protocol upgrade */


/** \brief return 1 if the packet is a pseudo packet */
#define PKT_IS_PSEUDOPKT(p) \
    ((p)->flags & (PKT_PSEUDO_STREAM_END|PKT_PSEUDO_DETECTLOG_FLUSH))

#define PKT_SET_SRC(p, src_val) ((p)->pkt_src = src_val)

#define DEFAULT_MTU 1500
#define MINIMUM_MTU 68      /**< ipv4 minimum: rfc791 */

#define DEFAULT_PACKET_SIZE (DEFAULT_MTU + ETHERNET_HEADER_LEN)

typedef struct Packet_
{
    /* Addresses, Ports and protocol
     * these are on top so we can use
     * the Packet as a hash key */
    Address src;
    Address dst;
    union {
        Port sp;
        uint8_t type;
    };
    union {
        Port dp;
        uint8_t code;
    };
    uint8_t proto;

	/** physical rx and tx port */
	u32 phy_port[QUA_RXTX];
	
    /* make sure we can't be attacked on when the tunneled packet
     * has the exact same tuple as the lower levels */
    uint8_t recursion_level;
	
    uint16_t vlan_id[2];
    uint8_t vlan_idx;

    /* Pkt Flags */
    uint32_t flags;

    /* raw hash value for looking up the flow, will need to modulated to the
     * hash size still */
    uint32_t flow_hash;

    struct timeval ts;

    /* pkt vars */
    PktVar *pktvar;

    /* header pointers */
    EthernetHdr *ethh;

	MarvellDSAHdr *dsah;
	/** ntoh32(this->dsah.dsa) */
	u32 dsa;

    IPV4Hdr *ip4h;
    IPV6Hdr *ip6h;
    TCPHdr *tcph;
    UDPHdr *udph;
    SCTPHdr *sctph;
    ICMPV4Hdr *icmpv4h;
    ICMPV6Hdr *icmpv6h;
    PPPHdr *ppph;
    PPPOESessionHdr *pppoesh;
    PPPOEDiscoveryHdr *pppoedh;
    GREHdr *greh;
    VLANHdr *vlanh[2];

	/* IPv4 and IPv6 are mutually exclusive */
	union {
		IPV4Vars ip4vars;
		struct {
			IPV6Vars ip6vars;
			IPV6ExtHdrs ip6eh;
		};
	};
	/* Can only be one of TCP, UDP, ICMP at any given time */
	union {
		TCPVars tcpvars;
		ICMPV4Vars icmpv4vars;
		ICMPV6Vars icmpv6vars;
	} l4vars;
#define tcpvars     l4vars.tcpvars
#define icmpv4vars  l4vars.icmpv4vars
#define icmpv6vars  l4vars.icmpv6vars

	struct Flow_ *flow;

    /* ptr to the payload of the packet
     * with it's length. */
    uint8_t *payload;
    uint16_t payload_len;
	
    /* IPS action to take */
    uint8_t action;

    /* Checksum for IP packets. */
    int32_t level3_comp_csum;
    /* Check sum for TCP, UDP or ICMP packets */
    int32_t level4_comp_csum;

    /* storage: set to pointer to heap and extended via allocation if necessary */
	uint8_t  *pkt;
    uint32_t pktlen;
    uint8_t *ext_pkt;
	void *iface[QUA_RXTX];	/** rx and tx iface for this packet. */

    /* engine events */
    PacketEngineEvents events;
}    __attribute__((aligned(128))) Packet;

#define PACKET_CLEAR_L4VARS(p) do {             \
    }while (0);

/**
 *  \brief reset these to -1(indicates that the packet is fresh from the queue)
 */
#define PACKET_RESET_CHECKSUMS(p) do {			\
	}while(0);

/* if p uses extended data, free them */
#define PACKET_FREE_EXTDATA(p) do {              \
	}while(0);


#define PACKET_INITIALIZE(p) {   \
    PACKET_RESET_CHECKSUMS((p)); \
}

#define PACKET_RELEASE_REFS(p) do {              \
	}while(0);


/**
 *  \brief Recycle a packet structure for reuse.
 */
#define PACKET_REINIT(p) do {             \
        CLEAR_ADDR(&(p)->src);                  \
        CLEAR_ADDR(&(p)->dst);                  \
        (p)->sp = 0;                            \
        (p)->dp = 0;                            \
        (p)->proto = 0;                         \
        (p)->recursion_level = 0;               \
        PACKET_FREE_EXTDATA((p));               \
        (p)->flags = (p)->flags & PKT_ALLOC;    \
        (p)->vlan_id[0] = 0;                    \
        (p)->vlan_id[1] = 0;                    \
        (p)->vlan_idx = 0;                      \
        (p)->ts.tv_sec = 0;                     \
        (p)->ts.tv_usec = 0;                    \
        (p)->action = 0;                        \
        (p)->ethh = NULL;                       \
        if ((p)->ip4h != NULL) {                \
            CLEAR_IPV4_PACKET((p));             \
        }                                       \
        if ((p)->ip6h != NULL) {                \
            CLEAR_IPV6_PACKET((p));             \
        }                                       \
        if ((p)->tcph != NULL) {                \
            CLEAR_TCP_PACKET((p));              \
        }                                       \
        if ((p)->udph != NULL) {                \
            CLEAR_UDP_PACKET((p));              \
        }                                       \
        if ((p)->sctph != NULL) {               \
            CLEAR_SCTP_PACKET((p));             \
        }                                       \
        if ((p)->icmpv4h != NULL) {             \
            CLEAR_ICMPV4_PACKET((p));           \
        }                                       \
        if ((p)->icmpv6h != NULL) {             \
            CLEAR_ICMPV6_PACKET((p));           \
        }                                       \
        (p)->ppph = NULL;                       \
        (p)->pppoesh = NULL;                    \
        (p)->pppoedh = NULL;                    \
        (p)->greh = NULL;                       \
        (p)->vlanh[0] = NULL;                   \
        (p)->vlanh[1] = NULL;                   \
        (p)->payload = NULL;                    \
        (p)->payload_len = 0;                   \
        (p)->pktlen = 0;                        \
        (p)->events.cnt = 0;                    \
        PACKET_RESET_CHECKSUMS((p));            \
    } while (0)

#define PACKET_RECYCLE(p) do { \
				PACKET_RELEASE_REFS((p)); \
				PACKET_REINIT((p)); \
			} while (0)

#define ENGINE_SET_EVENT(p, e) do { \
    if ((p)->events.cnt < PACKET_ENGINE_EVENT_MAX) { \
        (p)->events.events[(p)->events.cnt] = e; \
        (p)->events.cnt++; \
    } \
} while(0)

#define ENGINE_SET_INVALID_EVENT(p, e) do { \
    p->flags |= PKT_IS_INVALID; \
    ENGINE_SET_EVENT(p, e); \
} while(0)

#if defined(HAVE_DPDK)
#include "dpdk.h"

static inline Packet *
get_priv(struct rte_mbuf *m)
{
	return RTE_PTR_ADD(m, sizeof(struct rte_mbuf));
}

#endif

#include "dp_decode_eth.h"
#include "dp_decode_marvell_dsa.h"
#include "dp_decode_ipv4.h"
#include "dp_decode_ipv6.h"
#include "dp_decode_icmpv4.h"
#include "dp_decode_icmpv6.h"
#include "dp_decode_udp.h"
#include "dp_decode_tcp.h"
#include "dp_decode_sctp.h"
#include "dp_decode_gre.h"
#include "dp_decode_mpls.h"
#include "dp_decode_ppp.h"
#include "dp_decode_pppoe.h"
#include "dp_decode_vlan.h"
#include "dp_decode_arp.h"

typedef struct _dp_args_t {
	ThreadVars tv[MAX_LCORES];
	DecodeThreadVars dtv[MAX_LCORES];
	PacketQueue pq[MAX_LCORES];
}dp_private_t;

static inline void dump_pkt(uint8_t *pkt, int len)
{
	int i = 0;
	
	for (i = 0; i < len; i ++){
		if (!(i % 16))
			printf ("\n");
		printf("%02x ", pkt[i]);
	}
	printf ("\n");
}

extern Packet *PacketGetFromAlloc(void);
extern void PacketDecodeFinalize(ThreadVars *tv, DecodeThreadVars *dtv, Packet *p);
#endif
