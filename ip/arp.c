/* ***************************************************************************

  arp.c

  network ARP protocol code module

  May 2017 - Created

*************************************************************************** */

#include    <malloc.h>
#include    <string.h>
#include    <assert.h>

#include    <stdio.h>               // @@ for printf() debug only

#include    "ip/arp.h"
#include    "ip/stack.h"

/* -----------------------------------------
 * arp_query()
 *
 * This function searches the ARP table for and IP address
 * that matches ipArrd and return a pointer to the HW address.
 * The function uses the IP address as the key for the serach
 *
 * param:  netif the network interface and IP address to search
 * return: pointer to HW address if found, or NULL is not
 *
 */
hwaddr_t* const arp_query(struct net_interface_t* const netif,
                          ip4_addr_t ipAddr)
{
    int         i;
    hwaddr_t   *result = NULL;

    for (i = 0; i < ARP_TABLE_LENGTH; i++)
    {
        if ( netif->arpTable[i].ipAddress == ipAddr )   // check if IP address exists
        {
            netif->arpTable[i].lru++;                   // increment LRU count
            result = netif->arpTable[i].hwAddress;      // return pointer to HW address
            break;
        }
    }

    return result;
}

/* -----------------------------------------
 * arp_add_entry()
 *
 * This function adds an ARP entry to the table.
 * the function uses the ipAddress as the key and the
 * hwAddr and flags as the values.
 *
 * param:  netif the network interface and IP and HW address and flags
 * return: ERR_OK if entry was added, ip4_err_t if not
 *
 */
ip4_err_t arp_add_entry(struct net_interface_t* const netif,
                        ip4_addr_t ipAddr,
                        hwaddr_t hwAddr,
                        arp_flags_t flags)
{
    int         i;
    ip4_err_t   result = ERR_ARP_FULL;

    for (i = 0; i < ARP_TABLE_LENGTH; i++)
    {
        if ( (netif->arpTable[i].flags & (ARP_FLAG_STATIC | ARP_FLAG_DYNA)) == 0 )  // check if table entry is free
        {
            netif->arpTable[i].ipAddress = ipAddr;                                  // add the ARP information
            copy_hwaddr(netif->arpTable[i].hwAddress, hwAddr);
            netif->arpTable[i].flags |= flags;
            netif->arpTable[i].lru = 0;
            result = ERR_OK;
            break;
        }
    }

    /* scan LRU values, and evict
     * a slot with ARP_FLAG_DYNA and the lowest LRU count
     *
     */

    return result;
}

/* -----------------------------------------
 * arp_update_entry()
 *
 * This function updates an ARP entry in the table.
 * the function uses the ipAddress as the key and the hwAddr as the value
 *
 * param:  netif the network interface and IP and HW address and flags
 * return: ERR_OK if entry was added, ip4_err_t if not
 *
 */
ip4_err_t arp_update_entry(struct net_interface_t* const netif,
                           ip4_addr_t ipAddr,
                           hwaddr_t hwAddr,
                           arp_flags_t flags)
{
    int         i;
    ip4_err_t   result = ERR_ARP_NONE;

    for (i = 0; i < ARP_TABLE_LENGTH; i++)
    {
        if ( netif->arpTable[i].ipAddress == ipAddr )           // check if IP address exists
        {
            copy_hwaddr(netif->arpTable[i].hwAddress, hwAddr);  // update the ARP information
            netif->arpTable[i].flags &= ~(ARP_FLAG_STATIC | ARP_FLAG_DYNA);
            netif->arpTable[i].flags |= flags;
            netif->arpTable[i].lru = 0;                         // reset LRU counter
            result = ERR_OK;
            break;
        }
    }

    return result;
}

/* -----------------------------------------
 * arp_show()
 *
 * Repeated calls to this function will walk the ARP table
 *
 * param:  none
 * return: pointer to ARP table record
 *
 */
struct arp_tbl_t* const arp_show(void)
{
    return NULL;
}

/* -----------------------------------------
 * link_packet_handler()
 *
 * This function is the link layer packet handler for handling incoming ARP.
 * packet are forwarded here from the input interface and will be parsed for ARP
 * processing and reply or forwarded to next layer
 *
 * packets arriving here from the link interface should be already
 * filtered to have either broadcast address or our MAC address
 * as their destination.
 * if not, additional filtering/handling should be done here
 * the function only does a minimal check of the fields in the ARP packet
 * such as HTYPE = 1 and PTYPE = 0x0800
 *
 * this function does the following:
 * 1. examine type field
 * 2. process ARP request
 * 3. forward IPV$ type to network layer
 * 4. discard all the rest
 *
 * param:  packet buffer pointer and netif the network interface
 *         structure for this ethernet interface
 * return: none
 *
 */
void arp_packet_handler(struct pbuf_t* const p,
                        struct net_interface_t* const netif)
{
    struct ethernet_frame_t *frame;
    struct ethernet_frame_t *arp_resp_hdr;
    struct arp_t            *arp_resp;
    struct arp_t            *arp;
    struct pbuf_t           *arp_reply;

    frame = (struct ethernet_frame_t*) p->pbuf;     // recast frame structure
    arp   = (struct arp_t*) &frame->payloadStart;   // pointer to ARP data payload

    switch ( BEtoLE(frame->type) )
    {
    case TYPE_ARP:
/*
        printf("ARP\n htype %04x ptype %04x op %04x tpa %08lx myip %08lx\n",
                BEtoLE(arp->htype),
                BEtoLE(arp->ptype),
                BEtoLE(arp->oper),
                arp->tpa,
                netif->ip4addr);
*/
        if ( BEtoLE(arp->htype) != ARP_ETH_TYPE || BEtoLE(arp->ptype) != TYPE_IPV4 )
            break;                                  // drop the packet if not matching on network and protocol type
        switch ( BEtoLE(arp->oper) )                // action is based on the operation indicator
        {
        case ARP_OP_REQUEST:
            if ( arp->tpa == netif->ip4addr )       // is someone looking for us?
            {
                arp_reply = pbuf_allocate(TX);
                assert(arp_reply);                  // @@ error handling on buffer depletion, or just give up and drop packet until buffers free up?
                if ( arp_reply != NULL )
                {
                    arp_resp_hdr = (struct ethernet_frame_t*) arp_reply->pbuf;

                    copy_hwaddr(arp_resp_hdr->dest, frame->src);                // build header
                    copy_hwaddr(arp_resp_hdr->src, netif->hwaddr);
                    arp_resp_hdr->type = BEtoLE(TYPE_ARP);

                    arp_resp = (struct arp_t*) &(arp_resp_hdr->payloadStart);   // build ARP response packet
                    arp_resp->htype = BEtoLE(ARP_ETH_TYPE);
                    arp_resp->ptype = BEtoLE(TYPE_IPV4);
                    arp_resp->hlen = ARP_HLEN;
                    arp_resp->plen = ARP_PLEN;
                    arp_resp->oper = BEtoLE(ARP_OP_REPLY);
                    copy_hwaddr(arp_resp->sha, netif->hwaddr);
                    arp_resp->spa = netif->ip4addr;
                    copy_hwaddr(arp_resp->tha, arp->sha);
                    arp_resp->tpa = arp->spa;

                    arp_reply->len = sizeof(struct ethernet_frame_t) + sizeof(struct arp_t);
                    netif->linkoutput(netif->state, arp_reply);                 // send the ARP response
                    pbuf_free(arp_reply);
                }
            }
            break;

        case ARP_OP_REPLY:                          // use replies to update the table
            arp_add_entry(netif, arp->spa, arp->sha, ARP_FLAG_DYNA);
            break;

        default:;                                   // drop the packet
        }
        break;

    case TYPE_IPV4:
        printf(" IPv4\n");
        break;

    default:
        inputStub(p, netif);
    }
}

/* -----------------------------------------
 * arp_output()
 *
 * This function is the link layer packet handler for handling outgoing packets
 * that require address resolution ARP.
 * packet are forwarded here IP addresses are resolved to network MAC addresses
 * and sent using low_level_output()
 *
 * param:  packet buffer pointer and netif the network interface
 *         structure for this ethernet interface
 * return: ERR_OK if no errors or error level from ip4_err_t
 *
 */
ip4_err_t arp_output(struct net_interface_t* const netif,
                     struct pbuf_t* const p)
{
    struct ethernet_frame_t *frame;
    struct ip_packet_t      *ipHeader;
    struct arp_t            *arpReq;
    struct pbuf_t           *arpQ;
    hwaddr_t                *hwaddr;
    ip4_err_t                result;

    frame = (struct ethernet_frame_t*) p->pbuf;                 // establish pointer to etherner frame
    ipHeader = (struct ip_packet_t*) &(frame->payloadStart);    // establish pointer to IP header

    hwaddr = arp_query(netif, ipHeader->destIp);                // extract destination IP and search the ARP table

    if ( hwaddr )                                               // was and entry found in the table?
    {                                                           // yes...
        copy_hwaddr(frame->src, netif->hwaddr);                 // copy source HW address as our address
        copy_hwaddr(frame->dest, hwaddr);                       // copy destination HW address
        frame->type = BEtoLE(TYPE_IPV4);                        // IPv4 frame type
        result = netif->linkoutput(netif->state, p);            // send the frame *** p->len should already be set ***
        pbuf_free(p);                                           // free the pbuf
    }
    else
    {                                                           // no...
        pbuf_free(p);                                           // discard the frame to be sent ( @@ is this the right thing to do? )
        arpQ = pbuf_allocate(TX);                               // allocate a pbuf for an ARP query
        frame = (struct ethernet_frame_t*) arpQ->pbuf;          // establish pointer to etherner frame
        arpReq = (struct arp_t*) &(frame->payloadStart);        // establish pointer to ARP request packet

        copy_hwaddr(frame->dest, netif->broadcast);             // build request header as broadcast
        copy_hwaddr(frame->src, netif->hwaddr);
        frame->type = BEtoLE(TYPE_ARP);

        arpReq->htype = BEtoLE(ARP_ETH_TYPE);                   // build ARP request packet
        arpReq->ptype = BEtoLE(TYPE_IPV4);
        arpReq->hlen = ARP_HLEN;
        arpReq->plen = ARP_PLEN;
        arpReq->oper = BEtoLE(ARP_OP_REQUEST);
        copy_hwaddr(arpReq->sha, netif->hwaddr);
        arpReq->spa = netif->ip4addr;
        copy_hwaddr(arpReq->tha, netif->broadcast);
        arpReq->tpa = ipHeader->destIp;

        arpQ->len = sizeof(struct ethernet_frame_t) + sizeof(struct arp_t);
        result = netif->linkoutput(netif->state, arpQ);         // send the frame
        pbuf_free(arpQ);                                        // free the pbuf
    }

    return result;
}
