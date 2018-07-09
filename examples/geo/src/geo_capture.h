#ifndef GEO_CAPTURE_H
#define GEO_CAPTURE_H

#if !defined(HAVE_PCAP)
struct pcap_pkthdr {
	struct timeval ts;
	bpf_u_int32 caplen;
	bpf_u_int32 len;
};
#endif

struct ethhdr_t {
    uint8_t eth_dst[6];
    uint8_t eth_src[6];
    uint16_t eth_type;
} __attribute__((__packed__));

struct ipv4hdr_t {
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
} __attribute__((__packed__));
#define IPv4_GET_RAW_VER(ip4h)            (((ip4h)->ip_verhl & 0xf0) >> 4)
#define IPv4_GET_RAW_HLEN(ip4h)           ((ip4h)->ip_verhl & 0x0f)
#define IPv4_GET_RAW_IPTOS(ip4h)          ((ip4h)->ip_tos)
#define IPv4_GET_RAW_IPLEN(ip4h)          ((ip4h)->ip_len)
#define IPv4_GET_RAW_IPID(ip4h)           ((ip4h)->ip_id)
#define IPv4_GET_RAW_IPOFFSET(ip4h)       ((ip4h)->ip_off)
#define IPv4_GET_RAW_IPTTL(ip4h)          ((ip4h)->ip_ttl)
#define IPv4_GET_RAW_IPPROTO(ip4h)        ((ip4h)->ip_proto)
#define IPv4_GET_RAW_IPSRC(ip4h)          ((ip4h)->s_ip_src)
#define IPv4_GET_RAW_IPDST(ip4h)          ((ip4h)->s_ip_dst)

/* UDP header structure */
struct udphdr_t {
	uint16_t uh_sport;  /* source port */
	uint16_t uh_dport;  /* destination port */
	uint16_t uh_len;    /* length */
	uint16_t uh_sum;    /* checksum */
} __attribute__((__packed__));
#define UDP_GET_RAW_LEN(udph)                ntohs((udph)->uh_len)
#define UDP_GET_RAW_SRC_PORT(udph)           ntohs((udph)->uh_sport)
#define UDP_GET_RAW_DST_PORT(udph)           ntohs((udph)->uh_dport)
#define UDP_GET_RAW_SUM(udph)                ntohs((udph)->uh_sum)


typedef struct GEODecodeThreadVars_s {

	int cdr_index;

	/** stats/counters */
	uint64_t	counter_pkts;
	uint64_t	counter_bytes;
	uint64_t	counter_invalid;
	uint64_t	counter_no_mtmsi;
	uint64_t	counter_mtmsi;

	uint64_t	counter_no_mme_ue_s1ap_id;
	uint64_t	counter_mme_ue_s1ap_id;


	uint64_t	counter_has_imsi;						/* packet with imsi */
	uint64_t	counter_no_imsi;						/* packet without imsi */

	uint64_t	counter_has_imsi_mme_ue_s1apid;			/* both imsi and mme_ue_s1apid */
	uint64_t	counter_has_imsi_mtmsi;					/* both imsi and mtmis */
	
	uint64_t	counter_refilled_deta;
	uint64_t	counter_total_refill;					/* after refill with mtmsi */

	uint64_t	counter_bypassed;
	
}GEODecodeThreadVars;

typedef struct GEOThreadVars_ {
    pthread_t t;
	u32 lcore;
    char name[16];
    char *printable_name;
    char *thread_group_name;

    /** local id */
    int id;

    /* counters for this thread. */
	struct CounterCtx perf_private_ctx0;

	/** free function for this thread to free a packet. */
	int (*free_fn)(void *);
	
	struct GEOThreadVars_ *next;
    struct GEOThreadVars_ *prev;

	/** decode ops. */
	void *d_ops;
}GEOThreadVars;

static __oryx_always_inline__
void dump_imsi(const char *comment, char *imsi)
{
	int i;

	fprintf (stdout, "%s\n", comment);
	
	for (i = 0; i < 18; i ++) {
		fprintf (stdout, "%02x-", imsi[i]);
	}
	fprintf (stdout, "%s", "\n");
}

extern void geo_start_pcap(void);
extern void geo_end_pcap(void);

#endif
