#ifndef DP_DECODE_IPv4_H
#define DP_DECODE_IPv4_H

/* ONLY call these functions after making sure that:
 * 1. p->ip4h is set
 * 2. p->ip4h is valid (len is correct)
 */
#define IPV4_GET_VER(p) \
    IPV4_GET_RAW_VER((p)->ip4h)
#define IPV4_GET_HLEN(p) \
    (IPV4_GET_RAW_HLEN((p)->ip4h) << 2)
#define IPV4_GET_IPTOS(p) \
    IPV4_GET_RAW_IPTOS((p)->ip4h)
#define IPV4_GET_IPLEN(p) \
    (ntohs(IPV4_GET_RAW_IPLEN((p)->ip4h)))
#define IPV4_GET_IPID(p) \
    (ntohs(IPV4_GET_RAW_IPID((p)->ip4h)))
/* _IPV4_GET_IPOFFSET: get the content of the offset header field in host order */
#define _IPV4_GET_IPOFFSET(p) \
    (ntohs(IPV4_GET_RAW_IPOFFSET((p)->ip4h)))
/* IPV4_GET_IPOFFSET: get the final offset */
#define IPV4_GET_IPOFFSET(p) \
    (_IPV4_GET_IPOFFSET(p) & 0x1fff)
/* IPV4_GET_RF: get the RF flag. Use _IPV4_GET_IPOFFSET to save a ntohs call. */
#define IPV4_GET_RF(p) \
    (uint8_t)((_IPV4_GET_IPOFFSET((p)) & 0x8000) >> 15)
/* IPV4_GET_DF: get the DF flag. Use _IPV4_GET_IPOFFSET to save a ntohs call. */
#define IPV4_GET_DF(p) \
    (uint8_t)((_IPV4_GET_IPOFFSET((p)) & 0x4000) >> 14)
/* IPV4_GET_MF: get the MF flag. Use _IPV4_GET_IPOFFSET to save a ntohs call. */
#define IPV4_GET_MF(p) \
    (uint8_t)((_IPV4_GET_IPOFFSET((p)) & 0x2000) >> 13)
#define IPV4_GET_IPTTL(p) \
     IPV4_GET_RAW_IPTTL(p->ip4h)
#define IPV4_GET_IPPROTO(p) \
    IPV4_GET_RAW_IPPROTO((p)->ip4h)

#define CLEAR_IPV4_PACKET(p) do { \
    (p)->ip4h = NULL; \
    (p)->level3_comp_csum = -1; \
} while (0)

enum IPV4OptionFlags {
    IPV4_OPT_FLAG_EOL = 0,
    IPV4_OPT_FLAG_NOP,
    IPV4_OPT_FLAG_RR,
    IPV4_OPT_FLAG_TS,
    IPV4_OPT_FLAG_QS,
    IPV4_OPT_FLAG_LSRR,
    IPV4_OPT_FLAG_SSRR,
    IPV4_OPT_FLAG_SID,
    IPV4_OPT_FLAG_SEC,
    IPV4_OPT_FLAG_CIPSO,
    IPV4_OPT_FLAG_RTRALT,
};

/**
 * \brief Calculateor validate the checksum for the IP packet
 *
 * \param pkt  Pointer to the start of the IP packet
 * \param hlen Length of the IP header
 * \param init The current checksum if validating, 0 if generating.
 *
 * \retval csum For validation 0 will be returned for success, for calculation
 *    this will be the checksum.
 */
static __oryx_always_inline__
uint16_t IPV4Checksum(uint16_t *pkt, uint16_t hlen, uint16_t init)
{
    uint32_t csum = init;

    csum += pkt[0] + pkt[1] + pkt[2] + pkt[3] + pkt[4] + pkt[6] + pkt[7] +
        pkt[8] + pkt[9];

    hlen -= 20;
    pkt += 10;

    if (hlen == 0) {
        ;
    } else if (hlen == 4) {
        csum += pkt[0] + pkt[1];
    } else if (hlen == 8) {
        csum += pkt[0] + pkt[1] + pkt[2] + pkt[3];
    } else if (hlen == 12) {
        csum += pkt[0] + pkt[1] + pkt[2] + pkt[3] + pkt[4] + pkt[5];
    } else if (hlen == 16) {
        csum += pkt[0] + pkt[1] + pkt[2] + pkt[3] + pkt[4] + pkt[5] + pkt[6] +
            pkt[7];
    } else if (hlen == 20) {
        csum += pkt[0] + pkt[1] + pkt[2] + pkt[3] + pkt[4] + pkt[5] + pkt[6] +
            pkt[7] + pkt[8] + pkt[9];
    } else if (hlen == 24) {
        csum += pkt[0] + pkt[1] + pkt[2] + pkt[3] + pkt[4] + pkt[5] + pkt[6] +
            pkt[7] + pkt[8] + pkt[9] + pkt[10] + pkt[11];
    } else if (hlen == 28) {
        csum += pkt[0] + pkt[1] + pkt[2] + pkt[3] + pkt[4] + pkt[5] + pkt[6] +
            pkt[7] + pkt[8] + pkt[9] + pkt[10] + pkt[11] + pkt[12] + pkt[13];
    } else if (hlen == 32) {
        csum += pkt[0] + pkt[1] + pkt[2] + pkt[3] + pkt[4] + pkt[5] + pkt[6] +
            pkt[7] + pkt[8] + pkt[9] + pkt[10] + pkt[11] + pkt[12] + pkt[13] +
            pkt[14] + pkt[15];
    } else if (hlen == 36) {
        csum += pkt[0] + pkt[1] + pkt[2] + pkt[3] + pkt[4] + pkt[5] + pkt[6] +
            pkt[7] + pkt[8] + pkt[9] + pkt[10] + pkt[11] + pkt[12] + pkt[13] +
            pkt[14] + pkt[15] + pkt[16] + pkt[17];
    } else if (hlen == 40) {
        csum += pkt[0] + pkt[1] + pkt[2] + pkt[3] + pkt[4] + pkt[5] + pkt[6] +
            pkt[7] + pkt[8] + pkt[9] + pkt[10] + pkt[11] + pkt[12] + pkt[13] +
            pkt[14] + pkt[15] + pkt[16] + pkt[17] + pkt[18] + pkt[19];
    }

    csum = (csum >> 16) + (csum & 0x0000FFFF);
    csum += (csum >> 16);

    return (uint16_t) ~csum;
}

/* Generic validation
 *
 * [--type--][--len---]
 *
 * \todo This function needs removed in favor of specific validation.
 *
 * See: RFC 791
 */
static __oryx_always_inline__
int IPV4OptValidateGeneric(Packet *p, const IPV4Opt *o)
{
    switch (o->type) {
        /* See: RFC 4782 */
        case IPV4_OPT_QS:
            if (o->len < IPV4_OPT_QS_MIN) {
                ENGINE_SET_INVALID_EVENT(p, IPV4_OPT_INVALID_LEN);
                return -1;
            }
            break;
        /* See: RFC 1108 */
        case IPV4_OPT_SEC:
            if (o->len != IPV4_OPT_SEC_LEN) {
                ENGINE_SET_INVALID_EVENT(p, IPV4_OPT_INVALID_LEN);
                return -1;
            }
            break;
        case IPV4_OPT_SID:
            if (o->len != IPV4_OPT_SID_LEN) {
                ENGINE_SET_INVALID_EVENT(p, IPV4_OPT_INVALID_LEN);
                return -1;
            }
            break;
        /* See: RFC 2113 */
        case IPV4_OPT_RTRALT:
            if (o->len != IPV4_OPT_RTRALT_LEN) {
                ENGINE_SET_INVALID_EVENT(p, IPV4_OPT_INVALID_LEN);
                return -1;
            }
            break;
        default:
            /* Should never get here unless there is a coding error */
            ENGINE_SET_INVALID_EVENT(p, IPV4_OPT_UNKNOWN);
            return -1;
    }

    return 0;
}


/* Validate route type options
 *
 * [--type--][--len---][--ptr---][address1]...[addressN]
 *
 * See: RFC 791
 */
static __oryx_always_inline__
int IPV4OptValidateRoute(Packet *p, const IPV4Opt *o)
{
    uint8_t ptr;

    /* Check length */
    if (unlikely(o->len < IPV4_OPT_ROUTE_MIN)) {
        ENGINE_SET_INVALID_EVENT(p, IPV4_OPT_INVALID_LEN);
        return -1;
    }

    /* Data is required */
    if (unlikely(o->data == NULL)) {
        ENGINE_SET_INVALID_EVENT(p, IPV4_OPT_MALFORMED);
        return -1;
    }
    ptr = *o->data;

    /* Address pointer is 1 based and points at least after type+len+ptr,
     * must be a incremented by 4 bytes (address size) and cannot extend
     * past option length.
     */
    if (unlikely((ptr < 4) || (ptr % 4) || (ptr > o->len + 1))) {
        ENGINE_SET_INVALID_EVENT(p, IPV4_OPT_MALFORMED);
        return -1;
    }

    return 0;
}


/* Validate timestamp type options
 *
 * [--type--][--len---][--ptr---][ovfl][flag][rec1----...]...[recN----...]
 * NOTE: rec could be 4 (ts only) or 8 (ip+ts) bytes in length.
 *
 * See: RFC 781
 */
static __oryx_always_inline__
int IPV4OptValidateTimestamp(Packet *p, const IPV4Opt *o)
{
    uint8_t ptr;
    uint8_t flag;
    uint8_t rec_size;

    /* Check length */
    if (unlikely(o->len < IPV4_OPT_TS_MIN)) {
        ENGINE_SET_INVALID_EVENT(p, IPV4_OPT_INVALID_LEN);
        return -1;
    }

    /* Data is required */
    if (unlikely(o->data == NULL)) {
        ENGINE_SET_INVALID_EVENT(p, IPV4_OPT_MALFORMED);
        return -1;
    }
    ptr = *o->data;

    /* We need the flag to determine what is in the option payload */
    if (unlikely(ptr < 5)) {
        ENGINE_SET_INVALID_EVENT(p, IPV4_OPT_MALFORMED);
        return -1;
    }
    flag = *(o->data + 3) & 0x00ff;

    /* A flag of 1|3 means we have both the ip+ts in each record */
    rec_size = ((flag == 1) || (flag == 3)) ? 8 : 4;

    /* Address pointer is 1 based and points at least after
     * type+len+ptr+ovfl+flag, must be incremented by by the rec_size
     * and cannot extend past option length.
     */
    if (unlikely(((ptr - 5) % rec_size) || (ptr > o->len + 1))) {
        ENGINE_SET_INVALID_EVENT(p, IPV4_OPT_MALFORMED);
        return -1;
    }

    return 0;
}


/* Validate CIPSO option
 *
 * [--type--][--len---][--doi---][tags--...]
 *
 * See: draft-ietf-cipso-ipsecurity-01.txt
 * See: FIPS 188 (tags 6 & 7)
 */
static __oryx_always_inline__
int IPV4OptValidateCIPSO(Packet *p, const IPV4Opt *o)
{
//    uint32_t doi;
    uint8_t *tag;
    uint16_t len;

    /* Check length */
    if (unlikely(o->len < IPV4_OPT_CIPSO_MIN)) {
        ENGINE_SET_INVALID_EVENT(p, IPV4_OPT_INVALID_LEN);
        return -1;
    }

    /* Data is required */
    if (unlikely(o->data == NULL)) {
        ENGINE_SET_INVALID_EVENT(p, IPV4_OPT_MALFORMED);
        return -1;
    }
//    doi = *o->data;
    tag = o->data + 4;
    len = o->len - 1 - 1 - 4; /* Length of tags after header */


#if 0
    /* Domain of Interest (DOI) of 0 is reserved and thus invalid */
    /** \todo Aparently a DOI of zero is fine in practice - verify. */
    if (doi == 0) {
        ENGINE_SET_EVENT(p,IPV4_OPT_MALFORMED);
        return -1;
    }
#endif

    /* NOTE: We know len has passed min tests prior to this call */

    /* Check that tags are formatted correctly
     * [-ttype--][--tlen--][-tagdata-...]
     */
    while (len) {
        uint8_t ttype;
        uint8_t tlen;

        /* Tag header must fit within option length */
        if (unlikely(len < 2)) {
            //printf("CIPSO tag header too large %" PRIu16 " < 2\n", len);
            ENGINE_SET_INVALID_EVENT(p, IPV4_OPT_MALFORMED);
            return -1;
        }

        /* Tag header is type+len */
        ttype = *(tag++);
        tlen = *(tag++);

        /* Tag length must fit within the option length */
        if (unlikely(tlen > len)) {
            //printf("CIPSO tag len too large %" PRIu8 " > %" PRIu16 "\n", tlen, len);
            ENGINE_SET_INVALID_EVENT(p, IPV4_OPT_MALFORMED);
            return -1;
        }

        switch(ttype) {
            case 1:
            case 2:
            case 5:
            case 6:
            case 7:
                /* Tag is at least 4 and at most the remainder of option len */
                if (unlikely((tlen < 4) || (tlen > len))) {
                    //printf("CIPSO tag %" PRIu8 " bad tlen=%" PRIu8 " len=%" PRIu8 "\n", ttype, tlen, len);
                    ENGINE_SET_INVALID_EVENT(p, IPV4_OPT_MALFORMED);
                    return -1;
                }

                /* The alignment octet is always 0 except tag
                 * type 7, which has no such field.
                 */
                if (unlikely((ttype != 7) && (*tag != 0))) {
                    //printf("CIPSO tag %" PRIu8 " ao=%" PRIu8 "\n", ttype, tlen);
                    ENGINE_SET_INVALID_EVENT(p, IPV4_OPT_MALFORMED);
                    return -1;
                }

                /* Skip the rest of the tag payload */
                tag += tlen - 2;
                len -= tlen;

                continue;
            case 0:
                /* Tag type 0 is reserved and thus invalid */
                /** \todo Wireshark marks this a padding, but spec says reserved. */
                ENGINE_SET_INVALID_EVENT(p,IPV4_OPT_MALFORMED);
                return -1;
            default:
                //printf("CIPSO tag %" PRIu8 " unknown tag\n", ttype);
                ENGINE_SET_INVALID_EVENT(p, IPV4_OPT_MALFORMED);
                /** \todo May not want to return error here on unknown tag type (at least not for 3|4) */
                return -1;
        }
    }

    return 0;
}


typedef struct IPV4Options_ {
    IPV4Opt o_rr;
    IPV4Opt o_qs;
    IPV4Opt o_ts;
    IPV4Opt o_sec;
    IPV4Opt o_lsrr;
    IPV4Opt o_cipso;
    IPV4Opt o_sid;
    IPV4Opt o_ssrr;
    IPV4Opt o_rtralt;
} IPV4Options;

/**
 * Decode/Validate IPv4 Options.
 */
static __oryx_always_inline__
int DecodeIPV4Options(Packet *p, uint8_t *pkt, uint16_t len, IPV4Options *opts)
{
    uint16_t plen = len;

#if defined(BUILD_DEBUG)
    if (SCLogDebugEnabled()) {
        uint16_t i;
        char buf[256] = "";
        int offset = 0;

        for (i = 0; i < len; i++) {
            offset += snprintf(buf + offset, (sizeof(buf) - offset), "%02" PRIx8 " ", pkt[i]);
        }
        oryx_logd("IPV4OPTS: { %s}", buf);
    }
#endif

    /* Options length must be padded to 8byte boundary */
    if (plen % 8) {
        ENGINE_SET_EVENT(p,IPV4_OPT_PAD_REQUIRED);
        /* Warn - we can keep going */
    }

    while (plen)
    {
        p->ip4vars.opt_cnt++;

        /* single byte options */
        if (*pkt == IPV4_OPT_EOL) {
            /** \todo What if more data exist after EOL (possible covert channel or data leakage)? */
            oryx_logd("IPV4OPT %" PRIu16 " len 1 @ %" PRIu16 "/%" PRIu16 "",
                   *pkt, (len - plen), (len - 1));
            p->ip4vars.opts_set |= IPV4_OPT_FLAG_EOL;
            break;
        } else if (*pkt == IPV4_OPT_NOP) {
            oryx_logd("IPV4OPT %" PRIu16 " len 1 @ %" PRIu16 "/%" PRIu16 "",
                   *pkt, (len - plen), (len - 1));
            pkt++;
            plen--;

            p->ip4vars.opts_set |= IPV4_OPT_FLAG_NOP;

        /* multibyte options */
        } else {
            if (unlikely(plen < 2)) {
                /** \todo What if padding is non-zero (possible covert channel or data leakage)? */
                /** \todo Spec seems to indicate EOL required if there is padding */
                ENGINE_SET_EVENT(p,IPV4_OPT_EOL_REQUIRED);
                break;
            }

            /* Option length is too big for packet */
            if (unlikely(*(pkt+1) > plen)) {
                ENGINE_SET_INVALID_EVENT(p, IPV4_OPT_INVALID_LEN);
                return -1;
            }

            IPV4Opt opt = {*pkt, *(pkt+1), plen > 2 ? (pkt + 2) : NULL };

            /* we already know that the total options len is valid,
             * so here the len of the specific option must be bad.
             * Also check for invalid lengths 0 and 1. */
            if (unlikely(opt.len > plen || opt.len < 2)) {
                ENGINE_SET_INVALID_EVENT(p, IPV4_OPT_INVALID_LEN);
                return -1;
            }
            /* we are parsing the most commonly used opts to prevent
             * us from having to walk the opts list for these all the
             * time. */
            /** \todo Figure out which IP options are more common and list them first */
            switch (opt.type) {
                case IPV4_OPT_TS:
                    if (opts->o_ts.type != 0) {
                        ENGINE_SET_EVENT(p,IPV4_OPT_DUPLICATE);
                        /* Warn - we can keep going */
                        break;
                    } else if (IPV4OptValidateTimestamp(p, &opt)) {
                        return -1;
                    }
                    opts->o_ts = opt;
                    p->ip4vars.opts_set |= IPV4_OPT_FLAG_TS;
                    break;
                case IPV4_OPT_RR:
                    if (opts->o_rr.type != 0) {
                        ENGINE_SET_EVENT(p,IPV4_OPT_DUPLICATE);
                        /* Warn - we can keep going */
                        break;
                    } else if (IPV4OptValidateRoute(p, &opt) != 0) {
                        return -1;
                    }
                    opts->o_rr = opt;
                    p->ip4vars.opts_set |= IPV4_OPT_FLAG_RR;
                    break;
                case IPV4_OPT_QS:
                    if (opts->o_qs.type != 0) {
                        ENGINE_SET_EVENT(p,IPV4_OPT_DUPLICATE);
                        /* Warn - we can keep going */
                        break;
                    } else if (IPV4OptValidateGeneric(p, &opt)) {
                        return -1;
                    }
                    opts->o_qs = opt;
                    p->ip4vars.opts_set |= IPV4_OPT_FLAG_QS;
                    break;
                case IPV4_OPT_SEC:
                    if (opts->o_sec.type != 0) {
                        ENGINE_SET_EVENT(p,IPV4_OPT_DUPLICATE);
                        /* Warn - we can keep going */
                        break;
                    } else if (IPV4OptValidateGeneric(p, &opt)) {
                        return -1;
                    }
                    opts->o_sec = opt;
                    p->ip4vars.opts_set |= IPV4_OPT_FLAG_SEC;
                    break;
                case IPV4_OPT_LSRR:
                    if (opts->o_lsrr.type != 0) {
                        ENGINE_SET_EVENT(p,IPV4_OPT_DUPLICATE);
                        /* Warn - we can keep going */
                        break;
                    } else if (IPV4OptValidateRoute(p, &opt) != 0) {
                        return -1;
                    }
                    opts->o_lsrr = opt;
                    p->ip4vars.opts_set |= IPV4_OPT_FLAG_LSRR;
                    break;
                case IPV4_OPT_CIPSO:
                    if (opts->o_cipso.type != 0) {
                        ENGINE_SET_EVENT(p,IPV4_OPT_DUPLICATE);
                        /* Warn - we can keep going */
                        break;
                    } else if (IPV4OptValidateCIPSO(p, &opt) != 0) {
                        return -1;
                    }
                    opts->o_cipso = opt;
                    p->ip4vars.opts_set |= IPV4_OPT_FLAG_CIPSO;
                    break;
                case IPV4_OPT_SID:
                    if (opts->o_sid.type != 0) {
                        ENGINE_SET_EVENT(p,IPV4_OPT_DUPLICATE);
                        /* Warn - we can keep going */
                        break;
                    } else if (IPV4OptValidateGeneric(p, &opt)) {
                        return -1;
                    }
                    opts->o_sid = opt;
                    p->ip4vars.opts_set |= IPV4_OPT_FLAG_SID;
                    break;
                case IPV4_OPT_SSRR:
                    if (opts->o_ssrr.type != 0) {
                        ENGINE_SET_EVENT(p,IPV4_OPT_DUPLICATE);
                        /* Warn - we can keep going */
                        break;
                    } else if (IPV4OptValidateRoute(p, &opt) != 0) {
                        return -1;
                    }
                    opts->o_ssrr = opt;
                    p->ip4vars.opts_set |= IPV4_OPT_FLAG_SSRR;
                    break;
                case IPV4_OPT_RTRALT:
                    if (opts->o_rtralt.type != 0) {
                        ENGINE_SET_EVENT(p,IPV4_OPT_DUPLICATE);
                        /* Warn - we can keep going */
                        break;
                    } else if (IPV4OptValidateGeneric(p, &opt)) {
                        return -1;
                    }
                    opts->o_rtralt = opt;
                    p->ip4vars.opts_set |= IPV4_OPT_FLAG_RTRALT;
                    break;
                default:
                    oryx_logd("IPV4OPT <unknown> (%" PRIu8 ") len %" PRIu8,
                           opt.type, opt.len);
                    ENGINE_SET_EVENT(p,IPV4_OPT_INVALID);
                    /* Warn - we can keep going */
                    break;
            }

            pkt += opt.len;
            plen -= opt.len;
        }
    }

    return 0;
}


static __oryx_always_inline__
int DecodeIPV4Packet0(Packet *p, uint8_t *pkt, uint16_t len)
{
    if (unlikely(len < IPV4_HEADER_LEN)) {
		oryx_loge(-1,
			"ipv4 pkt smaller than minimum header size %" PRIu16 "", len);
        ENGINE_SET_INVALID_EVENT(p, IPV4_PKT_TOO_SMALL);
        return -1;
    }

    if (unlikely(IP_GET_RAW_VER(pkt) != 4)) {
        oryx_loge(-1,
			"wrong ip version in ip options %" PRIu8 "", IP_GET_RAW_VER(pkt));
        ENGINE_SET_INVALID_EVENT(p, IPV4_WRONG_IP_VER);
        return -1;
    }

    p->ip4h = (IPV4Hdr *)pkt;

    if (unlikely(IPV4_GET_HLEN(p) < IPV4_HEADER_LEN)) {
		oryx_loge(-1,
			"ipv4 header smaller than minimum size %" PRIu8 "", IPV4_GET_HLEN(p));
        ENGINE_SET_INVALID_EVENT(p, IPV4_HLEN_TOO_SMALL);
        return -1;
    }

    if (unlikely(IPV4_GET_IPLEN(p) < IPV4_GET_HLEN(p))) {
		oryx_loge(-1,
			"ipv4 pkt len smaller than ip header size %" PRIu8 "", IPV4_GET_IPLEN(p));
        ENGINE_SET_INVALID_EVENT(p, IPV4_IPLEN_SMALLER_THAN_HLEN);
        return -1;
    }

    if (unlikely(len < IPV4_GET_IPLEN(p))) {
		oryx_loge(-1,
			"truncated ipv4 packet %" PRIu16 "", len);
        ENGINE_SET_INVALID_EVENT(p, IPV4_TRUNC_PKT);
        return -1;
    }

    /* set the address struct */
    SET_IPV4_SRC_ADDR(p,&p->src);
    SET_IPV4_DST_ADDR(p,&p->dst);

#if defined(DECODE_IP4_OPTIONS)
    /* save the options len */
    uint8_t ip_opt_len = IPV4_GET_HLEN(p) - IPV4_HEADER_LEN;
    if (ip_opt_len > 0) {
        IPV4Options opts;
        memset(&opts, 0x00, sizeof(opts));
        DecodeIPV4Options(p, pkt + IPV4_HEADER_LEN, ip_opt_len, &opts);
    }
#endif
    return 0;
}

static __oryx_always_inline__
int DecodeIPv40(ThreadVars *tv, DecodeThreadVars *dtv, Packet *p, uint8_t *pkt, uint16_t len, PacketQueue *pq)
{
	oryx_logd("IPv4");

    oryx_counter_inc(&tv->perf_private_ctx0, dtv->counter_ipv4);
	
    oryx_logd("pkt %p len %"PRIu16"", pkt, len);

    /* do the actual decoding */
    if (unlikely(DecodeIPV4Packet0 (p, pkt, len) < 0)) {
		dump_pkt(GET_PKT(p), GET_PKT_LEN(p));
        p->ip4h = NULL;
        return TM_ECODE_FAILED;
    }
    p->proto = IPV4_GET_IPPROTO(p);

#if defined(HAVE_DEFRAG)
    /* If a fragment, pass off for re-assembly. */
    if (unlikely(IPV4_GET_IPOFFSET(p) > 0 || IPV4_GET_MF(p) == 1)) {
        Packet *rp = Defrag(tv, dtv, p, pq);
        if (rp != NULL) {
            PacketEnqueue(pq, rp);
        }
        p->flags |= PKT_IS_FRAGMENT;
        return TM_ECODE_OK;
    }
#endif
    /* do hdr test, process hdr rules */

#if defined(BUILD_DEBUG)
    if (SCLogDebugEnabled()) { /* only convert the addresses if debug is really enabled */
        /* debug print */
        char s[16], d[16];
        PrintInet(AF_INET, (const void *)GET_IPV4_SRC_ADDR_PTR(p), s, sizeof(s));
        PrintInet(AF_INET, (const void *)GET_IPV4_DST_ADDR_PTR(p), d, sizeof(d));
        oryx_logd("IPV4 %s->%s PROTO: %" PRIu32 " OFFSET: %" PRIu32 " RF: %" PRIu32 " DF: %" PRIu32 " MF: %" PRIu32 " ID: %" PRIu32 "", s,d,
                IPV4_GET_IPPROTO(p), IPV4_GET_IPOFFSET(p), IPV4_GET_RF(p),
                IPV4_GET_DF(p), IPV4_GET_MF(p), IPV4_GET_IPID(p));
    }
#endif /* DEBUG */

    /* check what next decoder to invoke */
    switch (IPV4_GET_IPPROTO(p)) {
        case IPPROTO_TCP:
			oryx_logd("DecodeEthernet TCP");
            DecodeTCP0(tv, dtv, p, pkt + IPV4_GET_HLEN(p),
                      IPV4_GET_IPLEN(p) - IPV4_GET_HLEN(p), pq);
            break;
        case IPPROTO_UDP:
			oryx_logd("DecodeEthernet UDP");
            DecodeUDP0(tv, dtv, p, pkt + IPV4_GET_HLEN(p),
                      IPV4_GET_IPLEN(p) - IPV4_GET_HLEN(p), pq);
            break;
        case IPPROTO_ICMP:
			oryx_logd("DecodeEthernet ICMPv4");
            DecodeICMPv40(tv, dtv, p, pkt + IPV4_GET_HLEN(p),
                         IPV4_GET_IPLEN(p) - IPV4_GET_HLEN(p), pq);
            break;
        case IPPROTO_GRE:
			oryx_logd("DecodeEthernet GRE");
            DecodeGRE0(tv, dtv, p, pkt + IPV4_GET_HLEN(p),
                      IPV4_GET_IPLEN(p) - IPV4_GET_HLEN(p), pq);
            break;
        case IPPROTO_SCTP:
			oryx_logd("DecodeEthernet SCTP");
            DecodeSCTP0(tv, dtv, p, pkt + IPV4_GET_HLEN(p),
                      IPV4_GET_IPLEN(p) - IPV4_GET_HLEN(p), pq);
            break;
        case IPPROTO_IPV6:
			oryx_logd("DecodeEthernet IPv6");
            {
            #if 0
                if (pq != NULL) {
                    /* spawn off tunnel packet */
                    Packet *tp = PacketTunnelPktSetup(tv, dtv, p, pkt + IPV4_GET_HLEN(p),
                            IPV4_GET_IPLEN(p) - IPV4_GET_HLEN(p),
                            DECODE_TUNNEL_IPV6, pq);
                    if (tp != NULL) {
                        PKT_SET_SRC(tp, PKT_SRC_DECODER_IPV4);
                        PacketEnqueue(pq,tp);
                    }
                }
			#endif
                break;
            }
	#if 0
        case IPPROTO_IP:
			oryx_logd("DecodeEthernet IPv4");
            /* check PPP VJ uncompressed packets and decode tcp dummy */
            if(p->ppph != NULL && ntohs(p->ppph->protocol) == PPP_VJ_UCOMP)    {
                DecodeIPv40(tv, dtv, p, pkt + IPV4_GET_HLEN(p),
                          IPV4_GET_IPLEN(p) -  IPV4_GET_HLEN(p), pq);
            }
            break;
	#endif
        case IPPROTO_ICMPV6:
			oryx_logd("DecodeEthernet ICMPv6");
            ENGINE_SET_INVALID_EVENT(p, IPV4_WITH_ICMPV6);
            break;
    }

    return TM_ECODE_OK;
}

#endif

