#include "oryx.h"
#include "dp_decode.h"

/**
 * \brief Function to decode GRE packets
 */

int DecodeGRE0(ThreadVars *tv, DecodeThreadVars *dtv, Packet *p, uint8_t *pkt, uint16_t len, PacketQueue *pq)
{
    uint16_t header_len = GRE_HDR_LEN;
    GRESreHdr *gsre = NULL;

	oryx_logd("GRE");

    oryx_counter_inc(&tv->perf_private_ctx0, dtv->counter_gre);
    if(len < GRE_HDR_LEN)    {
        ENGINE_SET_INVALID_EVENT(p, GRE_PKT_TOO_SMALL);
        return TM_ECODE_FAILED;
    }

    p->greh = (GREHdr *)pkt;
    if(p->greh == NULL)
        return TM_ECODE_FAILED;

    oryx_logd("p %p pkt %p GRE protocol %04x Len: %d GRE version %x",
        p, pkt, GRE_GET_PROTO(p->greh), len,GRE_GET_VERSION(p->greh));

    switch (GRE_GET_VERSION(p->greh))
    {
        case GRE_VERSION_0:

            /* GRE version 0 doenst support the fields below RFC 1701 */

            /**
             * \todo We need to make sure this does not allow bypassing
             *       inspection.  A server may just ignore these and
             *       continue processing the packet, but we will not look
             *       further into it.
             */

            if (GRE_FLAG_ISSET_RECUR(p->greh)) {
                ENGINE_SET_INVALID_EVENT(p, GRE_VERSION0_RECUR);
                return TM_ECODE_OK;
            }

            if (GREV1_FLAG_ISSET_FLAGS(p->greh))   {
                ENGINE_SET_INVALID_EVENT(p, GRE_VERSION0_FLAGS);
                return TM_ECODE_OK;
            }

            /* Adjust header length based on content */

            if (GRE_FLAG_ISSET_KY(p->greh))
                header_len += GRE_KEY_LEN;

            if (GRE_FLAG_ISSET_SQ(p->greh))
                header_len += GRE_SEQ_LEN;

            if (GRE_FLAG_ISSET_CHKSUM(p->greh) || GRE_FLAG_ISSET_ROUTE(p->greh))
                header_len += GRE_CHKSUM_LEN + GRE_OFFSET_LEN;

            if (header_len > len)   {
                ENGINE_SET_INVALID_EVENT(p, GRE_VERSION0_HDR_TOO_BIG);
                return TM_ECODE_OK;
            }

            if (GRE_FLAG_ISSET_ROUTE(p->greh))
            {
                while (1)
                {
                    if ((header_len + GRE_SRE_HDR_LEN) > len) {
                        ENGINE_SET_INVALID_EVENT(p,
                                                 GRE_VERSION0_MALFORMED_SRE_HDR);
                        return TM_ECODE_OK;
                    }

                    gsre = (GRESreHdr *)(pkt + header_len);

                    header_len += GRE_SRE_HDR_LEN;

                    if ((ntohs(gsre->af) == 0) && (gsre->sre_length == 0))
                        break;

                    header_len += gsre->sre_length;
                    if (header_len > len) {
                        ENGINE_SET_INVALID_EVENT(p,
                                                 GRE_VERSION0_MALFORMED_SRE_HDR);
                        return TM_ECODE_OK;
                    }
                }
            }
            break;

        case GRE_VERSION_1:

            /* GRE version 1 doenst support the fields below RFC 1701 */

            /**
             * \todo We need to make sure this does not allow bypassing
             *       inspection.  A server may just ignore these and
             *       continue processing the packet, but we will not look
             *       further into it.
             */

            if (GRE_FLAG_ISSET_CHKSUM(p->greh))    {
                ENGINE_SET_INVALID_EVENT(p,GRE_VERSION1_CHKSUM);
                return TM_ECODE_OK;
            }

            if (GRE_FLAG_ISSET_ROUTE(p->greh)) {
                ENGINE_SET_INVALID_EVENT(p,GRE_VERSION1_ROUTE);
                return TM_ECODE_OK;
            }

            if (GRE_FLAG_ISSET_SSR(p->greh))   {
                ENGINE_SET_INVALID_EVENT(p,GRE_VERSION1_SSR);
                return TM_ECODE_OK;
            }

            if (GRE_FLAG_ISSET_RECUR(p->greh)) {
                ENGINE_SET_INVALID_EVENT(p,GRE_VERSION1_RECUR);
                return TM_ECODE_OK;
            }

            if (GREV1_FLAG_ISSET_FLAGS(p->greh))   {
                ENGINE_SET_INVALID_EVENT(p,GRE_VERSION1_FLAGS);
                return TM_ECODE_OK;
            }

            if (GRE_GET_PROTO(p->greh) != GRE_PROTO_PPP)  {
                ENGINE_SET_INVALID_EVENT(p,GRE_VERSION1_WRONG_PROTOCOL);
                return TM_ECODE_OK;
            }

            if (!(GRE_FLAG_ISSET_KY(p->greh))) {
                ENGINE_SET_INVALID_EVENT(p,GRE_VERSION1_NO_KEY);
                return TM_ECODE_OK;
            }

            header_len += GRE_KEY_LEN;

            /* Adjust header length based on content */

            if (GRE_FLAG_ISSET_SQ(p->greh))
                header_len += GRE_SEQ_LEN;

            if (GREV1_FLAG_ISSET_ACK(p->greh))
                header_len += GREV1_ACK_LEN;

            if (header_len > len)   {
                ENGINE_SET_INVALID_EVENT(p, GRE_VERSION1_HDR_TOO_BIG);
                return TM_ECODE_OK;
            }

            break;
        default:
            ENGINE_SET_INVALID_EVENT(p, GRE_WRONG_VERSION);
            return TM_ECODE_OK;
    }

    switch (GRE_GET_PROTO(p->greh))
    {
        case ETHERNET_TYPE_IP:
            {
            #if 0
                if (pq != NULL) {
                    Packet *tp = PacketTunnelPktSetup(tv, dtv, p, pkt + header_len,
                            len - header_len, DECODE_TUNNEL_IPV4, pq);
                    if (tp != NULL) {
                        PKT_SET_SRC(tp, PKT_SRC_DECODER_GRE);
                        PacketEnqueue(pq,tp);
                    }
                }
			#endif
                break;
            }

        case GRE_PROTO_PPP:
            {
            #if 0
                if (pq != NULL) {
                    Packet *tp = PacketTunnelPktSetup(tv, dtv, p, pkt + header_len,
                            len - header_len, DECODE_TUNNEL_PPP, pq);
                    if (tp != NULL) {
                        PKT_SET_SRC(tp, PKT_SRC_DECODER_GRE);
                        PacketEnqueue(pq,tp);
                    }
                }
			#endif
                break;
            }

        case ETHERNET_TYPE_IPV6:
            {
            #if 0
                if (pq != NULL) {
                    Packet *tp = PacketTunnelPktSetup(tv, dtv, p, pkt + header_len,
                            len - header_len, DECODE_TUNNEL_IPV6, pq);
                    if (tp != NULL) {
                        PKT_SET_SRC(tp, PKT_SRC_DECODER_GRE);
                        PacketEnqueue(pq,tp);
                    }
                }
			#endif
                break;
            }

        case ETHERNET_TYPE_VLAN:
            {
            #if 0
                if (pq != NULL) {
                    Packet *tp = PacketTunnelPktSetup(tv, dtv, p, pkt + header_len,
                            len - header_len, DECODE_TUNNEL_VLAN, pq);
                    if (tp != NULL) {
                        PKT_SET_SRC(tp, PKT_SRC_DECODER_GRE);
                        PacketEnqueue(pq,tp);
                    }
                }
			#endif
                break;
            }

        case ETHERNET_TYPE_ERSPAN:
        {
        #if 0
            if (pq != NULL) {
                Packet *tp = PacketTunnelPktSetup(tv, dtv, p, pkt + header_len,
                        len - header_len, DECODE_TUNNEL_ERSPAN, pq);
                if (tp != NULL) {
                    PKT_SET_SRC(tp, PKT_SRC_DECODER_GRE);
                    PacketEnqueue(pq,tp);
                }
            }
		#endif
            break;
        }

        case ETHERNET_TYPE_BRIDGE:
            {
            #if 0
                if (pq != NULL) {
                    Packet *tp = PacketTunnelPktSetup(tv, dtv, p, pkt + header_len,
                            len - header_len, DECODE_TUNNEL_ETHERNET, pq);
                    if (tp != NULL) {
                        PKT_SET_SRC(tp, PKT_SRC_DECODER_GRE);
                        PacketEnqueue(pq,tp);
                    }
                }
			#endif
                break;
            }

        default:
            return TM_ECODE_OK;
    }
    return TM_ECODE_OK;
}

