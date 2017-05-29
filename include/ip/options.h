/* ***************************************************************************

  options.h

  header file for IP stack components

  May 2017 - Created

*************************************************************************** */

#ifndef __OPTIONS_H__
#define __OPTIONS_H__

/*
 * platform/compile option definitions
 *
 */
#define     SYSTEM_DOS          1           // DOS or LMTE executive environment
#define     SYSTEM_LMTE         !SYSTEM_DOS

/*
 * definitions that need to be placed in the right location
 *
 */
#define     HOSTNAME_LENGTH     10
#define     HOSTNAME            "nec-v25\0"

/*
 * Physical layer setup options, ethernet HW
 *
 */
#define     ETHIF_NAME_LENGTH   5
#define     ETHIF_NAME          "eth0\0"    // interface's identifier

#define     MAC0                0x00        // my old USB wifi 'B' adapter
#define     MAC1                0x0c
#define     MAC2                0x41
#define     MAC3                0x57
#define     MAC4                0x70
#define     MAC5                0x00

#define     FULL_DUPLEX         1           // set to 0 for half-duplex setup
#define     INTERFACE_COUNT     1           // # of ethernet interfaces in the system
#define     DRV_DMA_IO          1           // set to 1 for DMA based IO

#define     MTU                 1500

/*
 * Data Link layer setup options, buffers, ARP etc
 *
 */
#define     RX_PACKET_BUFS      1           // # of input packet buffers, relying on enc28j60 8K buffering
#define     TX_PACKET_BUFS      2           // # of output packet buffers
#define     MAX_PBUFS           8           // max # of RX or TX buffers
#define     PACKET_BUF_SIZE     1536        // size of packet buffer in bytes

#define     ARP_TABLE_LENGTH    10          // number of entries in the ARP table

/*
 * Network layer setup options
 *
 */
#define     ROUTE_TABLE_LENGTH   2          // max number of entries in the routing table, must be .gte. to INTERFACE_COUNT

#endif  /* __OPTIONS_H__ */
