#include "oryx.h"
#include "dp_decode.h"
#include "dp_flow.h"


/**
 * \brief Get variables and do some checks of the embedded IPV6 packet
 *
 * \param p Pointer to the packet we are filling
 * \param partial_packet  Pointer to the raw packet buffer
 * \param len the len of the rest of the packet not processed yet
 *
 * \retval void No return value
 */
static void DecodePartialIPV6(Packet *p, uint8_t *partial_packet, uint16_t len )
{
    /** Check the sizes, the header must fit at least */
    if (len < IPV6_HEADER_LEN) {
        oryx_logd("ICMPV6_IPV6_TRUNC_PKT");
        ENGINE_SET_INVALID_EVENT(p, ICMPV6_IPV6_TRUNC_PKT);
        return;
    }

    IPV6Hdr *icmp6_ip6h = (IPV6Hdr*)partial_packet;

    /** Check the embedded version */
    if(((icmp6_ip6h->s_ip6_vfc & 0xf0) >> 4) != 6)
    {
        oryx_logd("ICMPv6 contains Unknown IPV6 version "
                "ICMPV6_IPV6_UNKNOWN_VER");
        ENGINE_SET_INVALID_EVENT(p, ICMPV6_IPV6_UNKNOWN_VER);
        return;
    }

    /** We need to fill icmpv6vars */
    p->icmpv6vars.emb_ipv6h = icmp6_ip6h;

    /** Get the IP6 address */
    p->icmpv6vars.emb_ip6_src[0] = icmp6_ip6h->s_ip6_src[0];
    p->icmpv6vars.emb_ip6_src[1] = icmp6_ip6h->s_ip6_src[1];
    p->icmpv6vars.emb_ip6_src[2] = icmp6_ip6h->s_ip6_src[2];
    p->icmpv6vars.emb_ip6_src[3] = icmp6_ip6h->s_ip6_src[3];

    p->icmpv6vars.emb_ip6_dst[0] = icmp6_ip6h->s_ip6_dst[0];
    p->icmpv6vars.emb_ip6_dst[1] = icmp6_ip6h->s_ip6_dst[1];
    p->icmpv6vars.emb_ip6_dst[2] = icmp6_ip6h->s_ip6_dst[2];
    p->icmpv6vars.emb_ip6_dst[3] = icmp6_ip6h->s_ip6_dst[3];

    /** Get protocol and ports inside the embedded ipv6 packet and set the pointers */
    p->icmpv6vars.emb_ip6_proto_next = icmp6_ip6h->s_ip6_nxt;

    switch (icmp6_ip6h->s_ip6_nxt) {
        case IPPROTO_TCP:
            if (len >= IPV6_HEADER_LEN + TCP_HEADER_LEN ) {
                p->icmpv6vars.emb_tcph = (TCPHdr*)(partial_packet + IPV6_HEADER_LEN);
                p->icmpv6vars.emb_sport = p->icmpv6vars.emb_tcph->th_sport;
                p->icmpv6vars.emb_dport = p->icmpv6vars.emb_tcph->th_dport;

                oryx_logd("ICMPV6->IPV6->TCP header sport: "
                           "%"PRIu8" dport %"PRIu8"", p->icmpv6vars.emb_sport,
                            p->icmpv6vars.emb_dport);
            } else {
                oryx_logd("Warning, ICMPV6->IPV6->TCP "
                           "header Didn't fit in the packet!");
                p->icmpv6vars.emb_sport = 0;
                p->icmpv6vars.emb_dport = 0;
            }

            break;
        case IPPROTO_UDP:
            if (len >= IPV6_HEADER_LEN + UDP_HEADER_LEN ) {
                p->icmpv6vars.emb_udph = (UDPHdr*)(partial_packet + IPV6_HEADER_LEN);
                p->icmpv6vars.emb_sport = p->icmpv6vars.emb_udph->uh_sport;
                p->icmpv6vars.emb_dport = p->icmpv6vars.emb_udph->uh_dport;

                oryx_logd("ICMPV6->IPV6->UDP header sport: "
                           "%"PRIu8" dport %"PRIu8"", p->icmpv6vars.emb_sport,
                            p->icmpv6vars.emb_dport);
            } else {
                oryx_logd("Warning, ICMPV6->IPV6->UDP "
                           "header Didn't fit in the packet!");
                p->icmpv6vars.emb_sport = 0;
                p->icmpv6vars.emb_dport = 0;
            }

            break;
        case IPPROTO_ICMPV6:
            p->icmpv6vars.emb_icmpv6h = (ICMPV6Hdr*)(partial_packet + IPV6_HEADER_LEN);
            p->icmpv6vars.emb_sport = 0;
            p->icmpv6vars.emb_dport = 0;

            oryx_logd("ICMPV6->IPV6->ICMP header");

            break;
    }

    /* debug print */
#if defined(BUILD_DEBUG)
    char s[46], d[46];
    PrintInet(AF_INET6, (const void *)p->icmpv6vars.emb_ip6_src, s, sizeof(s));
    PrintInet(AF_INET6, (const void *)p->icmpv6vars.emb_ip6_dst, d, sizeof(d));
    oryx_logd("ICMPv6 embedding IPV6 %s->%s - CLASS: %" PRIu32 " FLOW: "
               "%" PRIu32 " NH: %" PRIu32 " PLEN: %" PRIu32 " HLIM: %" PRIu32,
               s, d, IPV6_GET_RAW_CLASS(icmp6_ip6h), IPV6_GET_RAW_FLOW(icmp6_ip6h),
               IPV6_GET_RAW_NH(icmp6_ip6h), IPV6_GET_RAW_PLEN(icmp6_ip6h), IPV6_GET_RAW_HLIM(icmp6_ip6h));
#endif

    return;
}

/**
 * \brief Decode ICMPV6 packets and fill the Packet with the decoded info
 *
 * \param tv Pointer to the thread variables
 * \param dtv Pointer to the decode thread variables
 * \param p Pointer to the packet we are filling
 * \param pkt Pointer to the raw packet buffer
 * \param len the len of the rest of the packet not processed yet
 * \param pq the packet queue were this packet go
 *
 * \retval void No return value
 */
int DecodeICMPV60(ThreadVars *tv, DecodeThreadVars *dtv, Packet *p,
                  uint8_t *pkt, uint16_t len, PacketQueue *pq)
{
    int full_hdr = 0;

    oryx_counter_inc(&tv->perf_private_ctx0, dtv->counter_icmpv6);

    if (len < ICMPV6_HEADER_LEN) {
        oryx_logd("ICMPV6_PKT_TOO_SMALL");
        ENGINE_SET_INVALID_EVENT(p, ICMPV6_PKT_TOO_SMALL);
        return TM_ECODE_FAILED;
    }

    p->icmpv6h = (ICMPV6Hdr *)pkt;
    p->proto = IPPROTO_ICMPV6;
    p->type = p->icmpv6h->type;
    p->code = p->icmpv6h->code;
    p->payload_len = len - ICMPV6_HEADER_LEN;
    p->payload = pkt + ICMPV6_HEADER_LEN;

    oryx_logd("ICMPV6 TYPE %" PRIu32 " CODE %" PRIu32 "", p->icmpv6h->type,
               p->icmpv6h->code);

    switch (ICMPV6_GET_TYPE(p)) {
        case ICMP6_DST_UNREACH:
            oryx_logd("ICMP6_DST_UNREACH");

            if (ICMPV6_GET_CODE(p) > ICMP6_DST_UNREACH_REJECTROUTE) {
                ENGINE_SET_EVENT(p, ICMPV6_UNKNOWN_CODE);
            } else {
                DecodePartialIPV6(p, (uint8_t*) (pkt + ICMPV6_HEADER_LEN),
                                  len - ICMPV6_HEADER_LEN );
                full_hdr = 1;
            }

            break;
        case ICMP6_PACKET_TOO_BIG:
            oryx_logd("ICMP6_PACKET_TOO_BIG");

            if (ICMPV6_GET_CODE(p) != 0) {
                ENGINE_SET_EVENT(p, ICMPV6_UNKNOWN_CODE);
            } else {
                p->icmpv6vars.mtu = ICMPV6_GET_MTU(p);
                DecodePartialIPV6(p, (uint8_t*) (pkt + ICMPV6_HEADER_LEN),
                                  len - ICMPV6_HEADER_LEN );
                full_hdr = 1;
            }

            break;
        case ICMP6_TIME_EXCEEDED:
            oryx_logd("ICMP6_TIME_EXCEEDED");

            if (ICMPV6_GET_CODE(p) > ICMP6_TIME_EXCEED_REASSEMBLY) {
                ENGINE_SET_EVENT(p, ICMPV6_UNKNOWN_CODE);
            } else {
                DecodePartialIPV6(p, (uint8_t*) (pkt + ICMPV6_HEADER_LEN),
                                  len - ICMPV6_HEADER_LEN );
                full_hdr = 1;
            }

            break;
        case ICMP6_PARAM_PROB:
            oryx_logd("ICMP6_PARAM_PROB");

            if (ICMPV6_GET_CODE(p) > ICMP6_PARAMPROB_OPTION) {
                ENGINE_SET_EVENT(p, ICMPV6_UNKNOWN_CODE);
            } else {
                p->icmpv6vars.error_ptr= ICMPV6_GET_ERROR_PTR(p);
                DecodePartialIPV6(p, (uint8_t*) (pkt + ICMPV6_HEADER_LEN),
                                  len - ICMPV6_HEADER_LEN );
                full_hdr = 1;
            }

            break;
        case ICMP6_ECHO_REQUEST:
            oryx_logd("ICMP6_ECHO_REQUEST id: %u seq: %u",
                       p->icmpv6h->icmpv6b.icmpv6i.id, p->icmpv6h->icmpv6b.icmpv6i.seq);

            if (ICMPV6_GET_CODE(p) != 0) {
                ENGINE_SET_EVENT(p, ICMPV6_UNKNOWN_CODE);
            } else {
                p->icmpv6vars.id = p->icmpv6h->icmpv6b.icmpv6i.id;
                p->icmpv6vars.seq = p->icmpv6h->icmpv6b.icmpv6i.seq;
                full_hdr = 1;
            }

            break;
        case ICMP6_ECHO_REPLY:
            oryx_logd("ICMP6_ECHO_REPLY id: %u seq: %u",
                       p->icmpv6h->icmpv6b.icmpv6i.id, p->icmpv6h->icmpv6b.icmpv6i.seq);

            if (ICMPV6_GET_CODE(p) != 0) {
                ENGINE_SET_EVENT(p, ICMPV6_UNKNOWN_CODE);
            } else {
                p->icmpv6vars.id = p->icmpv6h->icmpv6b.icmpv6i.id;
                p->icmpv6vars.seq = p->icmpv6h->icmpv6b.icmpv6i.seq;
                full_hdr = 1;
            }

            break;
        case ND_ROUTER_SOLICIT:
            oryx_logd("ND_ROUTER_SOLICIT");
            if (ICMPV6_GET_CODE(p) != 0) {
                ENGINE_SET_EVENT(p, ICMPV6_UNKNOWN_CODE);
            }
            break;
        case ND_ROUTER_ADVERT:
            oryx_logd("ND_ROUTER_ADVERT");
            if (ICMPV6_GET_CODE(p) != 0) {
                ENGINE_SET_EVENT(p, ICMPV6_UNKNOWN_CODE);
            }
            break;
        case ND_NEIGHBOR_SOLICIT:
            oryx_logd("ND_NEIGHBOR_SOLICIT");
            if (ICMPV6_GET_CODE(p) != 0) {
                ENGINE_SET_EVENT(p, ICMPV6_UNKNOWN_CODE);
            }
            break;
        case ND_NEIGHBOR_ADVERT:
            oryx_logd("ND_NEIGHBOR_ADVERT");
            if (ICMPV6_GET_CODE(p) != 0) {
                ENGINE_SET_EVENT(p, ICMPV6_UNKNOWN_CODE);
            }
            break;
        case ND_REDIRECT:
            oryx_logd("ND_REDIRECT");
            if (ICMPV6_GET_CODE(p) != 0) {
                ENGINE_SET_EVENT(p, ICMPV6_UNKNOWN_CODE);
            }
            break;
        case MLD_LISTENER_QUERY:
            oryx_logd("MLD_LISTENER_QUERY");
            if (ICMPV6_GET_CODE(p) != 0) {
                ENGINE_SET_EVENT(p, ICMPV6_UNKNOWN_CODE);
            }
            if (IPV6_GET_HLIM(p) != 1) {
                ENGINE_SET_EVENT(p, ICMPV6_MLD_MESSAGE_WITH_INVALID_HL);
            }
            break;
        case MLD_LISTENER_REPORT:
            oryx_logd("MLD_LISTENER_REPORT");
            if (ICMPV6_GET_CODE(p) != 0) {
                ENGINE_SET_EVENT(p, ICMPV6_UNKNOWN_CODE);
            }
            if (IPV6_GET_HLIM(p) != 1) {
                ENGINE_SET_EVENT(p, ICMPV6_MLD_MESSAGE_WITH_INVALID_HL);
            }
            break;
        case MLD_LISTENER_REDUCTION:
            oryx_logd("MLD_LISTENER_REDUCTION");
            if (ICMPV6_GET_CODE(p) != 0) {
                ENGINE_SET_EVENT(p, ICMPV6_UNKNOWN_CODE);
            }
            if (IPV6_GET_HLIM(p) != 1) {
                ENGINE_SET_EVENT(p, ICMPV6_MLD_MESSAGE_WITH_INVALID_HL);
            }
            break;
        case ICMP6_RR:
            oryx_logd("ICMP6_RR");
            if (ICMPV6_GET_CODE(p) > 2 && ICMPV6_GET_CODE(p) != 255) {
                ENGINE_SET_EVENT(p, ICMPV6_UNKNOWN_CODE);
            }
            break;
        case ICMP6_NI_QUERY:
            oryx_logd("ICMP6_NI_QUERY");
            if (ICMPV6_GET_CODE(p) > 2) {
                ENGINE_SET_EVENT(p, ICMPV6_UNKNOWN_CODE);
            }
            break;
        case ICMP6_NI_REPLY:
            oryx_logd("ICMP6_NI_REPLY");
            if (ICMPV6_GET_CODE(p) > 2) {
                ENGINE_SET_EVENT(p, ICMPV6_UNKNOWN_CODE);
            }
            break;
        case ND_INVERSE_SOLICIT:
            oryx_logd("ND_INVERSE_SOLICIT");
            if (ICMPV6_GET_CODE(p) != 0) {
                ENGINE_SET_EVENT(p, ICMPV6_UNKNOWN_CODE);
            }
            break;
        case ND_INVERSE_ADVERT:
            oryx_logd("ND_INVERSE_ADVERT");
            if (ICMPV6_GET_CODE(p) != 0) {
                ENGINE_SET_EVENT(p, ICMPV6_UNKNOWN_CODE);
            }
            break;
        case MLD_V2_LIST_REPORT:
            oryx_logd("MLD_V2_LIST_REPORT");
            if (ICMPV6_GET_CODE(p) != 0) {
                ENGINE_SET_EVENT(p, ICMPV6_UNKNOWN_CODE);
            }
            break;
        case HOME_AGENT_AD_REQUEST:
            oryx_logd("HOME_AGENT_AD_REQUEST");
            if (ICMPV6_GET_CODE(p) != 0) {
                ENGINE_SET_EVENT(p, ICMPV6_UNKNOWN_CODE);
            }
            break;
        case HOME_AGENT_AD_REPLY:
            oryx_logd("HOME_AGENT_AD_REPLY");
            if (ICMPV6_GET_CODE(p) != 0) {
                ENGINE_SET_EVENT(p, ICMPV6_UNKNOWN_CODE);
            }
            break;
        case MOBILE_PREFIX_SOLICIT:
            oryx_logd("MOBILE_PREFIX_SOLICIT");
            if (ICMPV6_GET_CODE(p) != 0) {
                ENGINE_SET_EVENT(p, ICMPV6_UNKNOWN_CODE);
            }
            break;
        case MOBILE_PREFIX_ADVERT:
            oryx_logd("MOBILE_PREFIX_ADVERT");
            if (ICMPV6_GET_CODE(p) != 0) {
                ENGINE_SET_EVENT(p, ICMPV6_UNKNOWN_CODE);
            }
            break;
        case CERT_PATH_SOLICIT:
            oryx_logd("CERT_PATH_SOLICIT");
            if (ICMPV6_GET_CODE(p) != 0) {
                ENGINE_SET_EVENT(p, ICMPV6_UNKNOWN_CODE);
            }
            break;
        case CERT_PATH_ADVERT:
            oryx_logd("CERT_PATH_ADVERT");
            if (ICMPV6_GET_CODE(p) != 0) {
                ENGINE_SET_EVENT(p, ICMPV6_UNKNOWN_CODE);
            }
            break;
        case ICMP6_MOBILE_EXPERIMENTAL:
            oryx_logd("ICMP6_MOBILE_EXPERIMENTAL");
            break;
        case MC_ROUTER_ADVERT:
            oryx_logd("MC_ROUTER_ADVERT");
            break;
        case MC_ROUTER_SOLICIT:
            oryx_logd("MC_ROUTER_SOLICIT");
            break;
        case MC_ROUTER_TERMINATE:
            oryx_logd("MC_ROUTER_TERMINATE");
            break;
        case FMIPV6_MSG:
            oryx_logd("FMIPV6_MSG");
            if (ICMPV6_GET_CODE(p) != 0) {
                ENGINE_SET_EVENT(p, ICMPV6_UNKNOWN_CODE);
            }
            break;
        case RPL_CONTROL_MSG:
            oryx_logd("RPL_CONTROL_MSG");
            if (ICMPV6_GET_CODE(p) > 3 && ICMPV6_GET_CODE(p) < 128) {
                ENGINE_SET_EVENT(p, ICMPV6_UNKNOWN_CODE);
            }
            if (ICMPV6_GET_CODE(p) > 132) {
                ENGINE_SET_EVENT(p, ICMPV6_UNKNOWN_CODE);
            }
            break;
        case LOCATOR_UDATE_MSG:
            oryx_logd("LOCATOR_UDATE_MSG");
            if (ICMPV6_GET_CODE(p) != 0) {
                ENGINE_SET_EVENT(p, ICMPV6_UNKNOWN_CODE);
            }
            break;
        case DUPL_ADDR_REQUEST:
            oryx_logd("DUPL_ADDR_REQUEST");
            if (ICMPV6_GET_CODE(p) != 0) {
                ENGINE_SET_EVENT(p, ICMPV6_UNKNOWN_CODE);
            }
            break;
        case DUPL_ADDR_CONFIRM:
            oryx_logd("DUPL_ADDR_CONFIRM");
            if (ICMPV6_GET_CODE(p) != 0) {
                ENGINE_SET_EVENT(p, ICMPV6_UNKNOWN_CODE);
            }
            break;
        case MPL_CONTROL_MSG:
            oryx_logd("MPL_CONTROL_MSG");
            if (ICMPV6_GET_CODE(p) != 0) {
                ENGINE_SET_EVENT(p, ICMPV6_UNKNOWN_CODE);
            }
            break;
        default:
            /* Various range taken from:
             *   http://www.iana.org/assignments/icmpv6-parameters/icmpv6-parameters.xhtml#icmpv6-parameters-2
             */
            if ((ICMPV6_GET_TYPE(p) > 4) &&  (ICMPV6_GET_TYPE(p) < 100)) {
                ENGINE_SET_EVENT(p, ICMPV6_UNASSIGNED_TYPE);
            } else if ((ICMPV6_GET_TYPE(p) >= 100) &&  (ICMPV6_GET_TYPE(p) < 102)) {
                ENGINE_SET_EVENT(p, ICMPV6_EXPERIMENTATION_TYPE);
            } else  if ((ICMPV6_GET_TYPE(p) >= 102) &&  (ICMPV6_GET_TYPE(p) < 127)) {
                ENGINE_SET_EVENT(p, ICMPV6_UNASSIGNED_TYPE);
            } else if ((ICMPV6_GET_TYPE(p) >= 160) &&  (ICMPV6_GET_TYPE(p) < 200)) {
                ENGINE_SET_EVENT(p, ICMPV6_UNASSIGNED_TYPE);
            } else if ((ICMPV6_GET_TYPE(p) >= 200) &&  (ICMPV6_GET_TYPE(p) < 202)) {
                ENGINE_SET_EVENT(p, ICMPV6_EXPERIMENTATION_TYPE);
            } else if (ICMPV6_GET_TYPE(p) >= 202) {
                ENGINE_SET_EVENT(p, ICMPV6_UNASSIGNED_TYPE);
            } else {
                oryx_logd("ICMPV6 Message type %" PRIu8 " not "
                        "implemented yet", ICMPV6_GET_TYPE(p));
                ENGINE_SET_EVENT(p, ICMPV6_UNKNOWN_TYPE);
            }
    }

    /* for a info message the header is just 4 bytes */
    if (!full_hdr) {
        if (p->payload_len >= 4) {
            p->payload_len -= 4;
            p->payload = pkt + 4;
        } else {
            p->payload_len = 0;
            p->payload = NULL;
        }
    }

#if defined(BUILD_DEBUG)
    if (ENGINE_ISSET_EVENT(p, ICMPV6_UNKNOWN_CODE))
        oryx_logd("Unknown Code, ICMPV6_UNKNOWN_CODE");

    if (ENGINE_ISSET_EVENT(p, ICMPV6_UNKNOWN_TYPE))
        oryx_logd("Unknown Type, ICMPV6_UNKNOWN_TYPE");
#endif

    FlowSetupPacket(p);

    return TM_ECODE_OK;
}


