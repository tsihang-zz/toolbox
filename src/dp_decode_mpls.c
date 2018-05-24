#include "oryx.h"
#include "dp_decode.h"

#define MPLS_HEADER_LEN         4
#define MPLS_PW_LEN             4
#define MPLS_MAX_RESERVED_LABEL 15

#define MPLS_LABEL_IPV4         0
#define MPLS_LABEL_ROUTER_ALERT 1
#define MPLS_LABEL_IPV6         2
#define MPLS_LABEL_NULL         3

#define MPLS_LABEL(shim)        ntohl(shim) >> 12
#define MPLS_BOTTOM(shim)       ((ntohl(shim) >> 8) & 0x1)

/* Inner protocol guessing values. */
#define MPLS_PROTO_ETHERNET_PW  0
#define MPLS_PROTO_IPV4         4
#define MPLS_PROTO_IPV6         6

int DecodeMPLS0(ThreadVars *tv, DecodeThreadVars *dtv, Packet *p, uint8_t *pkt,
    uint16_t len, PacketQueue *pq)
{
    uint32_t shim;
    int label;
    int event = 0;

	oryx_logd("MPLS");

    oryx_counter_inc(&tv->perf_private_ctx0, dtv->counter_mpls);

    do {
        if (len < MPLS_HEADER_LEN) {
            ENGINE_SET_INVALID_EVENT(p, MPLS_HEADER_TOO_SMALL);
            return TM_ECODE_FAILED;
        }
        shim = *(uint32_t *)pkt;
        pkt += MPLS_HEADER_LEN;
        len -= MPLS_HEADER_LEN;
    } while (MPLS_BOTTOM(shim) == 0);

    label = MPLS_LABEL(shim);
    if (label == MPLS_LABEL_IPV4) {
        return DecodeIPv40(tv, dtv, p, pkt, len, pq);
    }
    else if (label == MPLS_LABEL_ROUTER_ALERT) {
        /* Not valid at the bottom of the stack. */
        event = MPLS_BAD_LABEL_ROUTER_ALERT;
    }
    else if (label == MPLS_LABEL_IPV6) {
        return DecodeIPv60(tv, dtv, p, pkt, len, pq);
    }
    else if (label == MPLS_LABEL_NULL) {
        /* Shouldn't appear on the wire. */
        event = MPLS_BAD_LABEL_IMPLICIT_NULL;
    }
    else if (label < MPLS_MAX_RESERVED_LABEL) {
        event = MPLS_BAD_LABEL_RESERVED;
    }

    if (event) {
        goto end;
    }

    /* Best guess at inner packet. */
    switch (pkt[0] >> 4) {
    case MPLS_PROTO_IPV4:
        DecodeIPv40(tv, dtv, p, pkt, len, pq);
        break;
    case MPLS_PROTO_IPV6:
        DecodeIPv60(tv, dtv, p, pkt, len, pq);
        break;
    case MPLS_PROTO_ETHERNET_PW:
        DecodeEthernet0(tv, dtv, p, pkt + MPLS_PW_LEN, len - MPLS_PW_LEN,
            pq);
        break;
    default:
        ENGINE_SET_INVALID_EVENT(p, MPLS_UNKNOWN_PAYLOAD_TYPE);
        return TM_ECODE_OK;
    }

end:
    if (event) {
        ENGINE_SET_EVENT(p, event);
    }
    return TM_ECODE_OK;
}

