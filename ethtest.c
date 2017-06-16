/*
 *  ethtest.c
 *
 *      test program for V25 FlashLite board to test ENC28J60 ethernet
 *
 */

#include    <stdlib.h>
#include    <stdio.h>
#include    <string.h>
#include    <assert.h>
#include    <conio.h>
#include    <dos.h>

#include    "ip/stack.h"
#include    "ip/arp.h"
#include    "ip/icmp.h"
#include    "ip/error.h"
#include    "ip/types.h"
#include    "ppispi.h"

#include    "ip/netif.h"

/* -----------------------------------------
   definitions and globals
----------------------------------------- */
#define     WAIT_FOR_PING_RESPONSE  1000                                    // in mSec

#define     TEXT_PAYLOAD_LEN        30
#define     PING_TEXT               "Ping from FlashLite v25\0"

struct ping_payload_t
{
    uint32_t    time;
    char        payload[TEXT_PAYLOAD_LEN];
} pingPayload;

uint16_t        rxSeq;
char            ip[17];

/*------------------------------------------------
 * ping_input()
 *
 *  Ping response handler.
 *  Register as a call-back function
 *
 * param:  pointer response pbuf
 * return: none
 *
 */
void ping_input(struct pbuf_t* const p)
{
    struct ip_header_t *ip_in;
    struct icmp_t      *icmp_in;
    uint32_t            pingTime;

    pingTime = stack_time();

    ip_in = (struct ip_header_t*) &(p->pbuf[FRAME_HDR_LEN]);                // get pointers to data sections in packet
    icmp_in = (struct icmp_t*) &(p->pbuf[FRAME_HDR_LEN + IP_HDR_LEN]);

    stack_ip4addr_ntoa(ip_in->srcIp, ip, 17);                               // print out response in Ping format
    pingTime -= ((struct ping_payload_t*) &(icmp_in->payloadStart))->time;
    printf("%d bytes from %s: icmp_seq=%d ttl=%d time=%lu ms\n",
            stack_byteswap(ip_in->length),
            ip,
            stack_byteswap(icmp_in->seq),
            ip_in->ttl,
            pingTime);

    rxSeq = stack_byteswap(icmp_in->seq);                                   // signal a valid response
}

/*------------------------------------------------
 * main()
 *
 *
 */
void main(int argc, char* argv[])
{
    int                     linkState, i, done = 0, j = 0;
    uint16_t                ident, seq;
    ip4_err_t               result;
    struct net_interface_t *netif;

    printf("Build: ethtest.exe %s %s\n", __DATE__, __TIME__);

    spiIoInit();                                                    // initialize SPI interface

    stack_init();                                                   // initialize IP stack
    assert(stack_set_route(IP4_ADDR(255,255,255,0),
                           IP4_ADDR(192,168,1,1),
                           0) == ERR_OK);                           // setup default route
    netif = stack_get_ethif(0);                                     // get pointer to interface 0
    assert(netif);

    assert(interface_init(netif) == ERR_OK);                        // initialize interface and link HW
    interface_set_addr(netif, IP4_ADDR(192,168,1,19),               // setup static IP addressing
                              IP4_ADDR(255,255,255,0),
                              IP4_ADDR(192,168,1,1));

    icmp_ping_init(ping_input);                                     // register call-back for Ping responses
    ident = 1;
    seq = 0;
    rxSeq = seq;
    strncpy(pingPayload.payload, PING_TEXT, TEXT_PAYLOAD_LEN);

    linkState = interface_link_state(netif);
    printf("link state = '%s'\n", linkState ? "up" : "down");

    if ( linkState )
        result = arp_gratuitous(netif);                             // if link is 'up' send a Gratuitous ARP with our IP address

    if ( result != ERR_OK )
    {
        printf("arp_gratuitous() failed with %d\n", result);
        done = 1;
    }

    printf("---- entering main loop ----\n");

    while ( !done )
    {
        /*
         * periodically poll link state and if a change occurred from the last
         * test propagate the notification
         *
         */
        if ( interface_link_state(netif) != linkState )
        {
            linkState = interface_link_state(netif);
            printf("link state change, now = '%s'\n", linkState ? "up" : "down");
        }

        /* periodically poll for received frames,
         * drop or feed them up the stack for processing
         *
         */
        interface_input(netif);

        /* cyclic timer update and check
         *
         */
        stack_timers();

        /* @@ application goes here?
         *
         */
        if ( rxSeq == seq )
        {
            pingPayload.time = stack_time();
            seq++;
            //result = icmp_ping_output(IP4_ADDR(192,168,1,12), ident, seq, (uint8_t* const) &pingPayload, sizeof(struct ping_payload_t));    // output an ICMP Ping packet
            result = icmp_ping_output(IP4_ADDR(139,130,4,5), ident, seq, (uint8_t* const) &pingPayload, sizeof(struct ping_payload_t));

            switch ( result )
            {
                case ERR_OK:                                            // keep sending ping requests
                    break;

                case ERR_ARP_NONE:                                      // a route to the ping destination was not found, ARP was sent
                    printf("ARP request sent for Ping target\n");
                    break;

                case ERR_NETIF:
                case ERR_NO_ROUTE:
                case ERR_MEM:
                case ERR_DRV:
                    printf("error code %d\n", result);
                    done = 1;
                    break;

                default:
                    printf("unexpected error code %d\n", result);
                    done = 1;
            }
        }
        else if ( (stack_time() - pingPayload.time) > WAIT_FOR_PING_RESPONSE )
        {
            stack_ip4addr_ntoa(netif->ip4addr, ip, 17);
            printf("From %s icmp_seq=%d Destination Host Unreachable\n", ip, seq);
            rxSeq = seq;
        }
    }
}
