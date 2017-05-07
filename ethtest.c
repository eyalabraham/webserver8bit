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

#include    "ppispi.h"

#include    "netif/enc28j60.h"
#include    "lwipopts.h"
#include    "lwip/init.h"
#include    "lwip/dhcp.h"
#include    "lwip/timeouts.h"
#include    "lwip/sys.h"

/* -----------------------------------------
   definitions and globals
----------------------------------------- */

void showDhcpParem(struct dhcp  *dhcp)
{
    char    address[20];                        // xxx.xxx.xxx.xxx

    char *ip4addr_ntoa_r(const ip4_addr_t *addr, char *buf, int buflen);
    printf("IP          %s\nSubnet mask %s\nGateway     %s\n",
            ip4addr_ntoa_r(&(dhcp->offered_ip_addr), address, 20),
            ip4addr_ntoa_r(&(dhcp->offered_sn_mask), address, 20),
            ip4addr_ntoa_r(&(dhcp->offered_gw_addr), address, 20));
}

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
    struct netif    netif;
    struct dhcp     dhcp;
    int             linkState;
/*
    struct ip4_addr ipaddr, netmask, gw;

    IP4_ADDR(&gw, 192,168,0,1);                 // when static assignments, put these in netif_add()
    IP4_ADDR(&ipaddr, 192,168,0,19);
    IP4_ADDR(&netmask, 255,255,255,0);
*/

    // build date
    //
    printf("Build: ethtest.exe %s %s\n", __DATE__, __TIME__);

    spiIoInit();                                    // initialize SPI interface

    lwip_init();                                    // intialize LwIP stack

    netif_add(&netif, IP4_ADDR_ANY, IP4_ADDR_ANY, IP4_ADDR_ANY, NULL, enc28j60_init, netif_input);

#if LWIP_NETIF_STATUS_CALLBACK
    netif_set_status_callback(&netif, netif_status_callback);
#endif
    netif_set_default(&netif);

    /* Start DHCP and HTTPD */
    dhcp_set_struct(&netif, &dhcp);                 // DHCP usage: http://lwip.wikia.com/wiki/DHCP
    if ( dhcp_start(&netif) != ERR_OK )
    {
        printf("DHCP could not assign IP address\n");
        return;
    }

    showDhcpParem(&dhcp);

    printf("---- entering mainloop ----\n");

    linkState = ethLinkUp();
    printf("link state = '%s'\n", linkState ? "up" : "down");

    while ( 1 )
    {
      /* Check link state */
      if ( ethLinkUp() != linkState )
      {
          linkState = ethLinkUp();
          printf("link state change = '%s'\n", linkState ? "up" : "down");
          switch ( linkState )
          {
          case  0:
              netif_set_link_down(&netif);
              break;

          case  1:
              netif_set_link_up(&netif);
              break;
          }
      }

      /* Check for received frames, feed them to lwIP */
      enc28j60_input(&netif);

      /* Cyclic lwIP timers check */
      sys_check_timeouts();

      printf("time %lu\n", sys_now());

      /* @@ application goes here? */
    }
}
