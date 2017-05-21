/* ***************************************************************************

  ipv4.h

  Header file for IPv4 Network layer services and utilities

  May 2017 - Created

*************************************************************************** */

#ifndef __IPV4_H__
#define __IPV4_H__

#include    "ip/options.h"
#include    "ip/error.h"
#include    "ip/types.h"

/* -----------------------------------------
   IPv4 network layer functions
----------------------------------------- */
void        ip4_input(struct pbuf_t* const, struct net_interface_t* const);
ip4_err_t   ip4_output(struct pbuf_t* const);

#endif /* __IPV4_H__ */
