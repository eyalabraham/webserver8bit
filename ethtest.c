/*
 *  ethtest.c
 *
 *      test program for V25 FlashLite board to test ENC28J60 ethernet
 *
 */

#include    <stdlib.h>
#include    <stdio.h>
#include    <string.h>
#include    <assert.h>
#include    <conio.h>
#include    <dos.h>

#include    "netif/enc28j60.h"
#include    "lwipopts.h"
#include    "lwip/init.h"
#include    "lwip/dhcp.h"
#include    "lwip/timeouts.h"

/* -----------------------------------------
   definitions and globals
----------------------------------------- */


/*------------------------------------------------
 * main()
 *
 *  based on: http://www.nongnu.org/lwip/2_0_x/index.html
 *  run lwIP in a mainloop with NO_SYS to 1
 *  code example from: http://www.nongnu.org/lwip/2_0_x/group__lwip__nosys.html
 *
 *
 */
void main(int argc, char* argv[])
{
    struct netif netif;
    struct dhcp  dhcp;
/*
    struct ip_addr ipaddr, netmask, gw;

    IP4_ADDR(&gw, 192,168,0,1);                 // when static assignments, put these in netif_add()
    IP4_ADDR(&ipaddr, 192,168,0,19);
    IP4_ADDR(&netmask, 255,255,255,0);
*/

    lwip_init();

    netif_add(&netif, IP4_ADDR_ANY, IP4_ADDR_ANY, IP4_ADDR_ANY, NULL, enc28j60_init, netif_input);

#if LWIP_NETIF_STATUS_CALLBACK
    netif_set_status_callback(&netif, netif_status_callback);
#endif
    netif_set_default(&netif);
    //netif_set_up(&netif);                       // only when using static address

    /* Start DHCP and HTTPD */
    dhcp_set_struct(&netif, &dhcp);               // DHCP usage: http://lwip.wikia.com/wiki/DHCP
    if ( dhcp_start(&netif) != ERR_OK )
    {
        printf("DHCP could not assign IP address\n");
        return;
    }

    while ( 1 )
    {
      /* Check link state, e.g. via MDIO communication with PHY */
      if ( ethLinkUp() )
        netif_set_link_up(&netif);
      else
        netif_set_link_down(&netif);

      /* Check for received frames, feed them to lwIP */
      enc28j60_input(&netif);

      /* Cyclic lwIP timers check */
      sys_check_timeouts();

      /* @@ application goes here? */
    }
}
