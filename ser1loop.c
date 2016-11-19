/*****************************************************
 * ser1loopback.c
 *
 * this is a stand alone program to test the FlashLite
 * serial 1 port.
 * the program will initialize the serial 1 port to
 * 9600 BAUD, 1 start bit, no Parity and will echo
 * any character sent to it.
 *
 *****************************************************/

#include    <stdlib.h>
#include    <stdio.h>
#include    <string.h>
#include    <assert.h>
#include    <dos.h>
#include    "v25.h"

/* -----------------------------------------
   global definitions
----------------------------------------- */
#define     SER1MODE    0xc9            // 8 bit, 1 start, 1 stop, no parity
#define     SER1CTRL    0x03            // set to Fclk/16 scaler
#define     SER1BAUD    65              // BAUD rate divisor

#define     LOOPCNT     10              // number of characters to echo before exiting
#define     COMMTOUT    10              // 5 second time out on comm wait

/* -----------------------------------------
   startup code
----------------------------------------- */

int main(int argc, char* argv[])
{
    //struct SFR _far*    pSfr;           // pointer to NEC v25 IO register set
    struct SFR*		    pSfr;           // pointer to NEC v25 IO register set
    short int           bInByte;        // input byte to echo
    struct dostime_t    sysTime;        // hold system time for timeout calculations
    int                 nTimeofDay = 0; // time of day in seconds
    int                 nTimeOut;
    int                 i;

    // parse command line parameters:
    // ser1loop [-v]
    //  '-v' print build and version information
    for ( i = 1; i < argc; i++ )
    {
        if ( strcmp(argv[i], "-v") == 0 )
        {
            printf("ser1loop.exe %s %s\n", __DATE__, __TIME__);
            return 0;
        }
        else
        {
            printf("main(), bad command line option '%s'\n", argv[i]);
            return 1;
        }
    }

    // setup serial interface 1
    //  config: 9600BAUD, 8 bit, 1 start, 1 stop, no parity, no flow control
    pSfr = MK_FP(0xf000, 0xff00);       // define SFR
    pSfr->scm1 = SER1MODE;              // serial mode register
    pSfr->scc1 = SER1CTRL;              // serial control register
    pSfr->brg1 = SER1BAUD;              // serial baud rate register
    printf("configured SER1 port\n");

    // loop back up to LOOPCNT characters then exit
    for ( i = 0; i < LOOPCNT; i++ )
    {
        _dos_gettime(&sysTime);                      // setup time stamp for timeout measurmanets
        nTimeofDay = sysTime.second + sysTime.minute * 60 + sysTime.hour * 360;
        nTimeOut = nTimeofDay + COMMTOUT;
        while ( (pSfr->scs1 & 0x10) == 0 )       // wait for character to be received
        {
            _dos_gettime(&sysTime);
            nTimeofDay = sysTime.second + sysTime.minute * 60 + sysTime.hour * 360;
            if ( nTimeofDay > nTimeOut )
            {
                printf("time-out waiting for serial input\n");
                goto TIMEOUT;
            }
        }

        bInByte = pSfr->rxb1;                   // read input character
        printf("received byte 0x%x (count %d)\n", bInByte, i);

        _dos_gettime(&sysTime);                      // setup time stamp for timeout measurmanets
        nTimeofDay = sysTime.second + sysTime.minute * 60 + sysTime.hour * 360;
        nTimeOut = nTimeofDay + COMMTOUT;
        while ( (pSfr->scs1 & 0x20) == 0 )      // wait for TX completion
        {
            _dos_gettime(&sysTime);
            nTimeofDay = sysTime.second + sysTime.minute * 60 + sysTime.hour * 360;
            if ( nTimeofDay > nTimeOut )
            {
                printf("time-out waiting for serial output\n");
                goto TIMEOUT;
            }
        }


        pSfr->txb1 = bInByte;                   // send it back

    }

TIMEOUT:

    printf("exiting\n");

    // exit w/ status
    return 0;
}
