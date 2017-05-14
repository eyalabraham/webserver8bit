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
static struct ip4stack_t   stack;                                  // IP stack data structure
static struct pbuf_t       txPbuf[TX_PACKET_BUFS];                 // transmit and
static struct pbuf_t       rxPbuf[RX_PACKET_BUFS];                 // receive buffer pointers

/* -----------------------------------------
   static function prototype
----------------------------------------- */


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
    assert((TX_PACKET_BUFS > 0) && (TX_PACKET_BUFS <= MAX_PBUFS));
    assert((RX_PACKET_BUFS > 0) && (RX_PACKET_BUFS <= MAX_PBUFS));
    for (i = 0; i < TX_PACKET_BUFS; i++)
        txPbuf[i].len = PBUF_FREE;
    for (i = 0; i < RX_PACKET_BUFS; i++)
        rxPbuf[i].len = PBUF_FREE;
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
    dosTimetic = (uint32_t) sysTime.hsecond +
                 (uint32_t) (100 * sysTime.second) +
                 (uint32_t) (6000 * sysTime.minute) +
                 (uint32_t) (360000 * sysTime.hour);
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
 * pbuf_allocate()
 *
 *  find a free packet buffer from the static pool
 *  of buffers and return a pointer to it.
 *  the allocation is a simple scan over a list of pointers
 *  to find a slot that is not assigned (in NULL)
 *
 *  param:  buffer type to be 'RX' or 'TX' pool
 *  return: pointer to packet buffer or NULL if failed
 *
 */
struct pbuf_t* const pbuf_allocate(pbuf_type_t type)
{
    struct pbuf_t *p = NULL;                        // return NULL is not free buffers
    int            i;

    switch ( type )
    {
    case TX:
        for (i = 0; i < TX_PACKET_BUFS; i++)        // scan the list of pointers for a free slot
        {
            if ( txPbuf[i].len == PBUF_FREE )       // if slot is free
            {
                txPbuf[i].len = PBUF_MARKED;        // mark as in use
                p = &(txPbuf[i]);                   // get pbuf pointer for return
                break;                              // exit loop
            }
        }
        break;

    case RX:
        for (i = 0; i < RX_PACKET_BUFS; i++)
        {
            if ( rxPbuf[i].len == PBUF_FREE )
            {
                rxPbuf[i].len = PBUF_MARKED;
                p = &(rxPbuf[i]);
                break;
            }
        }
        break;

    default:;
    }

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
 * BEtoLE()
 *
 *  big-endian to little-endian.
 *
 *  param:  uint16 in big-endian
 *  return: uint16 in little-endian
 *
 */
uint16_t BEtoLE(uint16_t w)
{
    uint16_t    temp;

    temp  = (uint16_t)(w >> 8) & 0x00ff;
    temp |= (uint16_t)(w << 8) & 0xff00;

    return temp;
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
 *  received packets will be forwarded to this function, which
 *  will replace MAC addresses with self addressed MAC and the
 *  be transmitted with a call to low_level_output()
 *
 *  param:  pbuf pointer to received data and the network interface structure
 *  return: ERR_OK on success or other status on failure.
 *
 */
ip4_err_t outputStub(struct net_interface_t* const netif, struct pbuf_t* const p)
{
    ip4_err_t                result;
    struct ethernet_frame_t *frame;
    uint8_t                 *payload;

    frame = (struct ethernet_frame_t*) p->pbuf; // some careful recasting...
    frame->dest[0] = 0x00;                      // so that the junk frame get's routed back to me
    frame->dest[1] = 0x0c;
    frame->dest[2] = 0x41;
    frame->dest[3] = 0x57;
    frame->dest[4] = 0x70;
    frame->dest[5] = 0x00;
    frame->src[0]  = 0x00;
    frame->src[1]  = 0x0c;
    frame->src[2]  = 0x41;
    frame->src[3]  = 0x57;
    frame->src[4]  = 0x70;
    frame->src[5]  = 0x00;
    frame->type = 0x0008;

    p->len = 64;

    /* this is the point where padding would be added to the frame
     * and/or CRC would be calculated and appended
     * my implementation relies on ENC28J60 to do the padding and CRC
     *
     */
    result = netif->linkoutput(netif->state, p);

    return result;
}
