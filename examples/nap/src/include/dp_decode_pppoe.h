#ifndef DP_DECODE_PPPoE_H
#define DP_DECODE_PPPoE_H


/**
 * \brief Main decoding function for PPPOE Discovery packets
 */
static __oryx_always_inline__
int DecodePPPoEDiscovery0(threadvar_ctx_t *tv, decode_threadvar_ctx_t *dtv, packet_t *p, uint8_t *pkt, uint16_t len, PacketQueue *pq)
{
	oryx_logd("PPPoE_discovery");

    oryx_counter_inc(&tv->perf_private_ctx0, dtv->counter_pppoe);

    if (len < PPPOE_DISCOVERY_HEADER_MIN_LEN) {
        ENGINE_SET_INVALID_EVENT(p, PPPOE_PKT_TOO_SMALL);
        return TM_ECODE_FAILED;
    }

    p->pppoedh = (PPPOEDiscoveryHdr *)pkt;
    if (p->pppoedh == NULL)
        return TM_ECODE_FAILED;

    /* parse the PPPOE code */
    switch (p->pppoedh->pppoe_code)
    {
        case  PPPOE_CODE_PADI:
            break;
        case  PPPOE_CODE_PADO:
            break;
        case  PPPOE_CODE_PADR:
            break;
        case PPPOE_CODE_PADS:
            break;
        case PPPOE_CODE_PADT:
            break;
        default:
            oryx_logd("unknown PPPOE code: 0x%0"PRIX8"", p->pppoedh->pppoe_code);
            ENGINE_SET_INVALID_EVENT(p, PPPOE_WRONG_CODE);
            return TM_ECODE_OK;
    }

    /* parse any tags we have in the packet */

    uint16_t tag_length = 0;
    PPPOEDiscoveryTag* pppoedt = (PPPOEDiscoveryTag*) (p->pppoedh +  PPPOE_DISCOVERY_HEADER_MIN_LEN);

    uint16_t pppoe_length = ntohs(p->pppoedh->pppoe_length);
    uint16_t packet_length = len - PPPOE_DISCOVERY_HEADER_MIN_LEN ;

    oryx_logd("pppoe_length %"PRIu16", packet_length %"PRIu16"",
        pppoe_length, packet_length);

    if (pppoe_length > packet_length) {
        oryx_logd("malformed PPPOE tags");
        ENGINE_SET_INVALID_EVENT(p, PPPOE_MALFORMED_TAGS);
        return TM_ECODE_OK;
    }

    while (pppoedt < (PPPOEDiscoveryTag*) (pkt + (len - sizeof(PPPOEDiscoveryTag))) && pppoe_length >=4 && packet_length >=4)
    {
        tag_length = ntohs(pppoedt->pppoe_tag_length);

        oryx_logd ("PPPoE Tag type %x, length %u", ntohs(pppoedt->pppoe_tag_type), tag_length);

        if (pppoe_length >= (4 + tag_length)) {
            pppoe_length -= (4 + tag_length);
        } else {
            pppoe_length = 0; // don't want an underflow
        }

        if (packet_length >= 4 + tag_length) {
            packet_length -= (4 + tag_length);
        } else {
            packet_length = 0; // don't want an underflow
        }

        pppoedt = pppoedt + (4 + tag_length);
    }

    return TM_ECODE_OK;
}

/**
 * \brief Main decoding function for PPPOE Session packets
 */
static __oryx_always_inline__
int DecodePPPoESession0(threadvar_ctx_t *tv, decode_threadvar_ctx_t *dtv, packet_t *p, uint8_t *pkt, uint16_t len, PacketQueue *pq)
{
	oryx_logd("PPPoE_session");

    oryx_counter_inc(&tv->perf_private_ctx0, dtv->counter_pppoe);
    if (len < PPPOE_SESSION_HEADER_LEN) {
        ENGINE_SET_INVALID_EVENT(p, PPPOE_PKT_TOO_SMALL);
        return TM_ECODE_FAILED;
    }

    p->pppoesh = (PPPOESessionHdr *)pkt;
    if (p->pppoesh == NULL)
        return TM_ECODE_FAILED;

    oryx_logd("PPPOE VERSION %" PRIu32 " TYPE %" PRIu32 " CODE %" PRIu32 " SESSIONID %" PRIu32 " LENGTH %" PRIu32 "",
           PPPOE_SESSION_GET_VERSION(p->pppoesh),  PPPOE_SESSION_GET_TYPE(p->pppoesh),  p->pppoesh->pppoe_code,  ntohs(p->pppoesh->session_id),  ntohs(p->pppoesh->pppoe_length));

    /* can't use DecodePPP() here because we only get a single 2-byte word to indicate protocol instead of the full PPP header */

    if (ntohs(p->pppoesh->pppoe_length) > 0) {
        /* decode contained PPP packet */

        switch (ntohs(p->pppoesh->protocol))
        {
            case PPP_VJ_COMP:
            case PPP_IPX:
            case PPP_OSI:
            case PPP_NS:
            case PPP_DECNET:
            case PPP_APPLE:
            case PPP_BRPDU:
            case PPP_STII:
            case PPP_VINES:
            case PPP_HELLO:
            case PPP_LUXCOM:
            case PPP_SNS:
            case PPP_MPLS_UCAST:
            case PPP_MPLS_MCAST:
            case PPP_IPCP:
            case PPP_OSICP:
            case PPP_NSCP:
            case PPP_DECNETCP:
            case PPP_APPLECP:
            case PPP_IPXCP:
            case PPP_STIICP:
            case PPP_VINESCP:
            case PPP_IPv6CP:
            case PPP_MPLSCP:
            case PPP_LCP:
            case PPP_PAP:
            case PPP_LQM:
            case PPP_CHAP:
                ENGINE_SET_EVENT(p,PPP_UNSUP_PROTO);
                break;

            case PPP_VJ_UCOMP:

                if(len < (PPPOE_SESSION_HEADER_LEN + IPv4_HEADER_LEN))    {
                    ENGINE_SET_INVALID_EVENT(p, PPPVJU_PKT_TOO_SMALL);
                    return TM_ECODE_OK;
                }

                if(IPv4_GET_RAW_VER((IPv4Hdr *)(pkt + PPPOE_SESSION_HEADER_LEN)) == 4) {
                    DecodeIPv40(tv, dtv, p, pkt + PPPOE_SESSION_HEADER_LEN, len - PPPOE_SESSION_HEADER_LEN, pq );
                }
                break;

            case PPP_IP:
                if(len < (PPPOE_SESSION_HEADER_LEN + IPv4_HEADER_LEN))    {
                    ENGINE_SET_INVALID_EVENT(p, PPPIPv4_PKT_TOO_SMALL);
                    return TM_ECODE_OK;
                }

                DecodeIPv40(tv, dtv, p, pkt + PPPOE_SESSION_HEADER_LEN, len - PPPOE_SESSION_HEADER_LEN, pq );
                break;

            /* PPP IPv6 was not tested */
            case PPP_IPv6:
                if(len < (PPPOE_SESSION_HEADER_LEN + IPv6_HEADER_LEN))    {
                    ENGINE_SET_INVALID_EVENT(p, PPPIPv6_PKT_TOO_SMALL);
                    return TM_ECODE_OK;
                }

                DecodeIPv60(tv, dtv, p, pkt + PPPOE_SESSION_HEADER_LEN, len - PPPOE_SESSION_HEADER_LEN, pq );
                break;

            default:
                oryx_logd("unknown PPP protocol: %" PRIx32 "",ntohs(p->ppph->protocol));
                ENGINE_SET_INVALID_EVENT(p, PPP_WRONG_TYPE);
                return TM_ECODE_OK;
        }
    }
    return TM_ECODE_OK;
}

#endif

