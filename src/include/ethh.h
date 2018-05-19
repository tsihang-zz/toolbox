#ifndef ETHERNET_H_H
#define ETHERNET_H_H

#define ETHERNET_HEADER_LEN           14

/* Cisco Fabric Path / DCE header length. */
#define ETHERNET_DCE_HEADER_LEN       ETHERNET_HEADER_LEN + 2

/* Ethernet types -- taken from Snort and Libdnet */
#define ETHERNET_TYPE_PUP             0x0200 /* PUP protocol */
#define ETHERNET_TYPE_IP              0x0800
#define ETHERNET_TYPE_ARP             0x0806
#define ETHERNET_TYPE_BRIDGE          0x6558 /* transparant ethernet bridge (GRE) */
#define ETHERNET_TYPE_REVARP          0x8035
#define ETHERNET_TYPE_EAPOL           0x888e
#define ETHERNET_TYPE_IPV6            0x86dd
#define ETHERNET_TYPE_IPX             0x8137
#define ETHERNET_TYPE_PPPOE_DISC      0x8863 /* discovery stage */
#define ETHERNET_TYPE_PPPOE_SESS      0x8864 /* session stage */
#define ETHERNET_TYPE_8021AD          0x88a8
#define ETHERNET_TYPE_8021AH          0x88e7
#define ETHERNET_TYPE_8021Q           0x8100
#define ETHERNET_TYPE_LOOP            0x9000
#define ETHERNET_TYPE_8021QINQ        0x9100
#define ETHERNET_TYPE_ERSPAN          0x88BE
#define ETHERNET_TYPE_DCE             0x8903 /* Data center ethernet,
                                              * Cisco Fabric Path */
/** These ethernet type defined for ET1500 */
#define ETHERNET_TYPE_0028			  0x0028
#define ETHERNET_TYPE_003A			  0x003A
#define ETHERNET_TYPE_0042			  0x0042
#define ETHERNET_TYPE_C028			  0xC028/* port 1 */
#define ETHERNET_TYPE_C030			  0xC030/* port 2 */
#define ETHERNET_TYPE_C038			  0xC038
#define ETHERNET_TYPE_C040			  0xC040/* port 4 */
#define ETHERNET_TYPE_0028			  0x0028

typedef struct EthernetHdr_ {
    uint8_t eth_dst[6];
    uint8_t eth_src[6];
    uint16_t eth_type;
} __attribute__((__packed__)) EthernetHdr;

#endif
