/* ***************************************************************************

  tcp.h

  Header file for TCP transport protocol
  This module implements only the functionality needed for a TCP transport

  June 2017 - Created

*************************************************************************** */

#ifndef __TCP_H__
#define __TCP_H__

#include    "ip/options.h"
#include    "ip/error.h"
#include    "ip/types.h"

/* -----------------------------------------
   TCP protocol functions
----------------------------------------- */
/* initialize open and close TCP connections
 */
void              tcp_init(void);                       // Initialize the TCP protocol
pcbid_t           tcp_new(void);                        // create a TCP connection and return TCP PCB which was created. NULL if the PCB data structure could not be allocated.
ip4_err_t         tcp_notify(pcbid_t,                   // notify a bound connection of changes, such as remote disconnection
                             tcp_notify_callback);
ip4_err_t         tcp_bind(pcbid_t,                     // bind a PCB connection to a local IP address and a port
                           ip4_addr_t,
                           uint16_t);
ip4_err_t         tcp_close(pcbid_t);                   // close a TCP connection and clear its PCB

/* server connection
 */
ip4_err_t         tcp_listen(pcbid_t);                  // server's passive-open to TCP_MAX_ACCEPT incoming client connections
ip4_err_t         tcp_accept(pcbid_t,                   // register an accept callback that will be called when
                             tcp_accept_callback);      // a client connects to the open server PCB
/* client connection
 */
ip4_err_t         tcp_connect(pcbid_t,                  // connect a TCP client to a server's IP address and a port
                              ip4_addr_t,               // NOTE: in this implementation use tcp_bind() first
                              uint16_t);
int               tcp_is_connected(pcbid_t);            // to poll if a tcp_connect() request was successful

/* send and receive functions
 */
ip4_err_t         tcp_send(pcbid_t,                     // send a segment of data from a buffer
                           uint8_t* const,
                           uint16_t);
ip4_err_t         tcp_recv(pcbid_t,                     // registers a callback to handle received data
                           tcp_recv_callback);

/* connection utilities
 */
ip4_addr_t        tcp_remote_addr(pcbid_t);             // get remote address of a connection
uint16_t          tcp_remote_port(pcbid_t);             // get remote port of a connection

#endif /* __TCP_H__ */
