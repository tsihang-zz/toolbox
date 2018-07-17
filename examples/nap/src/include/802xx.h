#ifndef VLAN_H_H
#define VLAN_H_H

/** Vlan type */
#define ETHERNET_TYPE_VLAN          0x8100

/** Vlan header struct */
typedef struct VLANHdr_ {
    uint16_t vlan_cfi;
    uint16_t protocol;  /**< protocol field */
} __attribute__((__packed__)) VLANHdr;

/** VLAN header length */
#define VLAN_HEADER_LEN 4

typedef struct IEEE8021ahHdr_ {
    uint32_t flags;
    uint8_t c_destination[6];
    uint8_t c_source[6];
    uint16_t type;              /**< next protocol */
}  __attribute__((__packed__)) IEEE8021ahHdr;

#define IEEE8021AH_HEADER_LEN sizeof(IEEE8021ahHdr)

/** Vlan macros to access Vlan priority, Vlan CFI and VID */
#define GET_VLAN_PRIORITY(vlanh)    ((ntohs((vlanh)->vlan_cfi) & 0xe000) >> 13)
#define GET_VLAN_CFI(vlanh)         ((ntohs((vlanh)->vlan_cfi) & 0x0100) >> 12)
#define GET_VLAN_ID(vlanh)          ((uint16_t)(ntohs((vlanh)->vlan_cfi) & 0x0FFF))
#define GET_VLAN_PROTO(vlanh)       ((ntohs((vlanh)->protocol)))

#endif
