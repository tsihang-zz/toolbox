#include "oryx.h"#include "dp_decode.h"#if 0const struct eth_decode_ops decode_ops = {	.decode_eth				= DecodeEthernet0,	.decode_arp				= DecodeARP0,	.decode_dsa				= DecodeMarvellDSA0,	.decode_vlan			= DecodeVLAN0,	.decode_ip4				= DecodeIPv40,	.decode_ip6				= DecodeIPv60,	.decode_gre				= DecodeGRE0,	.decode_icmp4			= DecodeICMPv40,	.decode_icmp6			= DecodeICMPv60,	.decode_tcp				= DecodeTCP0,	.decode_udp				= DecodeUDP0,	.decode_sctp			= DecodeSCTP0,	.decode_mpls			= DecodeMPLS0,	.decode_ppp				= DecodePPP0,	.decode_pppoe_session	= DecodePPPoESession0,	.decode_pppoe_disc		= DecodePPPoEDiscovery0,};#endif/**
 * \brief Return a malloced packet.
 */static
void PacketFree(Packet *p)
{
    p = p;
}

/**
 * \brief Get a malloced packet.
 *
 * \retval p packet, NULL on error
 */
Packet *PacketGetFromAlloc(void)
{
    	Packet *p = malloc(DEFAULT_PACKET_SIZE);
    	if (unlikely(p == NULL)){		
return NULL;
    }	
memset(p, 0, DEFAULT_PACKET_SIZE);
    //PACKET_INITIALIZE(p);
    p->flags |= PKT_ALLOC;

    //PACKET_PROFILING_START(p);
    return p;
}

