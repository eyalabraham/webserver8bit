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
#define     PPIPC_INIT      0x07                // PC0..PC2 initialization, INTE1 and INTE2 set with 8255 bit-clear command
#define     PPIPB_INIT      0x01                // reset line initialization

#define     DEV_BIT_MASK    0x07                // mask for device select bits

#define     OBF             0x80                // Mode-2 handshake status
#define     IBF             0x20
#define     AVR_RST         0x01                // AVR rest pin on PPI-PB

#define     INTE1_SET       outp(PPICNT,0x0d)   // INTE flip-flop
#define     INTE1_CLEAR     outp(PPICNT,0x0c)
#define     INTE2_SET       outp(PPICNT,0x09)
#define     INTE2_CLEAR     outp(PPICNT,0x08)

// NEC v25 interrupt and IO definitions for
// DMA channel-0
#define     DMAC0_VEC       20                  // DMA channel 0 interrupt vector

#define     PORT2_MODE      0xff                // PM2: all bits/pins that are in port mode are inputs
#define     PORT2_CTRL      0x07                // PMC2: P20..2 act as DMARQ0 input, DMAAK0 input, TC0 output

#define     DMA0_MODE_INIT  DMA0_MEM_IO         // DMAM0 initialization, disabled, byte transfer, io to mem
#define     DMA0_CTRL_INIT  0x00                // DMAC0 no address update
#define     DMA0_INT_INIT   0x40                // DIC0 DMA0 interrupt control, masked, vector interrupt

#define     DMA0_ENABLE     0x08                // DMAM0: DMA0 enable bit (EDMA)
#define     DMA0_IO_MEM     0xa0                // DMAM0: single transfer IO to memory
#define     DMA0_MEM_IO     0xc0                // DMAM0: single transfer memory to IO
#define     DMA0_INC_SRC    0x01                // increment source address
#define     DMA0_INC_DEST   0x10                // increment destination address
#define     DMA0_INT_MASK   0x40                // DIC0: interrupt mask bit

/* -----------------------------------------
   globals
----------------------------------------- */
static spiDevice_t      activeDevice = NONE;    // active device flag
static struct SFR*      pSfr;                   // pointer to NEC v25 IO register set
static void             (*callBack)(void);      // save call back function pointer
static int              nSpiReadBlock;          // read (=1) or write (=0) DMA cycle flag

/* -----------------------------------------
   driver functions
----------------------------------------- */

/*------------------------------------------------
 * spiDevSelect()
 *
 *  set device select lines on 8255 PPIPC
 *
 */
static void spiDevSelect(spiDevice_t device)
{
    unsigned char   select;

    select = (inp(PPIPC) & ~DEV_BIT_MASK) | (device & DEV_BIT_MASK);
    outp(PPIPC, select);
    activeDevice = device;
}

/*------------------------------------------------
 * spiIoComplete()
 *
 *  SPI IO completion interrupt handler
 *  this interrupt handler will be invoked at the end of
 *  the DMA service cycle when TC reaches '0'
 *
 *  Note for SPI WR:
 *   when the handler is invoked, there is still one byte
 *   pending transmission still help in the 8255.
 *   the handler will wait for OBF^ to be '1' (ACK^ by the AVR)
 *   before removing device select on PPIPC0..2
 *
 *  Note for SPI RD:
 *
 */
static void __interrupt spiIoComplete(void)
{
    INTE1_CLEAR;                                // disable interrupt generation on 8255 for strobed output and input
    INTE2_CLEAR;

    pSfr->dmam0 = DMA0_MEM_IO;                  // *** no need, EDMA will be cleared with TC, disable DMA on channel 0
    pSfr->dic0 = DMA0_INT_INIT;                 // mask DMA0 TC interrupt

    if ( nSpiReadBlock )
    {                                           // when completing a read operation with TC = 0
        while ( !(inp(PPIPC) & IBF) ) {}       // last extra byte is being read by the AVR, wait for Rx to complete
        spiDevSelect(NONE);                     // disable device selects
        inp(PPIPA);                             // "release" the AVR by dummy reading the extra byte
    }
    else
    {                                           // when completing a write operation with TC = 0
        while ( !(inp(PPIPC) & OBF) ) {}        // if OBF^ is '1' AVR ACK'ed the last byte and is transmitting it,
        spiDevSelect(NONE);                     // it is now safe to deselect the device
                                                // device deselect must happen ~1.5 SPI-byte-transmit-time after DMA TC interrupt or earlier
    }

    if ( callBack )                             // invoke the callback function if one is defined
        callBack();

    // ISR epilogue
    __asm { db  0x0f
            db  0x92
          };
}

/*------------------------------------------------
 * spiIoInit()
 *
 *  call once to initialize IO
 *
 */
#define     LOOP    53000                       // this will be approx 200mSec

void spiIoInit(void)
{
    unsigned int    i;
    unsigned int*   wpVector;

    pSfr = MK_FP(0xf000, 0xff00);               // initialize SFR pointer

    outp(PPICNT, PPI_MODE);                     // initialize 8255 Mode-2, PB output, PC0..2 output
    outp(PPIPB,  PPIPB_INIT);                   // initialize PB and PC
    outp(PPIPC,  PPIPC_INIT);

    INTE1_CLEAR;                                // disable interrupt generation on 8255 for strobed output and input
    INTE2_CLEAR;

    for ( i = 0; i < (LOOP/2); i++);
    outp(PPIPB,(inp(PPIPB) & ~AVR_RST));        // reset AVR
    for ( i = 0; i < LOOP; i++);                // wait approx 200mSec which is also needed by LCD to reset
    outp(PPIPB,(inp(PPIPB) | AVR_RST));
    for ( i = 0; i < (LOOP/2); i++);

    inp(PPIPA);                                 // dummy read to clear 8255 IBF, the above reset to AVR causes a fake strobe on STB^

    callBack = 0;                               // initialize call back pointer

    _disable();                                 // disable interrupts

    wpVector      = MK_FP(0, (DMAC0_VEC * 4));  // hook DMA completion interrupt service routine
    *wpVector++   = FP_OFF(spiIoComplete);
    *wpVector     = FP_SEG(spiIoComplete);

    pSfr->sar0  = 0;                            // initialize DMA channel 0
    pSfr->sar0h = 0;
    pSfr->dar0  = 0;
    pSfr->dar0h = 0;
    pSfr->tc0   = 0xffff;

    pSfr->portmc2 = PORT2_CTRL;                 // Port 2 control function mode for DMA channel 0 pins
    pSfr->portm2  = PORT2_MODE;

    pSfr->dmam0   = DMA0_MODE_INIT;             // DMA channel 0 setup
    pSfr->dmac0   = DMA0_CTRL_INIT;
    pSfr->dic0    = DMA0_INT_INIT;

    _enable();                                  // enable interrupts
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
        spiDevSelect(device);                   // select a device, which causes the AVR to initiate a read to the device

        while ( !(inp(PPIPC) & IBF) )           // wait for IBF to go high after AVR sends data and STB^
        {
            i++;                                // crude time-out
            if ( i == COUNT_OUT )
            {
                spiDevSelect(NONE);
                nReturn = SPI_ERR_RX_TIMEOUT;
                goto SPIRD_TOUT;
            }
        }

        spiDevSelect(NONE);                     // deselect the device so that no more reads are initiate on SPI bus (see AVR code in par2spi.c)

        *data = (unsigned char) inp(PPIPA);     // read the data from the 8255 buffer

        nReturn = SPI_OK;
    }
    else
    {
        nReturn = SPI_BUSY;
    }

SPIRD_TOUT:
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
        spiDevSelect(device);                   // select a device

        while ( !(inp(PPIPC) & OBF) ) {}        // if OBF^ is '1' then we can write out the data byte
        outp(PPIPA, data);                      // output data byte

        spiDevSelect(NONE);                     // deselect the device

        nReturn = SPI_OK;
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
int spiReadBlock(spiDevice_t device, unsigned char* inputBuffer, unsigned int count, void (*spiCallBack)(void))
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
int spiWriteBlock(spiDevice_t device, unsigned char* outputBuffer, unsigned int count, void (*spiCallBack)(void))
{
    return SPI_OK;
}
