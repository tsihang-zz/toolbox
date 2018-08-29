#ifndef DP_DECODE_IPv6_H
#define DP_DECODE_IPv6_H

#include "iph.h"

#define IPv6_SET_L4PROTO(p,proto)       (p)->ip6vars.l4proto = proto

/* XXX */
#define IPv6_GET_L4PROTO(p) \
    ((p)->ip6vars.l4proto)

/** \brief get the highest proto/next header field we know */
//#define IPv6_GET_UPPER_PROTO(p)         (p)->ip6eh.ip6_exthdrs_cnt ?
//    (p)->ip6eh.ip6_exthdrs[(p)->ip6eh.ip6_exthdrs_cnt - 1].next : IPv6_GET_NH((p))

#define CLEAR_IPv6_PACKET(p) do { \
    (p)->ip6h = NULL; \
} while (0)

#define IPv6_EXTHDR_SET_FH(p)       (p)->ip6eh.fh_set = TRUE
#define IPv6_EXTHDR_ISSET_FH(p)     (p)->ip6eh.fh_set
#define IPv6_EXTHDR_SET_RH(p)       (p)->ip6eh.rh_set = TRUE
#define IPv6_EXTHDR_ISSET_RH(p)     (p)->ip6eh.rh_set


#define IPv6_EXTHDRS     ip6eh.ip6_exthdrs
#define IPv6_EH_CNT      ip6eh.ip6_exthdrs_cnt

/**
 * \brief Function to decode IPv4 in IPv6 packets
 *
 */
static __oryx_always_inline__
void DecodeIPv4inIPv6(threadvar_ctx_t *tv, decode_threadvar_ctx_t *dtv, packet_t *p, uint8_t *pkt, uint16_t plen, PacketQueue *pq)
{
	oryx_logd("IPv4InIPv6");
	
    if (unlikely(plen < IPv4_HEADER_LEN)) {
        ENGINE_SET_INVALID_EVENT(p, IPv4_IN_IPv6_PKT_TOO_SMALL);
        return;
    }
#if 0
    if (IP_GET_RAW_VER(pkt) == 4) {
        if (pq != NULL) {
            packet_t *tp = PacketTunnelPktSetup(tv, dtv, p, pkt, plen, DECODE_TUNNEL_IPv4, pq);
            if (tp != NULL) {
                PKT_SET_SRC(tp, PKT_SRC_DECODER_IPv6);
                /* add the tp to the packet queue. */
                PacketEnqueue(pq,tp);
                oryx_counter_inc(&tv->perf_private_ctx0, dtv->counter_ipv4inipv6);
                return;
            }
        }
    } else
#endif
	{
        ENGINE_SET_EVENT(p, IPv4_IN_IPv6_WRONG_IP_VER);
    }
    return;
}

/**
 * \brief Function to decode IPv6 in IPv6 packets
 *
 */
static __oryx_always_inline__
int DecodeIPv6inIPv6(threadvar_ctx_t *tv, decode_threadvar_ctx_t *dtv, packet_t *p, uint8_t *pkt, uint16_t plen, PacketQueue *pq)
{
	oryx_logd("IPv6InIPv6");

    if (unlikely(plen < IPv6_HEADER_LEN)) {
        ENGINE_SET_INVALID_EVENT(p, IPv6_IN_IPv6_PKT_TOO_SMALL);
        return TM_ECODE_FAILED;
    }
#if 0
    if (IP_GET_RAW_VER(pkt) == 6) {
        if (unlikely(pq != NULL)) {
            packet_t *tp = PacketTunnelPktSetup(tv, dtv, p, pkt, plen, DECODE_TUNNEL_IPv6, pq);
            if (tp != NULL) {
                PKT_SET_SRC(tp, PKT_SRC_DECODER_IPv6);
                PacketEnqueue(pq,tp);
                oryx_counter_inc(&tv->perf_private_ctx0, dtv->counter_ipv6inipv6);
            }
        }
    } else
   #endif
   {
        ENGINE_SET_EVENT(p, IPv6_IN_IPv6_WRONG_IP_VER);
    }
    return TM_ECODE_OK;
}

static __oryx_always_inline__
void DecodeIPv6FragHeader(packet_t *p, uint8_t *pkt,
                          uint16_t hdrextlen, uint16_t plen,
                          uint16_t prev_hdrextlen)
{
    uint16_t frag_offset = (*(pkt + 2) << 8 | *(pkt + 3)) & 0xFFF8;
    int frag_morefrags   = (*(pkt + 2) << 8 | *(pkt + 3)) & 0x0001;

    p->ip6eh.fh_offset = frag_offset;
    p->ip6eh.fh_more_frags_set = frag_morefrags ? TRUE : FALSE;
    p->ip6eh.fh_nh = *pkt;

    uint32_t fh_id;
    memcpy(&fh_id, pkt+4, 4);
    p->ip6eh.fh_id = ntohl(fh_id);

    oryx_logd("IPv6 FH: offset %u, mf %s, nh %u, id %u/%x",
            p->ip6eh.fh_offset,
            p->ip6eh.fh_more_frags_set ? "true" : "false",
            p->ip6eh.fh_nh,
            p->ip6eh.fh_id, p->ip6eh.fh_id);

    // store header offset, data offset
    uint16_t frag_hdr_offset = (uint16_t)(pkt - GET_PKT_DATA(p));
    uint16_t data_offset = (uint16_t)(frag_hdr_offset + hdrextlen);
    uint16_t data_len = plen - hdrextlen;

    p->ip6eh.fh_header_offset = frag_hdr_offset;
    p->ip6eh.fh_data_offset = data_offset;
    p->ip6eh.fh_data_len = data_len;

    /* if we have a prev hdr, store the type and offset of it */
    if (prev_hdrextlen) {
        p->ip6eh.fh_prev_hdr_offset = frag_hdr_offset - prev_hdrextlen;
    }

    oryx_logd("IPv6 FH: frag_hdr_offset %u, data_offset %u, data_len %u",
            p->ip6eh.fh_header_offset, p->ip6eh.fh_data_offset,
            p->ip6eh.fh_data_len);
}

static __oryx_always_inline__ 
void DecodeIPv6ExtHdrs(threadvar_ctx_t *tv, decode_threadvar_ctx_t *dtv, packet_t *p, uint8_t *pkt, uint16_t len, PacketQueue *pq)
{
    uint8_t *orig_pkt = pkt;
    uint8_t nh = 0; /* careful, 0 is actually a real type */
    uint16_t hdrextlen = 0;
    uint16_t plen;
    char dstopts = 0;
    char exthdr_fh_done = 0;
    int hh = 0;
    int rh = 0;
    int eh = 0;
    int ah = 0;

    nh = IPv6_GET_NH(p);
    plen = len;

    while(1)
    {
        /* No upper layer, but we do have data. Suspicious. */
        if (nh == IPPROTO_NONE && plen > 0) {
            ENGINE_SET_EVENT(p, IPv6_DATA_AFTER_NONE_HEADER);
            return;
        }

        if (plen < 2) { /* minimal needed in a hdr */
            return;
        }

        switch(nh)
        {
            case IPPROTO_TCP:
                IPv6_SET_L4PROTO(p,nh);
                DecodeTCP0(tv, dtv, p, pkt, plen, pq);
                return;

            case IPPROTO_UDP:
                IPv6_SET_L4PROTO(p,nh);
                DecodeUDP0(tv, dtv, p, pkt, plen, pq);
                return;

            case IPPROTO_ICMPV6:
                IPv6_SET_L4PROTO(p,nh);
                DecodeICMPv60(tv, dtv, p, pkt, plen, pq);
                return;

            case IPPROTO_SCTP:
                IPv6_SET_L4PROTO(p,nh);
                DecodeSCTP0(tv, dtv, p, pkt, plen, pq);
                return;

            case IPPROTO_ROUTING:
                IPv6_SET_L4PROTO(p,nh);
                hdrextlen = 8 + (*(pkt+1) * 8);  /* 8 bytes + length in 8 octet units */

                oryx_logd("hdrextlen %"PRIu8, hdrextlen);

                if (hdrextlen > plen) {
                    ENGINE_SET_EVENT(p, IPv6_TRUNC_EXTHDR);
                    return;
                }

                if (rh) {
                    ENGINE_SET_EVENT(p, IPv6_EXTHDR_DUPL_RH);
                    /* skip past this extension so we can continue parsing the rest
                     * of the packet */
                    nh = *pkt;
                    pkt += hdrextlen;
                    plen -= hdrextlen;
                    break;
                }

                rh = 1;
                IPv6_EXTHDR_SET_RH(p);

                uint8_t ip6rh_type = *(pkt + 2);
                if (ip6rh_type == 0) {
                    ENGINE_SET_EVENT(p, IPv6_EXTHDR_RH_TYPE_0);
                }
                p->ip6eh.rh_type = ip6rh_type;

                nh = *pkt;
                pkt += hdrextlen;
                plen -= hdrextlen;
                break;

            case IPPROTO_HOPOPTS:
            case IPPROTO_DSTOPTS:
            {
                IPv6OptHAO hao_s, *hao = &hao_s;
                IPv6OptRA ra_s, *ra = &ra_s;
                IPv6OptJumbo jumbo_s, *jumbo = &jumbo_s;
                uint16_t optslen = 0;

                IPv6_SET_L4PROTO(p,nh);
                hdrextlen =  (*(pkt+1) + 1) << 3;
                if (hdrextlen > plen) {
                    ENGINE_SET_EVENT(p, IPv6_TRUNC_EXTHDR);
                    return;
                }

                uint8_t *ptr = pkt + 2; /* +2 to go past nxthdr and len */

                /* point the pointers to right structures
                 * in packet_t. */
                if (nh == IPPROTO_HOPOPTS) {
                    if (hh) {
                        ENGINE_SET_EVENT(p, IPv6_EXTHDR_DUPL_HH);
                        /* skip past this extension so we can continue parsing the rest
                         * of the packet */
                        nh = *pkt;
                        pkt += hdrextlen;
                        plen -= hdrextlen;
                        break;
                    }

                    hh = 1;

                    optslen = ((*(pkt + 1) + 1 ) << 3) - 2;
                }
                else if (nh == IPPROTO_DSTOPTS)
                {
                    if (dstopts == 0) {
                        optslen = ((*(pkt + 1) + 1 ) << 3) - 2;
                        dstopts = 1;
                    } else if (dstopts == 1) {
                        optslen = ((*(pkt + 1) + 1 ) << 3) - 2;
                        dstopts = 2;
                    } else {
                        ENGINE_SET_EVENT(p, IPv6_EXTHDR_DUPL_DH);
                        /* skip past this extension so we can continue parsing the rest
                         * of the packet */
                        nh = *pkt;
                        pkt += hdrextlen;
                        plen -= hdrextlen;
                        break;
                    }
                }

                if (optslen > plen) {
                    /* since the packet is long enough (we checked
                     * plen against hdrlen, the optlen must be malformed. */
                    ENGINE_SET_EVENT(p, IPv6_EXTHDR_INVALID_OPTLEN);
                    /* skip past this extension so we can continue parsing the rest
                     * of the packet */
                    nh = *pkt;
                    pkt += hdrextlen;
                    plen -= hdrextlen;
                    break;
                }
/** \todo move into own function to loaded on demand */
                uint16_t padn_cnt = 0;
                uint16_t other_cnt = 0;
                uint16_t offset = 0;
                while(offset < optslen)
                {
                    if (*ptr == IPv6OPT_PAD1)
                    {
                        padn_cnt++;
                        offset++;
                        ptr++;
                        continue;
                    }

                    if (offset + 1 >= optslen) {
                        ENGINE_SET_EVENT(p, IPv6_EXTHDR_INVALID_OPTLEN);
                        break;
                    }

                    /* length field for each opt */
                    uint8_t ip6_optlen = *(ptr + 1);

                    /* see if the optlen from the packet fits the total optslen */
                    if ((offset + 1 + ip6_optlen) > optslen) {
                        ENGINE_SET_EVENT(p, IPv6_EXTHDR_INVALID_OPTLEN);
                        break;
                    }

                    if (*ptr == IPv6OPT_PADN) /* PadN */
                    {
                        //fprintf (stdout, "PadN option\n");
                        padn_cnt++;

                        /* a zero padN len would be weird */
                        if (ip6_optlen == 0)
                            ENGINE_SET_EVENT(p, IPv6_EXTHDR_ZERO_LEN_PADN);
                    }
                    else if (*ptr == IPv6OPT_RA) /* RA */
                    {
                        ra->ip6ra_type = *(ptr);
                        ra->ip6ra_len  = ip6_optlen;

                        if (ip6_optlen < sizeof(ra->ip6ra_value)) {
                            ENGINE_SET_EVENT(p, IPv6_EXTHDR_INVALID_OPTLEN);
                            break;
                        }

                        memcpy(&ra->ip6ra_value, (ptr + 2), sizeof(ra->ip6ra_value));
                        ra->ip6ra_value = ntohs(ra->ip6ra_value);
                        //fprintf (stdout, "RA option: type %" PRIu32 " len %" PRIu32 " value %" PRIu32 "\n",
                        //    ra->ip6ra_type, ra->ip6ra_len, ra->ip6ra_value);
                        other_cnt++;
                    }
                    else if (*ptr == IPv6OPT_JUMBO) /* Jumbo */
                    {
                        jumbo->ip6j_type = *(ptr);
                        jumbo->ip6j_len  = ip6_optlen;

                        if (ip6_optlen < sizeof(jumbo->ip6j_payload_len)) {
                            ENGINE_SET_EVENT(p, IPv6_EXTHDR_INVALID_OPTLEN);
                            break;
                        }

                        memcpy(&jumbo->ip6j_payload_len, (ptr+2), sizeof(jumbo->ip6j_payload_len));
                        jumbo->ip6j_payload_len = ntohl(jumbo->ip6j_payload_len);
                        //fprintf (stdout, "Jumbo option: type %" PRIu32 " len %" PRIu32 " payload len %" PRIu32 "\n",
                        //    jumbo->ip6j_type, jumbo->ip6j_len, jumbo->ip6j_payload_len);
                    }
                    else if (*ptr == IPv6OPT_HAO) /* HAO */
                    {
                        hao->ip6hao_type = *(ptr);
                        hao->ip6hao_len  = ip6_optlen;

                        if (ip6_optlen < sizeof(hao->ip6hao_hoa)) {
                            ENGINE_SET_EVENT(p, IPv6_EXTHDR_INVALID_OPTLEN);
                            break;
                        }

                        memcpy(&hao->ip6hao_hoa, (ptr+2), sizeof(hao->ip6hao_hoa));
                        //fprintf (stdout, "HAO option: type %" PRIu32 " len %" PRIu32 " ",
                        //    hao->ip6hao_type, hao->ip6hao_len);
                        //char addr_buf[46];
                        //PrintInet(AF_INET6, (char *)&(hao->ip6hao_hoa),
                        //    addr_buf,sizeof(addr_buf));
                        //fprintf (stdout, "home addr %s\n", addr_buf);
                        other_cnt++;
                    } else {
                        if (nh == IPPROTO_HOPOPTS)
                            ENGINE_SET_EVENT(p, IPv6_HOPOPTS_UNKNOWN_OPT);
                        else
                            ENGINE_SET_EVENT(p, IPv6_DSTOPTS_UNKNOWN_OPT);

                        other_cnt++;
                    }
                    uint16_t optlen = (*(ptr + 1) + 2);
                    ptr += optlen; /* +2 for opt type and opt len fields */
                    offset += optlen;
                }
                /* flag packets that have only padding */
                if (padn_cnt > 0 && other_cnt == 0) {
                    if (nh == IPPROTO_HOPOPTS)
                        ENGINE_SET_EVENT(p, IPv6_HOPOPTS_ONLY_PADDING);
                    else
                        ENGINE_SET_EVENT(p, IPv6_DSTOPTS_ONLY_PADDING);
                }

                nh = *pkt;
                pkt += hdrextlen;
                plen -= hdrextlen;
                break;
            }

            case IPPROTO_FRAGMENT:
            {
                IPv6_SET_L4PROTO(p,nh);
                /* store the offset of this extension into the packet
                 * past the ipv6 header. We use it in defrag for creating
                 * a defragmented packet without the frag header */
                if (exthdr_fh_done == 0) {
                    p->ip6eh.fh_offset = pkt - orig_pkt;
                    exthdr_fh_done = 1;
                }

                uint16_t prev_hdrextlen = hdrextlen;
                hdrextlen = sizeof(IPv6FragHdr);
                if (hdrextlen > plen) {
                    ENGINE_SET_EVENT(p, IPv6_TRUNC_EXTHDR);
                    return;
                }

                /* for the frag header, the length field is reserved */
                if (*(pkt + 1) != 0) {
                    ENGINE_SET_EVENT(p, IPv6_FH_NON_ZERO_RES_FIELD);
                    /* non fatal, lets try to continue */
                }

                if (IPv6_EXTHDR_ISSET_FH(p)) {
                    ENGINE_SET_EVENT(p, IPv6_EXTHDR_DUPL_FH);
                    nh = *pkt;
                    pkt += hdrextlen;
                    plen -= hdrextlen;
                    break;
                }

                /* set the header flag first */
                IPv6_EXTHDR_SET_FH(p);

                /* parse the header and setup the vars */
                DecodeIPv6FragHeader(p, pkt, hdrextlen, plen, prev_hdrextlen);

                /* if FH has offset 0 and no more fragments are coming, we
                 * parse this packet further right away, no defrag will be
                 * needed. It is a useless FH then though, so we do set an
                 * decoder event. */
                if (p->ip6eh.fh_more_frags_set == 0 && p->ip6eh.fh_offset == 0) {
                    ENGINE_SET_EVENT(p, IPv6_EXTHDR_USELESS_FH);

                    nh = *pkt;
                    pkt += hdrextlen;
                    plen -= hdrextlen;
                    break;
                }

                /* the rest is parsed upon reassembly */
                p->flags |= PKT_IS_FRAGMENT;
                return;
            }
            case IPPROTO_ESP:
            {
                IPv6_SET_L4PROTO(p,nh);
                hdrextlen = sizeof(IPv6EspHdr);
                if (hdrextlen > plen) {
                    ENGINE_SET_EVENT(p, IPv6_TRUNC_EXTHDR);
                    return;
                }

                if (eh) {
                    ENGINE_SET_EVENT(p, IPv6_EXTHDR_DUPL_EH);
                    return;
                }

                eh = 1;

                nh = IPPROTO_NONE;
                pkt += hdrextlen;
                plen -= hdrextlen;
                break;
            }
            case IPPROTO_AH:
            {
                IPv6_SET_L4PROTO(p,nh);
                /* we need the header as a minimum */
                hdrextlen = sizeof(IPv6AuthHdr);
                /* the payload len field is the number of extra 4 byte fields,
                 * IPv6AuthHdr already contains the first */
                if (*(pkt+1) > 0)
                    hdrextlen += ((*(pkt+1) - 1) * 4);

                oryx_logd("hdrextlen %"PRIu8, hdrextlen);

                if (hdrextlen > plen) {
                    ENGINE_SET_EVENT(p, IPv6_TRUNC_EXTHDR);
                    return;
                }

                IPv6AuthHdr *ahhdr = (IPv6AuthHdr *)pkt;
                if (ahhdr->ip6ah_reserved != 0x0000) {
                    ENGINE_SET_EVENT(p, IPv6_EXTHDR_AH_RES_NOT_NULL);
                }

                if (ah) {
                    ENGINE_SET_EVENT(p, IPv6_EXTHDR_DUPL_AH);
                    nh = *pkt;
                    pkt += hdrextlen;
                    plen -= hdrextlen;
                    break;
                }

                ah = 1;

                nh = *pkt;
                pkt += hdrextlen;
                plen -= hdrextlen;
                break;
            }
            case IPPROTO_IPIP:
                IPv6_SET_L4PROTO(p,nh);
                DecodeIPv4inIPv6(tv, dtv, p, pkt, plen, pq);
                return;
            /* none, last header */
            case IPPROTO_NONE:
                IPv6_SET_L4PROTO(p,nh);
                return;
            case IPPROTO_ICMP:
                ENGINE_SET_EVENT(p,IPv6_WITH_ICMPV4);
                return;
            /* no parsing yet, just skip it */
            case IPPROTO_MH:
            case IPPROTO_HIP:
            case IPPROTO_SHIM6:
                hdrextlen = 8 + (*(pkt+1) * 8);  /* 8 bytes + length in 8 octet units */
                if (hdrextlen > plen) {
                    ENGINE_SET_EVENT(p, IPv6_TRUNC_EXTHDR);
                    return;
                }
                nh = *pkt;
                pkt += hdrextlen;
                plen -= hdrextlen;
                break;
            default:
                ENGINE_SET_EVENT(p, IPv6_UNKNOWN_NEXT_HEADER);
                IPv6_SET_L4PROTO(p,nh);
                return;
        }
    }

    return;
}

static __oryx_always_inline__
int DecodeIPv6Packet (threadvar_ctx_t *tv, decode_threadvar_ctx_t *dtv, packet_t *p, uint8_t *pkt, uint16_t len)
{
    if (unlikely(len < IPv6_HEADER_LEN)) {
        return -1;
    }

    if (unlikely(IP_GET_RAW_VER(pkt) != 6)) {
        oryx_logd("wrong ip version %" PRIu8 "",IP_GET_RAW_VER(pkt));
        ENGINE_SET_INVALID_EVENT(p, IPv6_WRONG_IP_VER);
        return -1;
    }

    p->ip6h = (IPv6Hdr *)pkt;

    if (unlikely(len < (IPv6_HEADER_LEN + IPv6_GET_PLEN(p))))
    {
        ENGINE_SET_INVALID_EVENT(p, IPv6_TRUNC_PKT);
        return -1;
    }

    SET_IPv6_SRC_ADDR(p,&p->src);
    SET_IPv6_DST_ADDR(p,&p->dst);

    return 0;
}

static __oryx_always_inline__
int DecodeIPv60(threadvar_ctx_t *tv, decode_threadvar_ctx_t *dtv, packet_t *p, uint8_t *pkt, uint16_t len, PacketQueue *pq)
{
	oryx_logd("IPv6");
	
    int ret;

	oryx_counter_inc(&tv->perf_private_ctx0, dtv->counter_ipv6);
    /* do the actual decoding */
    ret = DecodeIPv6Packet (tv, dtv, p, pkt, len);
    if (unlikely(ret < 0)) {
        p->ip6h = NULL;
        return TM_ECODE_FAILED;
    }

#if defined(BUILD_DEBUG)
    if (1) { /* only convert the addresses if debug is really enabled */
        /* debug print */
        char s[46], d[46];
        PrintInet(AF_INET6, (const void *)GET_IPv6_SRC_ADDR(p), s, sizeof(s));
        PrintInet(AF_INET6, (const void *)GET_IPv6_DST_ADDR(p), d, sizeof(d));
        oryx_logd("IPv6 %s->%s - CLASS: %" PRIu32 " FLOW: %" PRIu32 " NH: %" PRIu32 " PLEN: %" PRIu32 " HLIM: %" PRIu32 "", s,d,
                IPv6_GET_CLASS(p), IPv6_GET_FLOW(p), IPv6_GET_NH(p), IPv6_GET_PLEN(p),
                IPv6_GET_HLIM(p));
    }
#endif /* BUILD_DEBUG */

    /* now process the Ext headers and/or the L4 Layer */
    switch(IPv6_GET_NH(p)) {
        case IPPROTO_TCP:
            IPv6_SET_L4PROTO (p, IPPROTO_TCP);
            DecodeTCP0(tv, dtv, p, pkt + IPv6_HEADER_LEN, IPv6_GET_PLEN(p), pq);
            return TM_ECODE_OK;
        case IPPROTO_UDP:
            IPv6_SET_L4PROTO (p, IPPROTO_UDP);
            DecodeUDP0(tv, dtv, p, pkt + IPv6_HEADER_LEN, IPv6_GET_PLEN(p), pq);
            return TM_ECODE_OK;
        case IPPROTO_ICMPV6:
            IPv6_SET_L4PROTO (p, IPPROTO_ICMPV6);
            DecodeICMPv60(tv, dtv, p, pkt + IPv6_HEADER_LEN, IPv6_GET_PLEN(p), pq);
            return TM_ECODE_OK;
        case IPPROTO_SCTP:
            IPv6_SET_L4PROTO (p, IPPROTO_SCTP);
            DecodeSCTP0(tv, dtv, p, pkt + IPv6_HEADER_LEN, IPv6_GET_PLEN(p), pq);
            return TM_ECODE_OK;
        case IPPROTO_IPIP:
            IPv6_SET_L4PROTO(p, IPPROTO_IPIP);
            DecodeIPv4inIPv6(tv, dtv, p, pkt + IPv6_HEADER_LEN, IPv6_GET_PLEN(p), pq);
            return TM_ECODE_OK;
        case IPPROTO_IPv6:
            DecodeIPv6inIPv6(tv, dtv, p, pkt + IPv6_HEADER_LEN, IPv6_GET_PLEN(p), pq);
            return TM_ECODE_OK;
        case IPPROTO_FRAGMENT:
        case IPPROTO_HOPOPTS:
        case IPPROTO_ROUTING:
        case IPPROTO_NONE:
        case IPPROTO_DSTOPTS:
        case IPPROTO_AH:
        case IPPROTO_ESP:
        case IPPROTO_MH:
        case IPPROTO_HIP:
        case IPPROTO_SHIM6:
            DecodeIPv6ExtHdrs(tv, dtv, p, pkt + IPv6_HEADER_LEN, IPv6_GET_PLEN(p), pq);
            break;
        case IPPROTO_ICMP:
            ENGINE_SET_EVENT(p,IPv6_WITH_ICMPV4);
            break;
        default:
            ENGINE_SET_EVENT(p, IPv6_UNKNOWN_NEXT_HEADER);
            IPv6_SET_L4PROTO (p, IPv6_GET_NH(p));
            break;
    }
    p->proto = IPv6_GET_L4PROTO (p);

    /* Pass to defragger if a fragment. */
    if (IPv6_EXTHDR_ISSET_FH(p)) {
#if defined(HAVE_DEFRAG)
        packet_t *rp = Defrag(tv, dtv, p, pq);
        if (rp != NULL) {
            PacketEnqueue(pq,rp);
        }
#endif
    }

    return TM_ECODE_OK;
}


#endif

