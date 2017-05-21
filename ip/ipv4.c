/* ***************************************************************************

  ipv4.c

  IPv4 Network layer services and utilities
  This module include the ICMP implementation for the stack

  May 2017 - Created

*************************************************************************** */

#include    <malloc.h>
#include    <string.h>
#include    <assert.h>

#include    <stdio.h>               // @@ for printf() debug only

#include    "ip/ipv4.h"
#include    "ip/arp.h"
#include    "ip/stack.h"

/* -----------------------------------------
   static functions
----------------------------------------- */
static void ip4_icmp_handler(struct pbuf_t* const, struct net_interface_t* const);

/*------------------------------------------------
 * ip4_input()
 *
 *  IPv4 input packet handler
 *  The handler examines the IP packet, validates IP header and
 *  forwards to TCP, UDP transport handling or processes an ICMP function
 *
 * param:  pointer to the packet's pbuf and the source network interfaces
 * return: none
 *
 */
void ip4_input(struct pbuf_t* const p, struct net_interface_t* const netif)
{
    struct ip_header_t      *ip;
    uint16_t                 chksum;
    int                      ipHeaderLen;

    PRNT_FUNC;

    ip = (struct ip_header_t*) &(((struct ethernet_frame_t*)(p->pbuf))->payloadStart); // pointer to IP packet header

    /*
     * 1. validate header checksum, drop packet if bad
     * 2. switch on 'protocol' field and handle TCP, UDP or ICMP inputs
     * 3. call protocol handlers with pointer to ip packet
     *    (no need to pass forward the pbuf structure, 'free'ing it will take place where is was 'allocated'
     *      at the input. so just passing 'struct ip_header_t* const')
     * 4. @@ handling of fragmented packets here?
     *
     */

    ipHeaderLen = (ip->verHeaderLength & 0x0f) * 4;         // calculate header length
    chksum = stack_checksum(ip, ipHeaderLen);               // calculate header checksum
    if ( chksum != 0xffff )                                 // drop packet if checksum is wrong
    {
        return;                                             // @@ report/record checksum error?
    }
/*
    printf(" [ip] protocol %02x src %08lx dest %08lx\n",
         ip->protocol,
         ip->srcIp,
         ip->destIp);
*/
    switch ( ip->protocol )                                 // get IP packet protocol
    {
        case IP4_ICMP:                                      // handle ICMP requests
            ip4_icmp_handler(p, netif);
            break;

        case IP4_UDP:                                       // handle UDP requests
            printf("UDP\n");
            break;

        case IP4_TCP:                                       // handle TCP requests
            printf("TCP");
            break;

        default:;                                           // drop everything else @@ handle unidentified frame type
    }
}

/*------------------------------------------------
 * ip4_output()
 *
 *  The function outputs an IPv4 packet, formed in a pbuf
 *  The packet starting with an IPv4 header should be formed in a regular pbuf
 *  leaving room at the top of the pbuf for a frame header, and then passed to this function for output.
 *  ip4_output() will search all available network interfaces to match destination
 *  IP address with the interface subnet; if no appropriate subnet is found, the first
 *  interface's gateway address will be used.
 *  Output will be done through arp_output() for address resolution, a return of ERR_ARP_NONE
 *  from address resolution should be propagated and trigger a packet resend
 *
 * param:  pointer to output pbuf
 * return: ERR_OK if send was successful, ip4_err_t on error
 *
 */
ip4_err_t ip4_output(struct pbuf_t* const p)
{
    ip4_err_t       result = ERR_OK;

    return result;
}

/*------------------------------------------------
 * ip4_icmp_handler()
 *
 *  ICMP request and reply handler
 *  accepts ICMP requests and responds to them
 *
 * param:  pointer to the IP packet start point inside the pbuf data area
 *         *not* a pointer to the pbuf itself!
 *         network interface the packet came from
 * return: none
 *
 */
static void ip4_icmp_handler(struct pbuf_t* const p, struct net_interface_t* const netif)
{
    struct ip_header_t  *ip_in;
    struct icmp_t       *icmp_in;
    struct ip_header_t  *ip_out;
    struct icmp_t       *icmp_out;
    struct pbuf_t       *q;
    uint16_t             ipHeaderLen;
    uint16_t             payloadLen;
    ip4_err_t            result;

    PRNT_FUNC;

    ip_in = (struct ip_header_t*) &(((struct ethernet_frame_t*)(p->pbuf))->payloadStart); // pointer to IP packet header

    ipHeaderLen = (ip_in->verHeaderLength & 0x0f) * 4;                          // calculate header length in bytes
    icmp_in = (struct icmp_t*)(((uint8_t*) ip_in) + ipHeaderLen);               // pointer the ICMP header

    payloadLen = p->len - FRAME_HDR_LEN - ipHeaderLen - ICMP_HDR_LEN;           // calculate ICMP payload length in bytes

/*
    printf(" p->len=%d ipHeaderLen=%d payloadLen=%d\n",
            p->len,
            ipHeaderLen,
            payloadLen);
*/

    switch ( stack_byteswap(icmp_in->type_code) )
    {
        case ECHO_REQ:                                                          // handle ICMP ping request by responding to it
            q = pbuf_allocate(TX);                                              // allocate a pbuf and establish pointers
            ip_out = (struct ip_header_t*) &(q->pbuf[FRAME_HDR_LEN]);
            icmp_out = (struct icmp_t*) &(ip_out->payloadStart);

            ip_out->verHeaderLength = IP_VER + IP_IHL;                          // populate reply's IP header
            ip_out->qos = IP_QOS;
            ip_out->length = stack_byteswap(IP_HDR_LEN + ICMP_HDR_LEN + payloadLen);
            ip_out->id = ip_in->id;
            ip_out->defrag = stack_byteswap(0 | IP_FLAG_DF);
            ip_out->ttl = IP_TTL;
            ip_out->protocol = IP4_ICMP;
            ip_out->checksum = 0;                                               // replace after calculating
            ip_out->srcIp = ip_in->destIp;                                      // @@ should this be netif->ip4addr?
            ip_out->destIp = ip_in->srcIp;

            icmp_out->type_code = stack_byteswap(ECHO_REPLY);                   // populate ICMP reply header
            icmp_out->checksum = 0;                                             // replace after calculating
            icmp_out->id = icmp_in->id;
            icmp_out->seq = icmp_in->seq;

            memcpy(&(icmp_out->payloadStart),
                   &(icmp_in->payloadStart),
                   payloadLen);                                                 // copy payload

            ip_out->checksum = ~(stack_checksum(ip_out, IP_HDR_LEN));           // calculate checksums
            icmp_out->checksum = ~(stack_checksum(icmp_out, ICMP_HDR_LEN + payloadLen));

            q->len = FRAME_HDR_LEN + IP_HDR_LEN + ICMP_HDR_LEN + payloadLen;    // set packet length

            result = arp_output(netif, q);                                      // send the packet through the interface it came from

            pbuf_free(q);                                                   // free the transmit buffer
            break;

        case ECHO_REPLY:                                                        // @@ we got a reply from our ICMP ping
            break;                                                              // @@ check if ping app registered to get a response, drop for now

        default:;                                                               // drop everything else
    }
}
