/* ***************************************************************************

  ip4error.h

  header file for the Ipv4 stack error codes

  May 2017 - Created

*************************************************************************** */

#ifndef __IP4ERROR_H__
#define __IP4ERROR_H__

typedef enum                    // stack wide error codes
{
    ERR_OK       =  0,          // no errors, result ok
    ERR_MEM      = -1,          // out of memory error, memory resource allocation error
    ERR_DRV      = -2,          // an interface driver error occurred, like an SPI IO problem
    ERR_NETIF    = -3,          // network interface error
    ERR_ARP_FULL = -4,          // ARP table is full, entry not added ( or one was discarded to add this one )
    ERR_ARP_NONE = -5           // an ARP query or update request did not find the target IP in the table
} ip4_err_t;

#endif /* __IP4ERROR_H__ */




