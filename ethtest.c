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
#include    <time.h>

#include    "ip/netif.h"

#include    "ip/stack.h"
#include    "ip/arp.h"
#include    "ip/icmp.h"
#include    "ip/udp.h"

#include    "ip/error.h"
#include    "ip/types.h"

#include    "ppispi.h"

/* -----------------------------------------
   definitions
----------------------------------------- */
// test application
enum {ARP, PING, NTP} whatToDo;
#define     USAGE                   "ethtest -r {ARP and PING response} | -p {ping an address} | -t {NTP}\n"

// PING
#define     WAIT_FOR_PING_RESPONSE  1000            // in mSec
#define     TEXT_PAYLOAD_LEN        30
#define     PING_TEXT               "Ping from FlashLite v25\0"

// NTP
#define     NTP_PORT                123             // NTP port number
#define     NTP_MINPOLL             4               // minimum poll exponent (16 s)
#define     NTP_MAXPOLL             17              // maximum poll exponent (36 h)
#define     NTP_MAXDISP             16              // maximum dispersion (16 s)
#define     NTP_MINDISP             .005            // minimum dispersion increment (s)
#define     NTP_MAXDIST             1               // distance threshold (1 s)
#define     NTP_MAXSTRAT            16              // maximum stratum number
#define     NTP_REQUEST_INTERVAL    20000           // mSec

#define     NTP_LI_NONE             0x00            // NTP Leap Second indicator (b7..b6)
#define     NTP_LI_ADD_SEC          0x40
#define     NTP_LI_SUB_SEC          0x80
#define     NTP_LI_UNKNOWN          0xc0

#define     NTP_VERSION4            0x20            // NTP version number (b5..b3)
#define     NTP_VERSION3            0x18

#define     NTP_MODE_RES            0x00            // NTP mode (b2..b0)
#define     NTP_MODE_SYMACT         0x01
#define     NTP_MODE_SYMPASV        0x02
#define     NTP_MODE_CLIENT         0x03
#define     NTP_MODE_SERVER         0x04
#define     NTP_MODE_BCAST          0x05
#define     NTP_MODE_CTRL           0x06
#define     NTP_MODE_PRIV           0x07

#define DIFF_SEC_1900_1970         (2208988800UL)   // number of seconds between 1900 and 1970 (MSB=1)
#define DIFF_SEC_1970_2036         (2085978496UL)   // number of seconds between 1970 and Feb 7, 2036 (6:28:16 UTC) (MSB=0)


/* -----------------------------------------
   types and data structures
----------------------------------------- */
// PING
struct ping_payload_t
{
    uint32_t    time;
    char        payload[TEXT_PAYLOAD_LEN];
} pingPayload;

// NTP
// derived from: https://tools.ietf.org/html/rfc5905
struct ntp_short_t
{
    uint16_t    seconds;
    uint16_t    fraction;
};

struct ntp_timestamp_t
{
    uint32_t    seconds;
    uint32_t    fraction;
};

struct ntp_date_t
{
    uint32_t    eraNumber;
    uint32_t    eraOffset;
    uint32_t    eraFraction1;
    uint32_t    eraFraction2;
    uint32_t    eraFraction3;
};

struct ntp_t
{
    uint8_t                 flagsMode;
    uint8_t                 stratum;
    uint8_t                 poll;
    int8_t                  precision;
    struct ntp_short_t      rootDelay;
    struct ntp_short_t      rootDispersion;
    uint8_t                 referenceID[4];
    struct ntp_timestamp_t  refTimestamp;
    struct ntp_timestamp_t  orgTimestamp;   // T1
    struct ntp_timestamp_t  recTimestamp;   // T2
    struct ntp_timestamp_t  xmtTimestamp;   // T3 (T4 = dstTimestamp is calculated upon packet arrival)
};

/* -----------------------------------------
   globals
----------------------------------------- */

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
            stack_ntoh(ip_in->length),
            ip,
            stack_ntoh(icmp_in->seq),
            ip_in->ttl,
            pingTime);

    rxSeq = stack_ntoh(icmp_in->seq);                                       // signal a valid response
}

/*------------------------------------------------
 * sntp_process()
 *
 *  NTP processing of received timestamp
 *  source LwIP 2.0.2, sntp.c
 *
 * param:  pointer to response pbuf, source IP address and source port
 * return: none
 *
 */
void sntp_process(struct ntp_timestamp_t *receive_timestamp)
{
    uint32_t    rx_secs, t, us;
    int         is_1900_based;
    time_t      tim;

  /* convert NTP time (1900-based) to unix GMT time (1970-based)
   * if MSB is 0, SNTP time is 2036-based!
   */
  rx_secs = stack_ntohl(receive_timestamp->seconds);
  //printf("NTP time, net order: %08lx host order: %08lx\n", receive_timestamp->seconds, rx_secs);
  is_1900_based = ((rx_secs & 0x80000000) != 0);
  t = is_1900_based ? (rx_secs - DIFF_SEC_1900_1970) : (rx_secs + DIFF_SEC_1970_2036);
  tim = t;

  us = stack_ntohl(receive_timestamp->fraction) / 4295;
  //SNTP_SET_SYSTEM_TIME_US(t, us);
  /* display local time from GMT time */
  printf("NTP time: %s", ctime(&tim));
}

/*------------------------------------------------
 * ntp_input()
 *
 *  callback to receive NTP server responses and process time information
 *
 * param:  pointer to response pbuf, source IP address and source port
 * return: none
 *
 */
void ntp_input(struct pbuf_t* const p, const ip4_addr_t srcIP, const uint16_t srcPort)
{
    struct ntp_t   *ntpResponse;

    ntpResponse = (struct ntp_t*) &(p->pbuf[FRAME_HDR_LEN+IP_HDR_LEN+UDP_HDR_LEN]); // crude way to get pointer the NTP response payload
    sntp_process(&(ntpResponse->recTimestamp));
}

/*------------------------------------------------
 * main()
 *
 *
 */
int main(int argc, char* argv[])
{
    int                     linkState, i, done = 0, j = 0;
    uint16_t                ident, seq;
    ip4_err_t               result;
    struct net_interface_t *netif;
    struct udp_pcb_t       *ntp;
    uint32_t                lastNtpRequest;
    struct ntp_t            ntpPayload;

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

    /* parse command line variables
     *
     */
    if ( argc == 1 )
    {
        printf("%s\n", USAGE);
        return -1;
    }

    for ( i = 1; i < argc; i++ )
    {
        if ( strcmp(argv[i], "-r") == 0 )
        {
            whatToDo = ARP;
        }
        else if ( strcmp(argv[i], "-p") == 0 )
        {
            whatToDo = PING;
            /* prepare for PING response handling
             *
             */
            icmp_ping_init(ping_input);
            ident = 1;
            seq = 0;
            rxSeq = seq;
            strncpy(pingPayload.payload, PING_TEXT, TEXT_PAYLOAD_LEN);
        }
        else if ( strcmp(argv[i], "-t") == 0 )
        {
            whatToDo = NTP;
            /* prepare UDP protocol and
             * initialize for NTP
             *
             */
            udp_init();
            ntp = udp_new();
            assert(ntp);
            assert(udp_bind(ntp, IP4_ADDR(192,168,1,19), 123) == ERR_OK);
            assert(udp_recv(ntp, ntp_input) == ERR_OK);
            lastNtpRequest = stack_time();
        }
        else
        {
            printf("%s\n", USAGE);
            return -1;
        }
    }

    /* test link state and send gratuitous ARP
     *
     */
    linkState = interface_link_state(netif);
    printf("link state = '%s'\n", linkState ? "up" : "down");

    if ( linkState )
        result = arp_gratuitous(netif);                             // if link is 'up' send a Gratuitous ARP with our IP address

    if ( result != ERR_OK )
    {
        printf("arp_gratuitous() failed with %d\n", result);
        done = 1;
    }

    /* main loop
     *
     */
    printf("---- entering main loop ----\n");

    while ( !done )
    {
        /* periodically poll link state and if a change occurred from the last
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
        if ( whatToDo == PING )
        {
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
                    case ERR_TX_COLL:
                    case ERR_TX_LCOLL:
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
        } /* PING application */

        if ( whatToDo == NTP &&
             (stack_time() - lastNtpRequest) > NTP_REQUEST_INTERVAL )
        {
            lastNtpRequest = stack_time();
            // generate an NTP request
            ntpPayload.flagsMode = NTP_LI_UNKNOWN | NTP_VERSION3 | NTP_MODE_CLIENT;
            ntpPayload.stratum = 0;
            ntpPayload.poll = 10;
            ntpPayload.precision = -6;                                      // approx 18mSec (DOS clock tick)
            ntpPayload.rootDelay.seconds = stack_ntoh(0x0001);
            ntpPayload.rootDelay.fraction = 0;
            ntpPayload.rootDispersion.seconds = stack_ntoh(0x0001);
            ntpPayload.rootDispersion.fraction = 0;
            ntpPayload.referenceID[0] = 0;
            ntpPayload.referenceID[1] = 0;
            ntpPayload.referenceID[2] = 0;
            ntpPayload.referenceID[3] = 0;
            ntpPayload.refTimestamp.seconds = 0;
            ntpPayload.refTimestamp.fraction = 0;
            ntpPayload.orgTimestamp.seconds = 0;
            ntpPayload.orgTimestamp.fraction = 0;
            ntpPayload.recTimestamp.seconds = 0;
            ntpPayload.recTimestamp.fraction = 0;
            ntpPayload.xmtTimestamp.seconds = 0;
            ntpPayload.xmtTimestamp.fraction = 0;

            result = udp_sendto(ntp,(uint8_t*) &ntpPayload, sizeof(struct ntp_t), IP4_ADDR(216,229,0,179), NTP_PORT);
            switch ( result )
            {
                case ERR_OK:                                                // keep sending NTP requests periodically
                    whatToDo = ARP;                                         // do this only once for testing, but stay in the loop to receive response
                    break;

                case ERR_ARP_NONE:                                          // a route to the NTP server destination or gateway was not found, ARP was sent
                    printf("ARP request sent for NTP server target\n");
                    break;

                case ERR_NETIF:
                case ERR_NO_ROUTE:
                case ERR_MEM:
                case ERR_DRV:
                case ERR_TX_COLL:
                case ERR_TX_LCOLL:
                    printf("error code %d\n", result);
                    done = 1;
                    break;

                default:
                    printf("unexpected error code %d\n", result);
                    done = 1;
            }
        } /* NTP time protocol */
    } /* main loop */

    return 0;
}
