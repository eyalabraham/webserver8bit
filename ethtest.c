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

#include    "ip/error.h"
#include    "ip/stack.h"
#include    "ip/types.h"
#include    "ppispi.h"

#include    "ip/netif.h"

/* -----------------------------------------
   definitions and globals
----------------------------------------- */

/*
void showDhcpParem(struct dhcp  *dhcp)
{
    char    address[20];                        // xxx.xxx.xxx.xxx

    char *ip4addr_ntoa_r(const ip4_addr_t *addr, char *buf, int buflen);
    printf("IP          %s\nSubnet mask %s\nGateway     %s\n",
            ip4addr_ntoa_r(&(dhcp->offered_ip_addr), address, 20),
            ip4addr_ntoa_r(&(dhcp->offered_sn_mask), address, 20),
            ip4addr_ntoa_r(&(dhcp->offered_gw_addr), address, 20));
}
*/

/*------------------------------------------------
 * main()
 *
 *
 */
void main(int argc, char* argv[])
{
    int                     linkState, i, j = 0;
    struct net_interface_t* netif;

    printf("Build: ethtest.exe %s %s\n", __DATE__, __TIME__);

    spiIoInit();                                        // initialize SPI interface
    stack_init();                                       // initialize IP stack
    netif = stack_get_ethif(0);                         // get pointer to interface 0
    assert(netif);

    assert(interface_init(netif) == ERR_OK);            // initialize link HW
    interface_set_addr(netif, IP4_ADDR(192,168,1,19),   // setup static IP addressing
                              IP4_ADDR(255,255,255,0),
                              IP4_ADDR(192,168,1,1));

    printf("---- entering main loop ----\n");

    linkState = interface_link_state(netif);
    printf("link state = '%s'\n", linkState ? "up" : "down");

    while ( 1 )
    {
      /*
       * periodically poll link state and if a change occurred from the last
       * test propagate the notification
       *
       */
      if ( interface_link_state(netif) != linkState )
      {
          linkState = interface_link_state(netif);
          printf("link state change = '%s'\n", linkState ? "up" : "down");
          switch ( linkState )
          {
          case  0:
              //netif_set_link_down(&netif);
              break;

          case  1:
              //netif_set_link_up(&netif);
              break;
          }
      }

      /* periodically poll for received frames,
       * drop or feed them up the stack for processing
       *
       */
      interface_input(netif);

      /* cyclic timer update and check
       *
       */
      stack_timers();

      /* @@ application goes here?
       *
       */
    }
}
