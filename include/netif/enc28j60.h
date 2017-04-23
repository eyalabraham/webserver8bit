/* ***************************************************************************

  enc28j60.h

  header file for Microchip EMC28J60 ethernet chip driver for lwIP

  March 2017 - Created

*************************************************************************** */

#ifndef __ENC28J60_H__
#define __ENC28J60_H__

#include "lwip/err.h"
#include "lwip/netif.h"

/* -----------------------------------------
   interface initialization parameters
----------------------------------------- */

#define     HOSTNAME        "nec-v25"

#define     MAC0            0x00                // my old USB wifi 'B' adapter
#define     MAC1            0x0c
#define     MAC2            0x41
#define     MAC3            0x57
#define     MAC4            0x70
#define     MAC5            0x00

#define     MTU             1500

/* -----------------------------------------
   interface functions
----------------------------------------- */
err_t enc28j60_init(struct netif *netif);
void  enc28j60_input(struct netif *netif);
int   ethLinkUp(void);

#endif  /* __ENC28J60_H__ */
