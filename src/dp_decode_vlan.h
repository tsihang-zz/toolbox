#ifndef DP_DECODE_VLAN_H
#define DP_DECODE_VLAN_H

static inline uint16_t DecodeVLANGetId(const Packet *p, uint8_t layer)
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
/* return vlan id in host byte order */
#define VLAN_GET_ID1(p)             DecodeVLANGetId((p), 0)
#define VLAN_GET_ID2(p)             DecodeVLANGetId((p), 1)


int DecodeVLAN0(ThreadVars *tv, DecodeThreadVars *dtv, Packet *p, uint8_t *pkt, uint16_t len, PacketQueue *pq);

#endif

