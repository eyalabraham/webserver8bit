/* ***************************************************************************

  ip4stack.c

  Ipv4 stack services and utilities

  May 2017 - Created

*************************************************************************** */

#include    <malloc.h>
#include    <assert.h>
#include    <string.h>
#include    <stdio.h>

#include    "ip/stack.h"
#include    "ip/options.h"

#if  SYSTEM_DOS
#include    <dos.h>
#endif
#if  SYSTEM_LMTE
#include    "lmte.h"
#endif

/* -----------------------------------------
   module globals
----------------------------------------- */
struct ip4stack_t   stack;                                          // IP stack data structure

static struct pbuf_t       pBuf[PACKET_BUFS];                    // transmit and receive buffer pointers


/*------------------------------------------------
 * stack_init()
 *
 *  Initialize the IPv4 stack.
 *  Must be called at the beginning of the program before using any
 *  IP stack functionality.
 *
 */
void stack_init(void)
{
    int     i;

    // initialize IP stack structure
    memset(&stack, 0, sizeof(struct ip4stack_t));
    strncpy(HOSTNAME, stack.hostname, HOSTNAME_LENGTH);
    stack.interfaceCount = INTERFACE_COUNT;

    // initialize buffer allocation
    // test for valid range and initialize data structures
    assert((PACKET_BUFS > 0) && (PACKET_BUFS <= MAX_PBUFS));
    for (i = 0; i < PACKET_BUFS; i++)
        pBuf[i].len = PBUF_FREE;
}

/*------------------------------------------------
 * stack_get_ethif()
 *
 *  return a pointer to an interface on the stack in slot 'num'
 *
 *  param:  slot number
 *  return: pointer to network interface structure or NULL if failed
 *
 */
struct net_interface_t* const stack_get_ethif(uint8_t num)
{
    if ( num >= INTERFACE_COUNT )                           // slot number does not exist
        return NULL;

    return &(stack.interfaces[num]);                        // return pointer to interface
}

/*------------------------------------------------
 * stack_set_route()
 *
 *  setup a route table entry
 *
 *  param:  subnet mask and gateway in four octet format, network interface number
 *  return: ERR_OK if entry was set or ip4_err_t if value if not
 *
 */
ip4_err_t stack_set_route(ip4_addr_t nm, ip4_addr_t gw, uint8_t netif_num)
{
    int         i;
    ip4_err_t   result = ERR_RT_FULL;

    for (i = 0; i < ROUTE_TABLE_LENGTH; i++)            // scan the route table
    {
        if ( stack.routeTable[i].destNet == 0 )         // for en empty slot
        {
            stack.routeTable[i].destNet = gw & nm;      // an populate with the route parameters
            stack.routeTable[i].netMask = nm;
            stack.routeTable[i].gateway = gw;
            stack.routeTable[i].netIf   = netif_num;
            result = ERR_OK;
        }
    }

    return result;
}

/*------------------------------------------------
 * stack_get_route()
 *
 *  return a pointer to a route table entry
 *
 *  param:  zero-based entry index number
 *  return: NULL if index out of range, pointer to an entry if in range of table length
 *          the entry could be empty (all zeros)
 *
 */
struct route_tbl_t* const stack_get_route(uint8_t route)
{
    if ( route >= ROUTE_TABLE_LENGTH )                              // check if entry is valid
        return NULL;

    return ( stack.routeTable[route].gateway != 0 ? &(stack.routeTable[route]) : NULL );    // return a pointer to it
}

/*------------------------------------------------
 * stack_clear_route()
 *
 *  clear a route table entry
 *
 *  param:  zero-based entry index number
 *  return: ERR_OK if entry was set or ip4_err_t if value if not
 *
 */
ip4_err_t stack_clear_route(uint8_t route)
{
    if ( route >= ROUTE_TABLE_LENGTH )                  // check if entry is valid
        return ERR_RT_RANGE;

    stack.routeTable[route].destNet = 0;                // clear the route parameters
    stack.routeTable[route].netMask = 0;
    stack.routeTable[route].gateway = 0;
    stack.routeTable[route].netIf   = 0;

    return ERR_OK;
}

/*------------------------------------------------
 * stack_time()
 *
 *  return stack time in mSec
 *
 *  param:  none
 *  return: 32bit system clock in mSec
 *
 */
uint32_t stack_time(void)
{
#if  SYSTEM_DOS
    struct dostime_t    sysTime;                    // hold system time for timeout calculations
    uint32_t            dosTimetic;                 // DOS time tick temp

    _dos_gettime(&sysTime);                         // get system time
    dosTimetic = (uint32_t) (10 * sysTime.hsecond) +
                 (uint32_t) (1000 * sysTime.second) +
                 (uint32_t) (60000 * sysTime.minute) +
                 (uint32_t) (3600000 * sysTime.hour);
    return dosTimetic;
#endif  /* SYSTEM_DOS */
#if  SYSTEM_LMTE
    return getGlobalTicks();                        // return LMTE global tick count
#endif  /* SYSTEM_LMTE */
}

/*------------------------------------------------
 * stack_timers()
 *
 *  handle stack timers and timeouts for all network interfaces
 *  go through all defined interfaces on this stack and through all
 *  registered timers and timeouts
 *  invoke appropriate handlers that were registered by the protocols
 *
 *  param:  none
 *  return: none
 *
 */
void stack_timers(void)
{
}

/*------------------------------------------------
 * stack_set_protocol_handler()
 *
 *  register a call-back function handler for protocol inputs
 *
 *  param:  protocol for which this handler is being registered, pointer to handler call-back function
 *  return: none
 *
 */
void stack_set_protocol_handler(ip4_protocol_t protocol, void (*input_handler)(struct pbuf_t* const))
{
    switch ( protocol )
    {
        case IP4_ICMP:
            stack.icmp_input_handler = input_handler;
            break;

        case IP4_UDP:
            stack.udp_input_handler = input_handler;
            break;

        case IP4_TCP:
            stack.tcp_input_handler = input_handler;
            break;

        default:;
    }
}

/*------------------------------------------------
 * pbuf_allocate()
 *
 *  find a free packet buffer from the static pool
 *  of buffers and return a pointer to it.
 *  the allocation is a simple scan over a list of pointers
 *  to find a slot that is not assigned (is NULL)
 *
 *  param:  none
 *  return: pointer to packet buffer or NULL if failed
 *
 */
struct pbuf_t* const pbuf_allocate(void)
{
    struct pbuf_t *p = NULL;                        // return NULL if not free buffers
    int            i;

    for (i = 0; i < PACKET_BUFS; i++)               // scan the list of pointers for a free slot
    {
        if ( pBuf[i].len == PBUF_FREE )             // if slot is free
        {
            pBuf[i].len = PBUF_MARKED;              // mark as in use
            p = &(pBuf[i]);                         // get pbuf pointer for return
            break;                                  // exit loop
        }
    }

    assert(p);                                      // @@ keep this here for a little while...

    return p;
}

/*------------------------------------------------
 * pbuf_free()
 *
 *  free an allocated packet buffer on the static pool
 *  of buffers.
 *
 *  param:  pbuf pointer to free and buffer type 'RX' or 'TX' pool
 *
 */
void pbuf_free(struct pbuf_t* const p)
{
    p->len = PBUF_FREE;
}

/*------------------------------------------------
 * stack_byteswap()
 *
 *  big-endian to/from little-endian, 16bit bytes swap
 *
 *  param:  uint16 in big-endian
 *  return: uint16 in little-endian
 *
 */
uint16_t stack_byteswap(uint16_t w)
{
    uint16_t    temp;

    temp  = (uint16_t)(w >> 8) & 0x00ff;
    temp |= (uint16_t)(w << 8) & 0xff00;

    return temp;
}

/*------------------------------------------------
 * stack_checksum()
 *
 * Calculates checksum for 'len' bytes starting at 'dataptr'
 * accumulator size limits summable length to 64k
 * host endianess is irrelevant (p3 RFC1071)
 * ** the caller must invert bits for Internet sum ! **
 * source: LwIP v2.0.2 inet_checksum.c
 *
 * param:  dataptr points to start of data to be summed at any boundary
 *         len length of data to be summed
 * return: host order (!) checksum (non-inverted Internet sum)
 *
 */
uint16_t stack_checksum(const void *dataptr, int len)
{
    uint32_t        acc;
    uint16_t        src;
    const uint8_t  *octetptr;

    acc = 0;
    /* dataptr may be at odd or even addresses */
    octetptr = (const uint8_t*)dataptr;
    while (len > 1)
    {
        /* declare first octet as most significant
           thus assume network order, ignoring host order */
        src = (*octetptr) << 8;
        octetptr++;
        /* declare second octet as least significant */
        src |= (*octetptr);
        octetptr++;
        acc += src;
        len -= 2;
    }

    if (len > 0)
    {
        /* accumulate remaining octet */
        src = (*octetptr) << 8;
        acc += src;
    }

    /* add deferred carry bits */
    acc = (acc >> 16) + (acc & 0x0000ffffUL);
    if ((acc & 0xffff0000UL) != 0)
    {
        acc = (acc >> 16) + (acc & 0x0000ffffUL);
    }

    /* reorder sum, the caller must invert bits for Internet sum ! */
    return stack_byteswap((uint16_t)acc);
}

/*------------------------------------------------
 * stack_ip4addr_ntoa()
 *
 *  this function converts an IP address from a 32bit representation
 *  to a string 'dot' notation representation
 *  source: https://github.com/espressif/esp-idf/blob/master/components/lwip/core/ipv4/ip4_addr.c
 *
 *  param:  IP address in 32bit representation, pointer to output string, string length
 *          length of the string should have space for 'xxx.xxx.xxx.xxx\0'
 *          a '\0' will always be inserted at the last string position as a terminator.
 *  return: pointer to output string
 *
 */
char* stack_ip4addr_ntoa(ip4_addr_t s_addr, char* const buf, uint8_t buflen)
{
    char        inv[3];
    char       *rp;
    uint8_t    *ap;
    uint8_t     rem;
    uint8_t     n;
    uint8_t     i;
    int         len = 0;

    if ( n > 0 )
    {
        rp = buf;
        ap = (uint8_t *) &s_addr;
        for (n = 0; n < 4; n++)
        {
            i = 0;
            do
            {
                rem = *ap % (uint8_t)10;
                *ap /= (uint8_t)10;
                inv[i++] = '0' + rem;
            } while (*ap);

            while (i--)
            {
                if (len++ >= buflen)
                {
                    buf[0] = 0;
                    return NULL;
                }
                *rp++ = inv[i];
            }

            if (len++ >= buflen)
            {
                buf[0] = 0;
                return NULL;
            }
            *rp++ = '.';
            ap++;
        }

        *--rp = 0;
    }
    else
        buf[0] = 0;

    return buf;
}

/*------------------------------------------------
 * inputStub()
 *
 *  this function is a stub function for ethernet input.
 *  received packets will be forwarded to this function and be
 *  dropped (or can be displayed for debug...)
 *
 *  param:  pbuf pointer to received data and the network interface structure
 *  return: none
 *
 */
void inputStub(struct pbuf_t* const p, struct net_interface_t* const netif)     // @@ don't think we need a 'netif' here?
{
    int     i;

    printf("");

    printf("inputStub()\n dst: ");
    for (i = 0; i < 6; i++)
        printf("%02x ", (p->pbuf)[i]);
    printf("\n src: ");
    for (i = 6; i < 12; i++)
        printf("%02x ", (p->pbuf)[i]);
    printf("\n typ: ");
    for (i = 12; i < 14; i++)
        printf("%02x ", (p->pbuf)[i]);
    printf("\n");
}

/*------------------------------------------------
 * outputStub()
 *
 *  this function is a stub function for ethernet output.
 *  received packets will be forwarded to this function and be
 *  dropped (or can be displayed for debug...)
 *
 *  param:  pbuf pointer to received data and the network interface structure
 *  return: ERR_OK on success or other status on failure.
 *
 */
ip4_err_t outputStub(struct net_interface_t* const netif, struct pbuf_t* const p)
{
    int     i;

    printf("");

    printf("outputStub()\n dst: ");
    for (i = 0; i < 6; i++)
        printf("%02x ", (p->pbuf)[i]);
    printf("\n src: ");
    for (i = 6; i < 12; i++)
        printf("%02x ", (p->pbuf)[i]);
    printf("\n typ: ");
    for (i = 12; i < 14; i++)
        printf("%02x ", (p->pbuf)[i]);
    printf("\n");

    return ERR_OK;
}
