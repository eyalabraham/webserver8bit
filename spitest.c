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
//#include    <i86.h>
#include    <dos.h>

#include    "ppispi.h"                          // 8255 PPI to AVR/SPI driver
#include    "st7735.h"                          // ST7735 1.8" LCD driver
#include    "netif/enc28j60.h"                  // ENC28J60 Ethernet device
#include    "netif/enc28j60-hw.h"

/* -----------------------------------------
   definitions and globals
----------------------------------------- */
// ENC28J60 device register access functions
static void   ethReset(void);
static int    setRegisterBank(regBank_t);
static int    readControlRegister(ctrlReg_t, u8_t*);
static int    writeControlRegister(ctrlReg_t, u8_t);
static int    readPhyRegister(phyReg_t, u16_t*);
static int    controlBit(ctrlReg_t, u8_t);
static int    setControlBit(ctrlReg_t, u8_t);
static int    clearControlBit(ctrlReg_t, u8_t);

#define     SPI_TEST_PARAM          -6          // to fit within the returned values of SPI IO error
#define     USAGE                   "-r SPI-RD, -w SPI WR, -l LCD test -br DMA block read -bw DMA block write -e Ethernet device"

enum {NOTHING, SPIIO, BLOCK, LCD, ETHER} whatToDo;
volatile int            completeFlag = 0;

#define     FRAME_BUFF           6              // should be 40960 for the LCD
//static unsigned char    frameBuffer[FRAME_BUFF] = {0x55, 0x01, 0xaa, 0xf0, 0xcc, 0x0f}; // test data
static unsigned char    frameBuffer[FRAME_BUFF] = {0x18, 0x18, 0x18, 0x18, 0x18, 0x18}; // test data
unsigned char*          redBuffer;
unsigned char*          blueBuffer;
unsigned char*          tempBuffer[2];
u16_t                   phyID1 = 0, phyID2 = 0, phyCON1 = 0;
u8_t                    ethReg;

/* -----------------------------------------
   startup code
----------------------------------------- */

/*------------------------------------------------
 * spiCallBack()
 *
 *  DMA completion callback
 *
 */
void spiCallBack(void)
{
    completeFlag = 1;
    printf("spiCallBack()\n");
}

/*------------------------------------------------
 * main()
 *
 */
int main(int argc, char* argv[])
{
    int             i, err = SPI_OK, isRead = 0;
    unsigned char   byte;

    int             x, y, duplex;

    whatToDo = NOTHING;

    // build date
    //
    printf("Build: spitest.exe %s %s\n", __DATE__, __TIME__);

    // parse command line variables
    //
    if ( argc == 1 )
    {
        printf("%s\n", USAGE);
        err = SPI_TEST_PARAM;
        goto ABORT;
    }

    for ( i = 1; i < argc; i++ )
    {
        if ( strcmp(argv[i], "-r") == 0 )
        {
            isRead = 1;
            whatToDo = SPIIO;
        }
        else if ( strcmp(argv[i], "-w") == 0 )
        {
            isRead = 0;
            whatToDo = SPIIO;
        }
        else if ( strcmp(argv[i], "-l") == 0 )
        {
            whatToDo = LCD;
        }
        else if ( strcmp(argv[i], "-br") == 0 )
        {
            isRead = 1;
            whatToDo = BLOCK;
        }
        else if ( strcmp(argv[i], "-bw") == 0 )
        {
            isRead = 0;
            whatToDo = BLOCK;
        }
        else if ( strcmp(argv[i], "-e") == 0 )
        {
            whatToDo = ETHER;
        }
        else
        {
            printf("%s\n", USAGE);
            err = SPI_TEST_PARAM;
            goto ABORT;
        }
    }

    // test selector driver
    //
    switch ( whatToDo )
    {
    case NOTHING:
        break;

    case SPIIO:                                 // SPI read write test or dummy data
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
        break;

    case BLOCK:                                 // write a block of data to SPI using DMA channel 0
        spiIoInit();
        printf("spiIoInit() done\n");
        if ( isRead )
        {
            for (i = 0; i < FRAME_BUFF; i++)
                printf("0x%x ", frameBuffer[i]);
            printf("\n");
            spiReadBlock(ETHERNET_RD, frameBuffer, FRAME_BUFF-2, spiCallBack);
            printf("spiReadBlock() done\n");
        }
        else
        {
            spiWriteBlock(ETHERNET_WR, frameBuffer, FRAME_BUFF, spiCallBack);
            printf("spiWriteBlock() done\n");
        }

        while ( !completeFlag ) {}              // *** connect logic analyzer to MOSI line and check data patterns ***

        for (i = 0; i < FRAME_BUFF; i++)
            printf("0x%x ", frameBuffer[i]);
        printf("\n");
        break;

    case LCD:                                   // LCD test
        spiIoInit();
        lcdInit();
        redBuffer = lcdFrameBufferInit(ST7735_RED);
        blueBuffer = lcdFrameBufferInit(ST7735_BLUE);

        if ( blueBuffer == 0 || redBuffer == 0 )
        {
            printf("lcdFrameBufferInit() failed redBuffer=%lu blueBuffer=%lu\n", redBuffer, blueBuffer);
        }
        else
        {
            tempBuffer[0] = redBuffer;
            tempBuffer[1] = blueBuffer;
            for ( i = 0; i < 10; i++)
            {
                completeFlag = 0;
                lcdFrameBufferPush(tempBuffer[(i % 2)], spiCallBack);
                while ( !completeFlag ) {}
            }
        }

        lcdFrameBufferFree(redBuffer);
        lcdFrameBufferFree(blueBuffer);
        /*
        lcdFillScreen(ST7735_BLUE);
        for (y=0; y < lcdHeight(); y+=5)       // draw some lines in yellow
        {
            lcdDrawFastHLine(0, y, lcdWidth(), ST7735_YELLOW);
        }
        for (x=0; x < lcdWidth(); x+=5)
        {
            lcdDrawFastVLine(x, 0, lcdHeight(), ST7735_YELLOW);
        }
        */
        break;

    case ETHER:                                 // ethernet test
        spiIoInit();
        ethReset();
        //while ( controlBit(ESTAT, ESTAT_CLKRDY) == 0 ) {};  // wait for system clock to be ready before starting initialization @@ timeouts?
        readControlRegister(EREVID, &ethReg);
        readPhyRegister(PHID1, &phyID1);        // read and store PHY ID
        readPhyRegister(PHID2, &phyID2);
        readPhyRegister(PHID2, &phyCON1);
        duplex = ((phyCON1 & PHCON1_PDPXMD) ? 1 : 0);

        printf("EREVID = 0x%02x\n", ethReg);
        printf("device ID: phyID1=0x%04x phyID2=0x%04x\n", phyID1, phyID2);
        printf("wired for %s duplex PDPXMD = %d\n", (duplex ? "full" : "half"), duplex);
        break;

    }

ABORT:
    return err;
}

/* -----------------------------------------
   Ethernet utilities (see 'enc28j60.c')
----------------------------------------- */

static int setRegisterBank(regBank_t bank)
{
    u8_t    econ1Reg;
    int     result = SPI_BUSY;

    if ( bank == BANK0 || bank == BANK1 || bank == BANK2 || bank == BANK3 )
    {
        spiWriteByteKeepCS(ETHERNET_WR, (OP_RCR | ECON1));  // read ECON1
        if ( (result = spiReadByte(ETHERNET_RD, &econ1Reg)) != SPI_OK )
            goto ABORT_BANKSEL;
        econ1Reg &= 0xfc;                                   // set bank number bits [BSEL1:BSEL0]
        econ1Reg |= (((u8_t)bank) >> 5);
        spiWriteByteKeepCS(ETHERNET_WR, (OP_WCR | ECON1));  // write ECON1
        spiWriteByte(ETHERNET_WR, econ1Reg);
    }

ABORT_BANKSEL:
    //printf("setRegisterBank(%d) %d\n", bank, result);
    return result;                                          // bad bank number parameter passed in
}

static int readControlRegister(ctrlReg_t reg, u8_t* byte)
{
    u8_t    bank;
    u8_t    regId;
    u8_t    regValue;
    int     result = SPI_RD_ERR;

    bank = (reg & 0x60);                                // isolate bank number
    regId = (reg & 0x1f);                               // isolate register number

    if ( (result = setRegisterBank(bank)) != SPI_OK)    // select bank
        goto ABORT_RCR;

    spiWriteByteKeepCS(ETHERNET_WR, (OP_RCR | regId));  // issue a control register read command

    if ( reg & MACMII )                                 // is this a MAC or MII register?
    {                                                   // yes, then issue an extra dummy read
        if ( (result = spiReadByteKeepCS(ETHERNET_RD, &regValue)) != SPI_OK)
            goto ABORT_RCR;
    }

    if ( (result = spiReadByte(ETHERNET_RD, &regValue)) != SPI_OK) //issue actual read for the byte
        goto ABORT_RCR;

    *byte = regValue;

ABORT_RCR:
    //printf("readControlRegister(0x%02x,0x%02x) %d\n", reg, *byte, result);
    return result;
}

static int writeControlRegister(ctrlReg_t reg, u8_t byte)
{
    u8_t    bank;
    u8_t    regId;
    u8_t    regValue;
    int     result = SPI_WR_ERR;

    bank = (reg & 0x60);                                // isolate bank number
    regId = (reg & 0x1f);                               // isolate register number

    if ( (result = setRegisterBank(bank)) != SPI_OK)    // select bank
        goto ABORT_WCR;

    spiWriteByteKeepCS(ETHERNET_WR, (OP_WCR | regId));  // issue a control register write command
    spiWriteByte(ETHERNET_WR, byte);                    // and send the data byte

ABORT_WCR:
    //printf("writeControlRegister(0x%02x,0x%02x) %d\n", reg, byte, result);
    return result;
}

static int controlBit(ctrlReg_t reg, u8_t position)
{
    u8_t    value;
    int     result = 0;                                 // which is also 'SPI_BUSY'...

    if ( readControlRegister(reg, &value) == SPI_OK)    // get register value
        result = ((value & position) ? 1 : 0);

    //printf("controlBit(0x%02x,0x%02x) %d\n", reg, position, result);
    return result;
}

static int readPhyRegister(phyReg_t reg, u16_t* word)
{
    u8_t    byte;
    int     result = SPI_RD_ERR;

    if ( (result = writeControlRegister(MIREGADR, reg)) != SPI_OK)  // set PHY register address
        goto ABORT_PHYRD;

    setControlBit(MICMD, MICMD_MIIRD);                  // start the read operation
    while ( controlBit(MISTAT, MISTAT_BUSY) ) {};       // wait for read operation to complete @@ timeouts?
    clearControlBit(MICMD, MICMD_MIIRD);                // clear read request
    readControlRegister(MIRDL, &byte);                  // read low byte
    *word = byte;
    readControlRegister(MIRDH, &byte);                  // read high byte
    *word += ((u16_t) byte) << 8;

ABORT_PHYRD:
    //printf("readPhyRegister(0x%02x,0x%04x) %d\n", reg, *word, result);
    return result;
}

static int setControlBit(ctrlReg_t reg, u8_t position)
{
    u8_t    bank;
    u8_t    regId;
    u8_t    regValue;
    int     result = -1;                                    // which is also 'SPI_BUSY'...

    if ( reg & MACMII )                                     // is this a MAC or MII register?
    {                                                       // yes, can't use BFS command on MAC or MII, but...
        result = readControlRegister(reg, &regValue);       // can read register,
        regValue |= position;                               // modify
        result = writeControlRegister(reg, regValue);       // and write back
    }
    else
    {
        bank = (reg & 0x60);                                // isolate bank number
        regId = (reg & 0x1f);                               // isolate register number

        if ( (result = setRegisterBank(bank)) != SPI_OK)    // select bank
            goto ABORT_BFS;

        spiWriteByteKeepCS(ETHERNET_WR, (OP_BFS | regId));  // issue a control register write command
        spiWriteByte(ETHERNET_WR, position);                // and send the data byte
    }

ABORT_BFS:
    //printf("setControlBit(0x%02x,0x%02x) %d\n", reg, position, result);
    return result;
}

static int clearControlBit(ctrlReg_t reg, u8_t position)
{
    u8_t    bank;
    u8_t    regId;
    u8_t    regValue;
    int     result = -1;                                    // which is also 'SPI_BUSY'...

    if ( reg & MACMII )                                     // is this a MAC or MII register?
    {                                                       // yes, can't use BFC command on MAC or MII, but...
        result = readControlRegister(reg, &regValue);       // can read register,
        regValue &= ~position;                              // modify
        result = writeControlRegister(reg, regValue);       // and write back
    }
    else
    {
        bank = (reg & 0x60);                                // isolate bank number
        regId = (reg & 0x1f);                               // isolate register number

        if ( (result = setRegisterBank(bank)) != SPI_OK)    // select bank
            goto ABORT_BFC;

        spiWriteByteKeepCS(ETHERNET_WR, (OP_BFC | regId));  // issue a control register write command
        spiWriteByte(ETHERNET_WR, position);                // and send the data byte
    }

ABORT_BFC:
    //printf("clearControlBit(0x%02x,0x%02x) %d\n", reg, position, result);
    return result;
}

static void ethReset(void)
{
    spiWriteByte(ETHERNET_WR, OP_SC);                   // issue a reset command
    while ( controlBit(ESTAT, ESTAT_CLKRDY) == 0 ) {};  // wait for system clock to be ready @@ timeouts?
    printf("ethReset()\n");
}
