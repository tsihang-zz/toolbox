
#include "oryx.h"
#include "dp_decode.h"

int DecodeVLAN0(ThreadVars *tv, DecodeThreadVars *dtv, Packet *p, uint8_t *pkt, uint16_t len, PacketQueue *pq);

static inline uint16_t DecodeVLANGetId0(const Packet *p, uint8_t layer)
{
    if (unlikely(layer > 1))
        return 0;

    if (p->vlanh[layer] == NULL && (p->vlan_idx >= (layer + 1))) {
        return p->vlan_id[layer];
    } else {
        return GET_VLAN_ID(p->vlanh[layer]);
    }
    return 0;
}

static int DecodeIEEE8021ah0(ThreadVars *tv, DecodeThreadVars *dtv, Packet *p, uint8_t *pkt, uint16_t len, PacketQueue *pq)
{
    oryx_counter_inc(&tv->perf_private_ctx0, dtv->counter_ieee8021ah);

    if (len < IEEE8021AH_HEADER_LEN) {
        ENGINE_SET_INVALID_EVENT(p, IEEE8021AH_HEADER_TOO_SMALL);
        return TM_ECODE_FAILED;
    }

    IEEE8021ahHdr *hdr = (IEEE8021ahHdr *)pkt;
    uint16_t next_proto = ntohs(hdr->type);

    switch (next_proto) {
        case ETHERNET_TYPE_VLAN:
        case ETHERNET_TYPE_8021QINQ: {
            DecodeVLAN0(tv, dtv, p, pkt + IEEE8021AH_HEADER_LEN,
                    len - IEEE8021AH_HEADER_LEN, pq);
            break;
        }
    }
    return TM_ECODE_OK;
}


/**
 * \internal
 * \brief this function is used to decode IEEE802.1q packets
 *
 * \param tv pointer to the thread vars
 * \param dtv pointer code thread vars
 * \param p pointer to the packet struct
 * \param pkt pointer to the raw packet
 * \param len packet len
 * \param pq pointer to the packet queue
 *
 */
int DecodeVLAN0(ThreadVars *tv, DecodeThreadVars *dtv, Packet *p, uint8_t *pkt, uint16_t len, PacketQueue *pq)
{
    uint32_t proto;

	oryx_logd("VLAN");

    if (p->vlan_idx == 0)
        oryx_counter_inc(&tv->perf_private_ctx0, dtv->counter_vlan);
    else if (p->vlan_idx == 1)
        oryx_counter_inc(&tv->perf_private_ctx0, dtv->counter_vlan_qinq);

    if(len < VLAN_HEADER_LEN)    {
        ENGINE_SET_INVALID_EVENT(p, VLAN_HEADER_TOO_SMALL);
        return TM_ECODE_FAILED;
    }
    if (p->vlan_idx >= 2) {
        ENGINE_SET_EVENT(p,VLAN_HEADER_TOO_MANY_LAYERS);
        return TM_ECODE_FAILED;
    }

    p->vlanh[p->vlan_idx] = (VLANHdr *)pkt;
    if(p->vlanh[p->vlan_idx] == NULL)
        return TM_ECODE_FAILED;

    proto = GET_VLAN_PROTO(p->vlanh[p->vlan_idx]);

    oryx_logd("p %p pkt %p VLAN protocol %04x VLAN PRI %d VLAN CFI %d VLAN ID %d Len: %" PRId32 "",
            p, pkt, proto, GET_VLAN_PRIORITY(p->vlanh[p->vlan_idx]),
            GET_VLAN_CFI(p->vlanh[p->vlan_idx]), GET_VLAN_ID(p->vlanh[p->vlan_idx]), len);

    /* only store the id for flow hashing if it's not disabled. */
    if (dtv->vlan_disabled == 0)
        p->vlan_id[p->vlan_idx] = (uint16_t)GET_VLAN_ID(p->vlanh[p->vlan_idx]);

    p->vlan_idx++;

    switch (proto)   {
        case ETHERNET_TYPE_IP:
            DecodeIPv40(tv, dtv, p, pkt + VLAN_HEADER_LEN,
                       len - VLAN_HEADER_LEN, pq);
            break;
        case ETHERNET_TYPE_IPV6:
            DecodeIPv60(tv, dtv, p, pkt + VLAN_HEADER_LEN,
                       len - VLAN_HEADER_LEN, pq);
            break;
        case ETHERNET_TYPE_PPPOE_SESS:
            DecodePPPOESession0(tv, dtv, p, pkt + VLAN_HEADER_LEN,
                               len - VLAN_HEADER_LEN, pq);
            break;
        case ETHERNET_TYPE_PPPOE_DISC:
            DecodePPPOEDiscovery0(tv, dtv, p, pkt + VLAN_HEADER_LEN,
                                 len - VLAN_HEADER_LEN, pq);
            break;
        case ETHERNET_TYPE_VLAN:
        case ETHERNET_TYPE_8021AD:
            if (p->vlan_idx >= 2) {
                ENGINE_SET_EVENT(p,VLAN_HEADER_TOO_MANY_LAYERS);
                return TM_ECODE_OK;
            } else {
                DecodeVLAN0(tv, dtv, p, pkt + VLAN_HEADER_LEN,
                        len - VLAN_HEADER_LEN, pq);
            }
            break;
        case ETHERNET_TYPE_8021AH:
            DecodeIEEE8021ah0(tv, dtv, p, pkt + VLAN_HEADER_LEN,
                    len - VLAN_HEADER_LEN, pq);
            break;
		case ETHERNET_TYPE_ARP:
			DecodeARP0(tv, dtv, p, pkt + VLAN_HEADER_LEN,
                    len - VLAN_HEADER_LEN, pq);
			break;
        default:
            oryx_logd("unknown VLAN type: %" PRIx32 "", proto);
            ENGINE_SET_INVALID_EVENT(p, VLAN_UNKNOWN_TYPE);
            return TM_ECODE_OK;
    }

    return TM_ECODE_OK;
}

