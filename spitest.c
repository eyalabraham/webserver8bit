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

#include    "ppispi.h"                          // 8255 PPI to AVR/SPI driver
#include    "st7735.h"                          // ST7735 1.8" LCD driver

/* -----------------------------------------
   definitions and globals
----------------------------------------- */
#define     SPI_TEST_PARAM          -6          // to fit within the returned values of SPI IO error
#define     USAGE                   "-r SPI-RD, -w SPI WR, -l LCD test -br DMA block read -bw DMA block write"

enum {NOTHING, SPIIO, BLOCK, LCD, ETHER} whatToDo;
volatile int            completeFlag = 0;

#define     FRAME_BUFF           6              // should be 40960 for the LCD
//static unsigned char    frameBuffer[FRAME_BUFF] = {0x55, 0x01, 0xaa, 0xf0, 0xcc, 0x0f}; // test data
static unsigned char    frameBuffer[FRAME_BUFF] = {0x18, 0x18, 0x18, 0x18, 0x18, 0x18}; // test data
unsigned char*          redBuffer;
unsigned char*          blueBuffer;
unsigned char*          tempBuffer[2];

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

    int             x, y;

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
        break;

    }

ABORT:
    return err;
}

