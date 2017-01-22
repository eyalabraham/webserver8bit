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
#include    <conio.h>
#include    <i86.h>
#include    <dos.h>
#include    "v25.h"

/* -----------------------------------------
   definitions and types
----------------------------------------- */

/* -----------------------------------------------------------------------
   PPI 8255 Port B

  +-----+-----+-----+-----+-----+-----+-----+-----+
  | PB7 | PB6 | PB5 | PB4 | PB3 | PB2 | PB1 | PB0 |
  +-----+-----+-----+-----+-----+-----+-----+-----+
     |     |     |     |     |     |     |     |
     |     |     |     |     |     |     |     |
     |     |     |     |     |     |     |     +--- [o]  RST^ (AVR reset line)
     |     |     |     |     |     |     +--------- [o]
     |     |     |     |     |     +--------------- [o]
     |     |     |     |     +--------------------- [o]
     |     |     |     +--------------------------- [o]
     |     |     +--------------------------------- [o]
     |     +--------------------------------------- [o]
     +--------------------------------------------- [o]

   PPI 8255 Port C

  +-----+-----+-----+-----+-----+-----+-----+-----+
  | PC7 | PC6 | PC5 | PC4 | PC3 | PC2 | PC1 | PC0 |
  +-----+-----+-----+-----+-----+-----+-----+-----+
     |     |     |     |     |     |     |     |
     |     |     |     |     |     |     |     |
     |     |     |     |     |     |     |     +--- [o]  \
     |     |     |     |     |     |     +--------- [o]   | device select line
     |     |     |     |     |     +--------------- [o]  /
     |     |     |     |     +--------------------- [o]  INTR to CPU
     |     |     |     +--------------------------- [i]  STB^ from AVR / INTE2 enable interrupt from AVR strobe
     |     |     +--------------------------------- [o]  IBF to AVR
     |     +--------------------------------------- [i]  ACK^ from AVR / INTE1 enable interrupt from AVR Ack
     +--------------------------------------------- [o]  OBF^ to AVR

----------------------------------------------------------------------- */

// 8255 IO ports A, B, C and Control initialization
#define     PPIPA           0x00                // IO port addresses
#define     PPIPB           0x01
#define     PPIPC           0x02
#define     PPICNT          0x03

#define     PPI_MODE        0xc0                // PA in mode-2, PB in mode 0 output, interrupt generation disabled
#define     PPIPC_INIT      0x57                // PC0..PC2 initialization, INTE1=INTE2='1'
#define     PPIPB_INIT      0x01                // reset line initialization

#define     DEV_BIT_MASK    0x07                // mask for device select bits

#define     INTE1           0x40                // status and mask bits for 8255 Mode-2 interrupts
#define     INTE2           0x10
#define     INTEA           0x08
#define     OBF             0x80                // Mode-2 handshake status
#define     IBF             0x20
#define     AVR_RST         0x01                // AVR rest pin on PPI-PB

// SPI driver function return status codes
// functions will return 'int'
// positive number indicates success status
#define     SPI_OK                   0          // no error
#define     SPI_BUSY                -1          // SPI interface already in use, try again
#define     SPI_ERR_TX_TIMEOUT      -2          // transmit time out, ready not received from interface
#define     SPI_ERR_RX_TIMEOUT      -3          // receive time out, ready not received from interface
#define     SPI_IO_DIR_ERR          -4          // wrong IO direction, read in a write function or write in a read function
#define		SPI_RD_ERR				-5			// read initiated before input buffer cleared
#define     SPI_TEST_PARAM          -6

typedef int HANDLE;                             // SPI session handle

typedef enum                                    // valid devices supported
{
    LCD_CMD,                                    // ST7735R LCD controller commands
    LCD_DATA,                                   // ST7735R LCD controller data
    ETHERNET_RD,                                // ENC28J60 Ethernet read
    ETHERNET_WR,                                // ENC28J60 Ethernet write
    SD_CARD_RD,                                 // SD card read
    SD_CARD_WR,                                 // SD card write
    AVR_CMD,                                    // special command to AVR
    NONE                                        // no valid device selected
} spiDevice_t;

/* -----------------------------------------
   globals
----------------------------------------- */
spiDevice_t     activeDevice = NONE;            // active device flag
struct SFR*     pSfr;                           // pointer to NEC v25 IO register set

/* -----------------------------------------
   functional prototypes
----------------------------------------- */
void spiIoInit(void);                           // call once to initialize IO
int  spiReadByte(spiDevice_t, unsigned char*);  // read a byte from a device
int  spiWriteByte(spiDevice_t, unsigned char);  // write a byte to a device
                                                // read/write a block of data into/out-of buffer location of certain size, at completion call a callback function
int  spiReadBlock(spiDevice_t, unsigned char*, unsigned int, void(*)(void));
int  spiWriteBlock(spiDevice_t, unsigned char*, unsigned int, void(*)(void));

/* -----------------------------------------
   startup code
----------------------------------------- */

int main(int argc, char* argv[])
{
    int             i, err, isRead = 0;
    unsigned char   byte;

    if ( argc == 1 )
    {
        printf("select -r or -w\n");
        return SPI_TEST_PARAM;
    }

    for ( i = 1; i < argc; i++ )
    {
        if ( strcmp(argv[i], "-r") == 0 )
        {
            isRead = 1;
        }
        else if ( strcmp(argv[i], "-w") == 0 )
        {
            isRead = 0;
        }
        else
        {
            printf("select -r or -w\n");
            return SPI_TEST_PARAM;
        }
    }

    // build date
    printf("Build: ws.exe %s %s\n", __DATE__, __TIME__);

    spiIoInit();

    for ( i = 0; i < 32000; i++)
    {
        if ( isRead )
            err = spiReadByte(ETHERNET_RD, &byte);
        else
            err = spiWriteByte(ETHERNET_WR, 0x55);

        if ( err != SPI_OK )
        {
            printf("SPI error %d (count %d)\n", err, i);
            break;
        }
    }

    return err;
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
#define     LOOP    1000

void spiIoInit(void)
{
    int     i;

    pSfr = MK_FP(0xf000, 0xff00);               // initialize SFR pointer

    outp(PPICNT, PPI_MODE);                     // initialize 8255 Mode-2, PB output, PC0..2 output
    outp(PPIPB,  PPIPB_INIT);                   // initialize PB and PC
    outp(PPIPC,  PPIPC_INIT);

    for ( i = 0; i < LOOP; i++);
    outp(PPIPB,(inp(PPIPB) & ~AVR_RST));        // reset AVR
    for ( i = 0; i < LOOP; i++);
    outp(PPIPB,(inp(PPIPB) | AVR_RST));
    for ( i = 0; i < LOOP; i++);

    inp(PPIPA);                                 // dummy read to clear 8255 IBF, the above reset to AVR causes a fake strobe on STB^

    printf("spiIoInit(): IBF(%d)\n", inp(PPIPC) & IBF);
}

/*------------------------------------------------
 * spiDevSelect()
 *
 *  set device select lines on 8255 PPIPC
 *
 */
void spiDevSelect(spiDevice_t device)
{
    unsigned char   select;

    select = (inp(PPIPC) & ~DEV_BIT_MASK) | (device & DEV_BIT_MASK);
    outp(PPIPC, select);
}

/*------------------------------------------------
 * spiReadByte()
 *
 *  read a byte from a device
 *  the read action is triggered by a device selection
 *  function blocks until read byte is ready
 *
 */
#define     COUNT_OUT   60000

int spiReadByte(spiDevice_t device, unsigned char* data)
{
    unsigned int    i = 0;
    int             nReturn;

    if ( !(device == ETHERNET_RD || device == SD_CARD_RD) )
        return SPI_IO_DIR_ERR;

	if ( inp(PPIPC) & IBF )						// exit with error if IBF is 'hi'
		return SPI_RD_ERR;						// something went wrong and input buffer has unread data

    if ( activeDevice == NONE )
    {
        activeDevice = device;                  // flag the session as active

        spiDevSelect(device);                   // select a device, which causes the AVR to initiate a read to the device

        while ( !(inp(PPIPC) & IBF) )           // wait for IBF to go high after AVR sends data and STB^
        {
            i++;                                // crude time-out
            if ( i == COUNT_OUT )
            {
                activeDevice = NONE;
                spiDevSelect(NONE);
                return SPI_ERR_RX_TIMEOUT;
            }
        }

        spiDevSelect(NONE);                     // deselect the device so that no more reads are initiate on SPI bus (see AVR code in par2spi.c)

        *data = (unsigned char) inp(PPIPA);     // read the data from the 8255 buffer

        //printf("spiReadByte(): IBF(%d)\n", inp(PPIPC) & IBF);

        nReturn = SPI_OK;
        activeDevice = NONE;
    }
    else
    {
        nReturn = SPI_BUSY;
    }

    return nReturn;
}

/*------------------------------------------------
 * spiWriteByte()
 *
 *  write a byte to a device
 *  block until AVR acknowledges write to it
 *
 */
int spiWriteByte(spiDevice_t device, unsigned char data)
{
    unsigned char   select;
    int             nReturn;

    if ( device == ETHERNET_RD || device == SD_CARD_RD )
        return SPI_IO_DIR_ERR;

    if ( activeDevice == NONE )
    {
        activeDevice = device;                  // flag the session as active

        spiDevSelect(device);                   // select a device

        while ( !(inp(PPIPC) & OBF) ) {}        // if OBF^ is '1' then we can write out the data byte
        outp(PPIPA, data);                      // output data byte

        spiDevSelect(NONE)        ;             // deselect the device

        nReturn = SPI_OK;
        activeDevice = NONE;
    }
    else
    {
        nReturn = SPI_BUSY;
    }

    return nReturn;
}

/*------------------------------------------------
 * spiReadBlock()
 *
 *  read a block of data into buffer location of certain size,
 *  at completion call a callback function
 *
 */
int spiReadBlock(spiDevice_t device, unsigned char* inputBuffer, unsigned int count, void(*spiCallBack)(void))
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
int spiWriteBlock(spiDevice_t device, unsigned char* outputBuffer, unsigned int count, void(*spiCallBack)(void))
{
    return SPI_OK;
}
