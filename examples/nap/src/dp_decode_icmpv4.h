#ifndef DP_DECODE_ICMPv4_H
#define DP_DECODE_ICMPv4_H

/** marco for icmpv4 type access */
#define ICMPV4_GET_TYPE(p)      (p)->icmpv4h->type
/** marco for icmpv4 code access */
#define ICMPV4_GET_CODE(p)      (p)->icmpv4h->code

#define CLEAR_ICMPV4_PACKET(p) do { \
    (p)->level4_comp_csum = -1;     \
    PACKET_CLEAR_L4VARS((p));       \
    (p)->icmpv4h = NULL;            \
} while(0)

#define ICMPV4_HEADER_PKT_OFFSET 8

/** macro for icmpv4 "type" access */
#define ICMPV4_GET_TYPE(p)      (p)->icmpv4h->type
/** macro for icmpv4 "code" access */
#define ICMPV4_GET_CODE(p)      (p)->icmpv4h->code
/** macro for icmpv4 "csum" access */
#define ICMPV4_GET_RAW_CSUM(p)  ntohs((p)->icmpv4h->checksum)
#define ICMPV4_GET_CSUM(p)      (p)->icmpv4h->checksum

/* If message is informational */

/** macro for icmpv4 "id" access */
#define ICMPV4_GET_ID(p)        ((p)->icmpv4vars.id)
/** macro for icmpv4 "seq" access */
#define ICMPV4_GET_SEQ(p)       ((p)->icmpv4vars.seq)

/* If message is Error */

/** macro for icmpv4 embedded "protocol" access */
#define ICMPV4_GET_EMB_PROTO(p)    (p)->icmpv4vars.emb_ip4_proto
/** macro for icmpv4 embedded "ipv4h" header access */
#define ICMPV4_GET_EMB_IPv4(p)     (p)->icmpv4vars.emb_ipv4h
/** macro for icmpv4 embedded "tcph" header access */
#define ICMPV4_GET_EMB_TCP(p)      (p)->icmpv4vars.emb_tcph
/** macro for icmpv4 embedded "udph" header access */
#define ICMPV4_GET_EMB_UDP(p)      (p)->icmpv4vars.emb_udph
/** macro for icmpv4 embedded "icmpv4h" header access */
#define ICMPV4_GET_EMB_ICMPV4H(p)  (p)->icmpv4vars.emb_icmpv4h

/** macro for checking if a ICMP DEST UNREACH packet is valid for use
 *  in other parts of the engine, such as the flow engine. 
 *
 *  \warning use only _after_ the decoder has processed the packet
 */
#define ICMPV4_DEST_UNREACH_IS_VALID(p) ( \
    (!((p)->flags & PKT_IS_INVALID)) && \
    ((p)->icmpv4h != NULL) && \
    (ICMPV4_GET_TYPE((p)) == ICMP_DEST_UNREACH) && \
    (ICMPV4_GET_EMB_IPv4((p)) != NULL) && \
    ((ICMPV4_GET_EMB_TCP((p)) != NULL) || \
     (ICMPV4_GET_EMB_UDP((p)) != NULL)))

/**
 *  marco for checking if a ICMP packet is an error message or an
 *  query message.
 *
 *  \todo This check is used in the flow engine and needs to be as
 *        cheap as possible. Consider setting a bitflag at the decoder
 *        stage so we can to a bit check instead of the more expensive
 *        check below.
 */
#define ICMPV4_IS_ERROR_MSG(p) (ICMPV4_GET_TYPE((p)) == ICMP_DEST_UNREACH || \
        ICMPV4_GET_TYPE((p)) == ICMP_SOURCE_QUENCH || \
        ICMPV4_GET_TYPE((p)) == ICMP_REDIRECT || \
        ICMPV4_GET_TYPE((p)) == ICMP_TIME_EXCEEDED || \
        ICMPV4_GET_TYPE((p)) == ICMP_PARAMETERPROB)


/**
 * \brief Calculates the checksum for the ICMP packet
 *
 * \param pkt  Pointer to the start of the ICMP packet
 * \param hlen Total length of the ICMP packet(header + payload)
 *
 * \retval csum Checksum for the ICMP packet
 */
static __oryx_always_inline__
uint16_t ICMPV4CalculateChecksum0(uint16_t *pkt, uint16_t tlen)
{
    uint16_t pad = 0;
    uint32_t csum = pkt[0];

    tlen -= 4;
    pkt += 2;

    while (tlen >= 32) {
        csum += pkt[0] + pkt[1] + pkt[2] + pkt[3] + pkt[4] + pkt[5] + pkt[6] +
            pkt[7] + pkt[8] + pkt[9] + pkt[10] + pkt[11] + pkt[12] + pkt[13] +
            pkt[14] + pkt[15];
        tlen -= 32;
        pkt += 16;
    }

    while(tlen >= 8) {
        csum += pkt[0] + pkt[1] + pkt[2] + pkt[3];
        tlen -= 8;
        pkt += 4;
    }

    while(tlen >= 4) {
        csum += pkt[0] + pkt[1];
        tlen -= 4;
        pkt += 2;
    }

    while (tlen > 1) {
        csum += pkt[0];
        tlen -= 2;
        pkt += 1;
    }

    if (tlen == 1) {
        *(uint8_t *)(&pad) = (*(uint8_t *)pkt);
        csum += pad;
    }

    csum = (csum >> 16) + (csum & 0x0000FFFF);
    csum += (csum >> 16);

    return (uint16_t) ~csum;
}

/**
 * Note, this is the IP header, plus a bit of the original packet, not the whole thing!
 */
static __oryx_always_inline__
int DecodePartialIPv4(packet_t* p, uint8_t* partial_packet, uint16_t len)
{
    /** Check the sizes, the header must fit at least */
    if (len < IPv4_HEADER_LEN) {
        oryx_logd("DecodePartialIPv4: ICMPV4_IPv4_TRUNC_PKT");
        ENGINE_SET_INVALID_EVENT(p, ICMPV4_IPv4_TRUNC_PKT);
        return -1;
    }

    IPv4Hdr *icmp4_ip4h = (IPv4Hdr*)partial_packet;

    /** Check the embedded version */
    if (IPv4_GET_RAW_VER(icmp4_ip4h) != 4) {
        /** Check the embedded version */
        oryx_logd("DecodePartialIPv4: ICMPv4 contains Unknown IPv4 version "
                   "ICMPV4_IPv4_UNKNOWN_VER");
        ENGINE_SET_INVALID_EVENT(p, ICMPV4_IPv4_UNKNOWN_VER);
        return -1;
    }

    /** We need to fill icmpv4vars */
    p->icmpv4vars.emb_ipv4h = icmp4_ip4h;

    /** Get the IP address from the contained packet */
    p->icmpv4vars.emb_ip4_src = IPv4_GET_RAW_IPSRC(icmp4_ip4h);
    p->icmpv4vars.emb_ip4_dst = IPv4_GET_RAW_IPDST(icmp4_ip4h);

    p->icmpv4vars.emb_ip4_hlen=IPv4_GET_RAW_HLEN(icmp4_ip4h) << 2;

    switch (IPv4_GET_RAW_IPPROTO(icmp4_ip4h)) {
        case IPPROTO_TCP:
            if (len >= IPv4_HEADER_LEN + TCP_HEADER_LEN ) {
                p->icmpv4vars.emb_tcph = (TCPHdr*)(partial_packet + IPv4_HEADER_LEN);
                p->icmpv4vars.emb_sport = ntohs(p->icmpv4vars.emb_tcph->th_sport);
                p->icmpv4vars.emb_dport = ntohs(p->icmpv4vars.emb_tcph->th_dport);
                p->icmpv4vars.emb_ip4_proto = IPPROTO_TCP;

                oryx_logd("DecodePartialIPv4: ICMPV4->IPv4->TCP header sport: "
                           "%"PRIu8" dport %"PRIu8"", p->icmpv4vars.emb_sport,
                            p->icmpv4vars.emb_dport);
            } else if (len >= IPv4_HEADER_LEN + 4) {
                /* only access th_sport and th_dport */
                TCPHdr *emb_tcph = (TCPHdr*)(partial_packet + IPv4_HEADER_LEN);

                p->icmpv4vars.emb_tcph = NULL;
                p->icmpv4vars.emb_sport = ntohs(emb_tcph->th_sport);
                p->icmpv4vars.emb_dport = ntohs(emb_tcph->th_dport);
                p->icmpv4vars.emb_ip4_proto = IPPROTO_TCP;
                oryx_logd("DecodePartialIPv4: ICMPV4->IPv4->TCP partial header sport: "
                           "%"PRIu8" dport %"PRIu8"", p->icmpv4vars.emb_sport,
                            p->icmpv4vars.emb_dport);
            } else {
                oryx_logd("DecodePartialIPv4: Warning, ICMPV4->IPv4->TCP "
                           "header Didn't fit in the packet!");
                p->icmpv4vars.emb_sport = 0;
                p->icmpv4vars.emb_dport = 0;
            }

            break;
        case IPPROTO_UDP:
            if (len >= IPv4_HEADER_LEN + UDP_HEADER_LEN ) {
                p->icmpv4vars.emb_udph = (UDPHdr*)(partial_packet + IPv4_HEADER_LEN);
                p->icmpv4vars.emb_sport = ntohs(p->icmpv4vars.emb_udph->uh_sport);
                p->icmpv4vars.emb_dport = ntohs(p->icmpv4vars.emb_udph->uh_dport);
                p->icmpv4vars.emb_ip4_proto = IPPROTO_UDP;

                oryx_logd("DecodePartialIPv4: ICMPV4->IPv4->UDP header sport: "
                           "%"PRIu8" dport %"PRIu8"", p->icmpv4vars.emb_sport,
                            p->icmpv4vars.emb_dport);
            } else {
                oryx_logd("DecodePartialIPv4: Warning, ICMPV4->IPv4->UDP "
                           "header Didn't fit in the packet!");
                p->icmpv4vars.emb_sport = 0;
                p->icmpv4vars.emb_dport = 0;
            }

            break;
        case IPPROTO_ICMP:
            if (len >= IPv4_HEADER_LEN + ICMPV4_HEADER_LEN ) {
                p->icmpv4vars.emb_icmpv4h = (ICMPV4Hdr*)(partial_packet + IPv4_HEADER_LEN);
                p->icmpv4vars.emb_sport = 0;
                p->icmpv4vars.emb_dport = 0;
                p->icmpv4vars.emb_ip4_proto = IPPROTO_ICMP;

                oryx_logd("DecodePartialIPv4: ICMPV4->IPv4->ICMP header");
            }

            break;
    }

    /* debug print */
#if defined(BUILD_DEBUG)
    char s[16], d[16];
    PrintInet(AF_INET, &(p->icmpv4vars.emb_ip4_src), s, sizeof(s));
    PrintInet(AF_INET, &(p->icmpv4vars.emb_ip4_dst), d, sizeof(d));
    oryx_logd("ICMPv4 embedding IPv4 %s->%s - PROTO: %" PRIu32 " ID: %" PRIu32 "", s,d,
            IPv4_GET_RAW_IPPROTO(icmp4_ip4h), IPv4_GET_RAW_IPID(icmp4_ip4h));
#endif

    return 0;
}

/** DecodeICMPV4
 *  \brief Main ICMPv4 decoding function
 */
static __oryx_always_inline__
int DecodeICMPv40(threadvar_ctx_t *tv, decode_threadvar_ctx_t *dtv, packet_t *p, uint8_t *pkt, uint16_t len, PacketQueue *pq)
{
	oryx_logd("ICMPv4");
	
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
                    if (DecodePartialIPv4(p, (uint8_t *)(pkt + ICMPV4_HEADER_PKT_OFFSET),
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
                    DecodePartialIPv4( p, (uint8_t*) (pkt + ICMPV4_HEADER_PKT_OFFSET), len - ICMPV4_HEADER_PKT_OFFSET  );
            }
            break;

        case ICMP_REDIRECT:
            if (p->icmpv4h->code>ICMP_REDIR_HOSTTOS) {
                ENGINE_SET_EVENT(p,ICMPV4_UNKNOWN_CODE);
            } else {
                // parse IP header plus 64 bytes
                if (len > ICMPV4_HEADER_PKT_OFFSET)
                    DecodePartialIPv4( p, (uint8_t*) (pkt + ICMPV4_HEADER_PKT_OFFSET), len - ICMPV4_HEADER_PKT_OFFSET );
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
                    DecodePartialIPv4( p, (uint8_t*) (pkt + ICMPV4_HEADER_PKT_OFFSET), len - ICMPV4_HEADER_PKT_OFFSET );
            }
            break;

        case ICMP_PARAMETERPROB:
            if (p->icmpv4h->code!=0) {
                ENGINE_SET_EVENT(p,ICMPV4_UNKNOWN_CODE);
            } else {
                // parse IP header plus 64 bytes
                if (len > ICMPV4_HEADER_PKT_OFFSET)
                    DecodePartialIPv4( p, (uint8_t*) (pkt + ICMPV4_HEADER_PKT_OFFSET), len - ICMPV4_HEADER_PKT_OFFSET );
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

#endif

