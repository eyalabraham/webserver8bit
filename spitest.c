/*
 *  spitest.c
 *
 *      test program for V25 FlashLite board to test SPI interface through AVR
 *
 */

#include    <stdlib.h>
#include    <stdio.h>
#include    <string.h>
#include    <assert.h>
#include    <dos.h>
#include    "v25.h"

/* -----------------------------------------
   definitions and types
----------------------------------------- */

// SPI driver function return status codes
// functions will return 'int'
// positive number indicates success status
#define     SPI_OK                   0          // no error
#define     SPI_BUSY                -1          // SPI interface already in use, try again
#define     SPI_ERR_INVALID_HANDLE  -2          // invalid handle supplied for the session, or session not open
#define     SPI_ERR_TX_TIMEOUT      -3          // transmit time out, ready not received from interface
#define     SPI_ERR_RX_TIMEOUT      -4          // receive time out, ready not received from interface

typedef int HANDLE;                             // SPI session handle

typedef enum                                    // "legal" devices supported
{
    ETHRNET,                                    // ENC28J60 Ethernet
    LCD_DATA,                                   // ST7735R LCD controller data
    LCD_CMD                                     // ST7735R LCD controller commands
} spiDevice_t;

/* -----------------------------------------
   globals
----------------------------------------- */
HANDLE      hSpiSession = 0;

/* -----------------------------------------
   functional prototypes
----------------------------------------- */
void spiIoInit(void);                           // call once to initialize IO
int  spiStartSession(spiDevice_t);              // start an SPI exchange session, must be matched with an spiEndSession() call, returns status or valid handle
                                                // SPI CS (Slave Select) will stay asserted until spiEndSession() is called
int  spiEndSession(HANDLE);                     // end an open session
int  spiReadByte(HANDLE, unsigned char*);       // read a byte from an open session
int  spiWriteByte(HANDLE, unsigned char);       // write a byte to an open session
                                                // read/write a block of data into/out-of buffer location of certain size, at completion call a callback function
int  spiReadBlock(HANDLE, unsigned char*, unsigned int, void(*)(void));
int  spiWriteBlock(HANDLE, unsigned char*, unsigned int, void(*)(void));

/* -----------------------------------------
   startup code
----------------------------------------- */

int main(int argc, char* argv[])
{
    struct SFR*     pSfr;           // pointer to NEC v25 IO register set

    return 0;
}

/* -----------------------------------------
   driver functions
----------------------------------------- */

/*------------------------------------------------
 * spiIoInit()
 *
 *  call once to initialize IO
 *
 */
void spiIoInit(void)
{
}

/*------------------------------------------------
 *  spiStartSession()
 *
 *   start an SPI exchange session, must be matched with
 *   an spiEndSession() call, returns status or valid handle
 *   SPI CS (Slave Select) will stay asserted until spiEndSession() is called
 *
 */
int spiStartSession(spiDevice_t device)
{
    return SPI_OK;
}

/*------------------------------------------------
 * spiEndSession()
 *
 *  end an open session
 *
 */
int spiEndSession(HANDLE hSPI)
{
    return SPI_OK;
}

/*------------------------------------------------
 * spiReadByte()
 *
 *  read a byte from an open session
 *
 */
int spiReadByte(HANDLE hSPI, unsigned char* data)
{
    return SPI_OK;
}

/*------------------------------------------------
 * spiWriteByte()
 *
 *  write a byte to an open session
 *
 */
int spiWriteByte(HANDLE hSPI, unsigned char data)
{
    return SPI_OK;
}

/*------------------------------------------------
 * spiReadBlock()
 *
 *  read a block of data into buffer location of certain size,
 *  at completion call a callback function
 *
 */
int spiReadBlock(HANDLE hSPI, unsigned char* inputBuffer, unsigned int count, void(*spiCallBack)(void))
{
    return SPI_OK;
}

/*------------------------------------------------
 * spiReadBlock()
 *
 *  write a block of data out of buffer location of certain size,
 *  at completion call a callback function
 *
 */
int spiWriteBlock(HANDLE hSPI, unsigned char* outputBuffer, unsigned int count, void(*spiCallBack)(void))
{
    return SPI_OK;
}
