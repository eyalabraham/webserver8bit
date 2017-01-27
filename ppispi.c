/*
 *  ppispi.c
 *
 *      driver for 8255 PPI to AVR/SPI interface
 *
 */

#include    <conio.h>
#include    <i86.h>
#include    <dos.h>

#include    "ppispi.h"
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

/* -----------------------------------------
   globals
----------------------------------------- */
spiDevice_t     activeDevice = NONE;            // active device flag
struct SFR*     pSfr;                           // pointer to NEC v25 IO register set

/* -----------------------------------------
   driver functions
----------------------------------------- */

/*------------------------------------------------
 * spiIoInit()
 *
 *  call once to initialize IO
 *
 */
#define     LOOP    45000                       // this will be approx 200mSec

void spiIoInit(void)
{
    unsigned int     i;

    pSfr = MK_FP(0xf000, 0xff00);               // initialize SFR pointer

    outp(PPICNT, PPI_MODE);                     // initialize 8255 Mode-2, PB output, PC0..2 output
    outp(PPIPB,  PPIPB_INIT);                   // initialize PB and PC
    outp(PPIPC,  PPIPC_INIT);

    for ( i = 0; i < LOOP; i++);
    outp(PPIPB,(inp(PPIPB) & ~AVR_RST));        // reset AVR
    for ( i = 0; i < LOOP; i++);                // wait approx 200mSec which is also needed by LCD to reset
    outp(PPIPB,(inp(PPIPB) | AVR_RST));
    for ( i = 0; i < LOOP; i++);

    inp(PPIPA);                                 // dummy read to clear 8255 IBF, the above reset to AVR causes a fake strobe on STB^
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
 *  function blocks until read byte is ready or times-out
 *
 */
#define     COUNT_OUT   30000

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

        spiDevSelect(NONE);                     // deselect the device

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
