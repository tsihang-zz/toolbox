#include "oryx.h"
#include "dp_decode.h"
#include "dp_flow.h"

/**
 * Note, this is the IP header, plus a bit of the original packet, not the whole thing!
 */
static int DecodePartialIPV4(Packet* p, uint8_t* partial_packet, uint16_t len)
{
    /** Check the sizes, the header must fit at least */
    if (len < IPV4_HEADER_LEN) {
        oryx_logd("DecodePartialIPV4: ICMPV4_IPV4_TRUNC_PKT");
        ENGINE_SET_INVALID_EVENT(p, ICMPV4_IPV4_TRUNC_PKT);
        return -1;
    }

    IPV4Hdr *icmp4_ip4h = (IPV4Hdr*)partial_packet;

    /** Check the embedded version */
    if (IPV4_GET_RAW_VER(icmp4_ip4h) != 4) {
        /** Check the embedded version */
        oryx_logd("DecodePartialIPV4: ICMPv4 contains Unknown IPV4 version "
                   "ICMPV4_IPV4_UNKNOWN_VER");
        ENGINE_SET_INVALID_EVENT(p, ICMPV4_IPV4_UNKNOWN_VER);
        return -1;
    }

    /** We need to fill icmpv4vars */
    p->icmpv4vars.emb_ipv4h = icmp4_ip4h;

    /** Get the IP address from the contained packet */
    p->icmpv4vars.emb_ip4_src = IPV4_GET_RAW_IPSRC(icmp4_ip4h);
    p->icmpv4vars.emb_ip4_dst = IPV4_GET_RAW_IPDST(icmp4_ip4h);

    p->icmpv4vars.emb_ip4_hlen=IPV4_GET_RAW_HLEN(icmp4_ip4h) << 2;

    switch (IPV4_GET_RAW_IPPROTO(icmp4_ip4h)) {
        case IPPROTO_TCP:
            if (len >= IPV4_HEADER_LEN + TCP_HEADER_LEN ) {
                p->icmpv4vars.emb_tcph = (TCPHdr*)(partial_packet + IPV4_HEADER_LEN);
                p->icmpv4vars.emb_sport = ntohs(p->icmpv4vars.emb_tcph->th_sport);
                p->icmpv4vars.emb_dport = ntohs(p->icmpv4vars.emb_tcph->th_dport);
                p->icmpv4vars.emb_ip4_proto = IPPROTO_TCP;

                oryx_logd("DecodePartialIPV4: ICMPV4->IPV4->TCP header sport: "
                           "%"PRIu8" dport %"PRIu8"", p->icmpv4vars.emb_sport,
                            p->icmpv4vars.emb_dport);
            } else if (len >= IPV4_HEADER_LEN + 4) {
                /* only access th_sport and th_dport */
                TCPHdr *emb_tcph = (TCPHdr*)(partial_packet + IPV4_HEADER_LEN);

                p->icmpv4vars.emb_tcph = NULL;
                p->icmpv4vars.emb_sport = ntohs(emb_tcph->th_sport);
                p->icmpv4vars.emb_dport = ntohs(emb_tcph->th_dport);
                p->icmpv4vars.emb_ip4_proto = IPPROTO_TCP;
                oryx_logd("DecodePartialIPV4: ICMPV4->IPV4->TCP partial header sport: "
                           "%"PRIu8" dport %"PRIu8"", p->icmpv4vars.emb_sport,
                            p->icmpv4vars.emb_dport);
            } else {
                oryx_logd("DecodePartialIPV4: Warning, ICMPV4->IPV4->TCP "
                           "header Didn't fit in the packet!");
                p->icmpv4vars.emb_sport = 0;
                p->icmpv4vars.emb_dport = 0;
            }

            break;
        case IPPROTO_UDP:
            if (len >= IPV4_HEADER_LEN + UDP_HEADER_LEN ) {
                p->icmpv4vars.emb_udph = (UDPHdr*)(partial_packet + IPV4_HEADER_LEN);
                p->icmpv4vars.emb_sport = ntohs(p->icmpv4vars.emb_udph->uh_sport);
                p->icmpv4vars.emb_dport = ntohs(p->icmpv4vars.emb_udph->uh_dport);
                p->icmpv4vars.emb_ip4_proto = IPPROTO_UDP;

                oryx_logd("DecodePartialIPV4: ICMPV4->IPV4->UDP header sport: "
                           "%"PRIu8" dport %"PRIu8"", p->icmpv4vars.emb_sport,
                            p->icmpv4vars.emb_dport);
            } else {
                oryx_logd("DecodePartialIPV4: Warning, ICMPV4->IPV4->UDP "
                           "header Didn't fit in the packet!");
                p->icmpv4vars.emb_sport = 0;
                p->icmpv4vars.emb_dport = 0;
            }

            break;
        case IPPROTO_ICMP:
            if (len >= IPV4_HEADER_LEN + ICMPV4_HEADER_LEN ) {
                p->icmpv4vars.emb_icmpv4h = (ICMPV4Hdr*)(partial_packet + IPV4_HEADER_LEN);
                p->icmpv4vars.emb_sport = 0;
                p->icmpv4vars.emb_dport = 0;
                p->icmpv4vars.emb_ip4_proto = IPPROTO_ICMP;

                oryx_logd("DecodePartialIPV4: ICMPV4->IPV4->ICMP header");
            }

            break;
    }

    /* debug print */
#ifdef DEBUG
    char s[16], d[16];
    PrintInet(AF_INET, &(p->icmpv4vars.emb_ip4_src), s, sizeof(s));
    PrintInet(AF_INET, &(p->icmpv4vars.emb_ip4_dst), d, sizeof(d));
    oryx_logd("ICMPv4 embedding IPV4 %s->%s - PROTO: %" PRIu32 " ID: %" PRIu32 "", s,d,
            IPV4_GET_RAW_IPPROTO(icmp4_ip4h), IPV4_GET_RAW_IPID(icmp4_ip4h));
#endif

    return 0;
}

/** DecodeICMPV4
 *  \brief Main ICMPv4 decoding function
 */
int DecodeICMPV40(ThreadVars *tv, DecodeThreadVars *dtv, Packet *p, uint8_t *pkt, uint16_t len, PacketQueue *pq)
{
    oryx_counter_inc(&tv->perf_private_ctx0, dtv->counter_icmpv4);
	
    if (len < ICMPV4_HEADER_LEN) {
        ENGINE_SET_INVALID_EVENT(p, ICMPV4_PKT_TOO_SMALL);
        return TM_ECODE_FAILED;
    }

    p->icmpv4h = (ICMPV4Hdr *)pkt;

    oryx_logd("ICMPV4 TYPE %" PRIu32 " CODE %" PRIu32 "", p->icmpv4h->type, p->icmpv4h->code);

    p->proto = IPPROTO_ICMP;
    p->type = p->icmpv4h->type;
    p->code = p->icmpv4h->code;
    p->payload = pkt + ICMPV4_HEADER_LEN;
    p->payload_len = len - ICMPV4_HEADER_LEN;

    ICMPV4ExtHdr* icmp4eh = (ICMPV4ExtHdr*) p->icmpv4h;

    switch (p->icmpv4h->type)
    {
        case ICMP_ECHOREPLY:
            p->icmpv4vars.id=icmp4eh->id;
            p->icmpv4vars.seq=icmp4eh->seq;
            if (p->icmpv4h->code!=0) {
                ENGINE_SET_EVENT(p,ICMPV4_UNKNOWN_CODE);
            }
            break;

        case ICMP_DEST_UNREACH:
            if (p->icmpv4h->code > NR_ICMP_UNREACH) {
                ENGINE_SET_EVENT(p,ICMPV4_UNKNOWN_CODE);
            } else {
                /* parse IP header plus 64 bytes */
                if (len > ICMPV4_HEADER_PKT_OFFSET) {
                    if (DecodePartialIPV4(p, (uint8_t *)(pkt + ICMPV4_HEADER_PKT_OFFSET),
                                             len - ICMPV4_HEADER_PKT_OFFSET ) == 0)
                    {
                        /* ICMP ICMP_DEST_UNREACH influence TCP/UDP flows */
                        if (ICMPV4_DEST_UNREACH_IS_VALID(p)) {
                            FlowSetupPacket(p);
                        }
                    }
                }
            }


            break;

        case ICMP_SOURCE_QUENCH:
            if (p->icmpv4h->code!=0) {
                ENGINE_SET_EVENT(p,ICMPV4_UNKNOWN_CODE);
            } else {
                // parse IP header plus 64 bytes
                if (len >= ICMPV4_HEADER_PKT_OFFSET)
                    DecodePartialIPV4( p, (uint8_t*) (pkt + ICMPV4_HEADER_PKT_OFFSET), len - ICMPV4_HEADER_PKT_OFFSET  );
            }
            break;

        case ICMP_REDIRECT:
            if (p->icmpv4h->code>ICMP_REDIR_HOSTTOS) {
                ENGINE_SET_EVENT(p,ICMPV4_UNKNOWN_CODE);
            } else {
                // parse IP header plus 64 bytes
                if (len > ICMPV4_HEADER_PKT_OFFSET)
                    DecodePartialIPV4( p, (uint8_t*) (pkt + ICMPV4_HEADER_PKT_OFFSET), len - ICMPV4_HEADER_PKT_OFFSET );
            }
            break;

        case ICMP_ECHO:
            p->icmpv4vars.id=icmp4eh->id;
            p->icmpv4vars.seq=icmp4eh->seq;
            if (p->icmpv4h->code!=0) {
                ENGINE_SET_EVENT(p,ICMPV4_UNKNOWN_CODE);
            }
            break;

        case ICMP_TIME_EXCEEDED:
            if (p->icmpv4h->code>ICMP_EXC_FRAGTIME) {
                ENGINE_SET_EVENT(p,ICMPV4_UNKNOWN_CODE);
            } else {
                // parse IP header plus 64 bytes
                if (len > ICMPV4_HEADER_PKT_OFFSET)
                    DecodePartialIPV4( p, (uint8_t*) (pkt + ICMPV4_HEADER_PKT_OFFSET), len - ICMPV4_HEADER_PKT_OFFSET );
            }
            break;

        case ICMP_PARAMETERPROB:
            if (p->icmpv4h->code!=0) {
                ENGINE_SET_EVENT(p,ICMPV4_UNKNOWN_CODE);
            } else {
                // parse IP header plus 64 bytes
                if (len > ICMPV4_HEADER_PKT_OFFSET)
                    DecodePartialIPV4( p, (uint8_t*) (pkt + ICMPV4_HEADER_PKT_OFFSET), len - ICMPV4_HEADER_PKT_OFFSET );
            }
            break;

        case ICMP_TIMESTAMP:
            p->icmpv4vars.id=icmp4eh->id;
            p->icmpv4vars.seq=icmp4eh->seq;
            if (p->icmpv4h->code!=0) {
                ENGINE_SET_EVENT(p,ICMPV4_UNKNOWN_CODE);
            }
            break;

        case ICMP_TIMESTAMPREPLY:
            p->icmpv4vars.id=icmp4eh->id;
            p->icmpv4vars.seq=icmp4eh->seq;
            if (p->icmpv4h->code!=0) {
                ENGINE_SET_EVENT(p,ICMPV4_UNKNOWN_CODE);
            }
            break;

        case ICMP_INFO_REQUEST:
            p->icmpv4vars.id=icmp4eh->id;
            p->icmpv4vars.seq=icmp4eh->seq;
            if (p->icmpv4h->code!=0) {
                ENGINE_SET_EVENT(p,ICMPV4_UNKNOWN_CODE);
            }
            break;

        case ICMP_INFO_REPLY:
            p->icmpv4vars.id=icmp4eh->id;
            p->icmpv4vars.seq=icmp4eh->seq;
            if (p->icmpv4h->code!=0) {
                ENGINE_SET_EVENT(p,ICMPV4_UNKNOWN_CODE);
            }
            break;

        case ICMP_ADDRESS:
            p->icmpv4vars.id=icmp4eh->id;
            p->icmpv4vars.seq=icmp4eh->seq;
            if (p->icmpv4h->code!=0) {
                ENGINE_SET_EVENT(p,ICMPV4_UNKNOWN_CODE);
            }
            break;

        case ICMP_ADDRESSREPLY:
            p->icmpv4vars.id=icmp4eh->id;
            p->icmpv4vars.seq=icmp4eh->seq;
            if (p->icmpv4h->code!=0) {
                ENGINE_SET_EVENT(p,ICMPV4_UNKNOWN_CODE);
            }
            break;

        default:
            ENGINE_SET_EVENT(p,ICMPV4_UNKNOWN_TYPE);

    }

    return TM_ECODE_OK;
}

