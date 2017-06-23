/* ***************************************************************************

  ip4error.h

  header file for the Ipv4 stack error codes

  May 2017 - Created

*************************************************************************** */

#ifndef __IP4ERROR_H__
#define __IP4ERROR_H__

#define PRNT_FUNC   {}              //printf("%s\n",__func__)

typedef enum                        // stack wide error codes
{
    ERR_OK        =  0,             // no errors, result ok
    ERR_MEM       = -1,             // out of memory error, memory resource allocation error
    ERR_DRV       = -2,             // an interface driver error occurred, like an SPI IO problem
    ERR_NETIF     = -3,             // network interface error
    ERR_ARP_FULL  = -4,             // ARP table is full, entry not added ( or one was discarded to add this one )
    ERR_ARP_NONE  = -5,             // an ARP query or update request did not find the target IP in the table
    ERR_RT_FULL   = -6,             // route table full, entry not added
    ERR_RT_RANGE  = -7,             // route table index out of range
    ERR_BAD_PROT  = -8,             // trying to send an unsupported protocol (ipv4.c)
    ERR_NO_ROUTE  = -9,             // no route to send IP packet, check routing table
    ERR_TX_COLL   = -10,            // too many transmit collisions, Tx aborter
    ERR_TX_LCOLL  = -11,            // late-collision during transmit, possible full/half-duplex mismatch
    ERR_IN_USE    = -12,            // UDP or TCP IP address and/or port are already bound to by another connection or are invalid
    ERR_NOT_BOUND = -13             // PCB is not bound
} ip4_err_t;

#endif /* __IP4ERROR_H__ */




