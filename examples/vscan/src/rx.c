#include "oryx.h"
#include "ethh.h"
#include "iph.h"
#include "tcph.h"
#include "udph.h"
#include "sctph.h"
#include "http.h"

#include "mpm.h"

#if !defined(HAVE_PCAP)
struct pcap_pkthdr {
	struct timeval ts;  
	bpf_u_int32 caplen; 
	bpf_u_int32 len;    
};
#endif

struct cdr_scghdr_t {                          
        uint16_t        len;                   
        uint16_t        msg_type;              
        uint16_t        seq_id;                
        uint16_t        reserved;              
        uint32_t        offset;                
};                                             
                                               
                                               
struct cdr_hdr_t {                             
        struct cdr_scghdr_t     scg;           
        uint16_t                total_len;     
        uint16_t                table_id;      
        uint16_t                service_detail;
        uint16_t                policy_id;     
        uint64_t                start_time;    
        uint64_t                cdr_id;        
        uint8_t                 device_id;     
        uint8_t                 filter_flag;   
        uint8_t                 data_type;     
        uint8_t                 cpu_clock_mul; 
        uint8_t                 reserved1[4];  
};                                             

extern mpm_ctx_t mpm_ctx;
extern mpm_threadctx_t mpm_thread_ctx;
extern PrefilterRuleStore pmq;

extern volatile bool force_quit;
int http_match(mpm_ctx_t *mpm_ctx, mpm_threadctx_t *mpm_thread_ctx, PrefilterRuleStore *pmq, struct http_keyval_t *v);

extern oryx_file_t *fp;
static uint32_t refcnt_all = 0, refcnt_tcp = 0, refcnt_udp = 0, refcnt_http = 0;

struct class_count_t {
	uint32_t	pkt_tcp;
	uint32_t	pkt_udp;
};

static __oryx_always_inline__
void dump_pkt(uint8_t *pkt, int len)
{
	int i = 0;
	
	for (i = 0; i < len; i ++){
		if (!(i % 16))
			fprintf (stdout, "\n");
		fprintf (stdout, "%02x ", pkt[i]);
	}
	fprintf (stdout, "\n");
}

static cdr_information(const char *buf, size_t buflen)
{
	struct cdr_hdr_t *h = (struct cdr_hdr_t *)buf;
	fprintf(stdout, "%16s%12u\n", "scg.len:",		ntoh16(h->scg.len));
	fprintf(stdout, "%16s%12u\n", "scg.msg_type:",	ntoh16(h->scg.msg_type));
	fprintf(stdout, "%16s%12u\n", "scg.seq_id:",	ntoh16(h->scg.seq_id));
	fprintf(stdout, "%16s%12u\n", "scg.offset:",	ntoh32(h->scg.offset));
	fprintf(stdout, "%16s%12u\n", "total_len:",		ntoh16(h->total_len));
	fprintf(stdout, "%16s%12u\n", "table_id:",		ntoh16(h->table_id));
	fprintf(stdout, "%16s%12u\n", "start_time:",	ntoh64(h->start_time));
	fprintf(stdout, "%16s%12u\n", "device_id:",		(h->device_id));
}

static void rx_pkt_handler(u_char *user, const struct pcap_pkthdr *h,
                                   		const u_char *bytes)
{
	IPv4Hdr *ip4h;
	IPv6Hdr *ip6h;
	UDPHdr *udph;
	TCPHdr *tcph;
	SCTPHdr *sctph;
	char *pkt = bytes;

	refcnt_all ++;
	
	if(h->caplen < ETHERNET_HEADER_LEN) return;

	/*
	*  Figure out which layer 2 protocol the frame belongs to and call
	*  the corresponding decoding module.  The protocol field of an 
	*  Ethernet II header is the 13th + 14th byte.	This is an endian
	*  independent way of extracting a big endian short from memory.  We
	*  extract the first byte and make it the big byte and then extract
	*  the next byte and make it the small byte.
	*/
	int offset = 12;
	if((pkt[12] << 0x08 | pkt[13]) == 0x8100)
		offset += 4;

	uint16_t eth_type = (pkt[offset] << 0x08 | pkt[offset + 1]);
	ip4h = ip6h = NULL;
	switch(eth_type) {
		case ETHERNET_TYPE_IP:
			ip4h = (IPv4Hdr *)(void *)&pkt[offset + 2];
			break;
		case ETHERNET_TYPE_IPv6:
			ip6h = (IPv6AuthHdr *)(void *)&pkt[offset + 2];
		default:
			return;
	}

	offset += 2;
	
	uint16_t plen = 0;
	uint16_t iphl = 0;
	void *next_proto = NULL;
	uint16_t sp = 0, dp = 0;
	
	if(ip4h) {
		iphl = (IPv4_GET_RAW_HLEN(ip4h) << 2);
		next_proto = (void *)((char *)ip4h + iphl);
		offset += iphl;
		
		switch (IPv4_GET_RAW_IPPROTO(ip4h)) {
			case IPPROTO_TCP:
				tcph = (TCPHdr *)next_proto;
				plen = ntohs(IPv4_GET_RAW_IPLEN(ip4h)) - iphl;
				
				sp = ntohs(tcph->th_sport);
				dp = ntohs(tcph->th_dport);
				offset += TCP_HEADER_LEN;
				refcnt_tcp ++;
				break;
				
			case IPPROTO_UDP:
				udph = (UDPHdr *)next_proto;
				plen = ntohs(IPv4_GET_RAW_IPLEN(ip4h)) - iphl;
				sp = ntohs(udph->uh_sport);
				dp = ntohs(udph->uh_dport);
				offset += UDP_HEADER_LEN;
				refcnt_udp ++;
				break;
				
			case IPPROTO_SCTP:
				sctph = (SCTPHdr *)next_proto;
				plen = ntohs(IPv4_GET_RAW_IPLEN(ip4h)) - iphl;
				sp = ntohs(sctph->sh_sport);
				dp = ntohs(sctph->sh_dport);
				offset += SCTP_HEADER_LEN;
				break;
			
			default:
				return;
		}

		cdr_information(&pkt[offset], 0);
		return;
		
		if((sp == 80 || dp == 80)) {
			refcnt_http ++;
			size_t http_len = plen;
			char *http_start = (char *)&pkt[offset];
			struct http_ctx_t ctx;
			if (!http_parse(&ctx, http_start, http_len)) {
				if (ctx.method == HTTP_METHOD_GET &&
					http_match(&mpm_ctx, &mpm_thread_ctx, &pmq, &ctx.uri)) {
					__http_kv_dump("Hit URI", &ctx.uri);
				} else {
					if (http_match(&mpm_ctx, &mpm_thread_ctx, &pmq, &ctx.ct))
						__http_kv_dump("Hit Content-Type", &ctx.ct);
				}
				//http_dump(&ctx, http_start, http_len);
			}
#if 0
			fprintf (stdout,
				"%u(%u), eth_type %04x, iphl %02x, iphp %02x, sp %u, dp %u, plen %u\n",
				h->caplen, h->len, eth_type, ip4h->ip_verhl, ip4h->ip_proto, sp, dp, plen);
#endif
		}
	}
	
	return;
}

static void perftmr_handler(struct oryx_timer_t *tmr, int __oryx_unused_param__ argc, 
				char __oryx_unused_param__**argv)
{
	fprintf (stdout, "refcnt_all %u, refcnt_tcp %u, refcnt_udp %u, refcnt_http %u\n",
		refcnt_all, refcnt_tcp, refcnt_udp, refcnt_http);
}

static void *rx_fn(void *argv)
{
	int64_t rank_acc;
	struct netdev_t *netdev = (struct netdev_t *)argv;
	
	atomic64_set(&netdev->rank, 0);

	netdev_open(netdev);

	struct oryx_timer_t *tm = oryx_tmr_create (1, "perftmr", (TMR_OPTIONS_PERIODIC | TMR_OPTIONS_ADVANCED),
											  perftmr_handler, 0, NULL, 3000);
	oryx_tmr_start(tm);

	FOREVER {
		if (force_quit)
			break;
		
		if (!netdev->handler)
			continue;
		
		rank_acc = pcap_dispatch(netdev->handler,
			1024, 
			netdev->pcap_handler, 
			(u_char *)netdev);

		if (rank_acc >= 0) {
			atomic64_add (&netdev->rank, rank_acc);
		} else {
			pcap_close (netdev->handler);
			netdev->handler = NULL;
			switch (rank_acc) {
				case -1:
				case -2:
				case -3:
				default:
					fprintf (stdout, "pcap_dispatch=%ld\n", rank_acc);
					break;
			}
		}
	}

	oryx_task_deregistry_id(pthread_self());
	return NULL;
}

static struct netdev_t rx_netdev = {
	.handler		= NULL,
	.devname		= "lo",
	.pcap_handler	= rx_pkt_handler,
	.private		= NULL,
};

struct oryx_task_t rx_netdev_task =
{
	.module = THIS,
	.sc_alias = "Netdev Task",
	.fn_handler = rx_fn,
	.lcore_mask = INVALID_CORE,
	.ul_prio = KERNEL_SCHED,
	.argc = 0,
	.argv = &rx_netdev,
	.ul_flags = 0,	/** Can not be recyclable. */
};

