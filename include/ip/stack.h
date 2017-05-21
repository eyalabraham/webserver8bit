/* ***************************************************************************

  ip4stack.h

  header file for the Ipv4 stack

  May 2017 - Created

*************************************************************************** */

#ifndef __IP4STACK_H__
#define __IP4STACK_H__

#include    <sys/types.h>

#include    "ip/options.h"
#include    "ip/error.h"
#include    "ip/types.h"

/* -----------------------------------------
   IPv4 interface and utility functions
----------------------------------------- */
void          stack_init(void);                             // initialize the IP stack
struct net_interface_t* const stack_get_ethif(uint8_t);     // get pointer to an interface on the stack
uint32_t      stack_time(void);                             // return stack time in mSec
void          stack_timers(void);                           // handle stack timers and timeouts for all network interfaces

struct pbuf_t* const pbuf_allocate(pbuf_type_t);            // allocate a transmit or receive buffer
void          pbuf_free(struct pbuf_t* const);              // free a buffer allocation

void          stack_sig(stack_sig_t);                       // signal stack events
uint16_t      stack_byteswap(uint16_t);                     // big-endian to little-endian 16bit bytes swap
uint16_t      stack_checksum(const void*, int);             // checksum calculation
void          inputStub(struct pbuf_t* const,               // input stub function
                        struct net_interface_t* const);
ip4_err_t     outputStub(struct net_interface_t* const,     // output stub function
                         struct pbuf_t* const);

#endif /* __IP4STACK_H__ */
