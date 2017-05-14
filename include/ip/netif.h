/* ***************************************************************************

  netif.h

  header file for the network interface types

  May 2017 - Created

*************************************************************************** */

#ifndef __NETIF_H__
#define __NETIF_H__

#include    "ip/error.h"
#include    "ip/options.h"
#include    "ip/enc28j60.h"
#include    "ip/types.h"

/* -----------------------------------------
   network interface functions
----------------------------------------- */
ip4_err_t   interface_init(struct net_interface_t* const);      // interface initialization ( also calls enc28j60Init() )
void        interface_input(struct net_interface_t* const);     // poll for input packets and forward up the stack for processing

#endif /* __NETIF_H__ */
