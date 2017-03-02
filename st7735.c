/***************************************************
  This is a library for the Adafruit 1.8" SPI display.

This library works with the Adafruit 1.8" TFT Breakout w/SD card
  ----> http://www.adafruit.com/products/358
The 1.8" TFT shield
  ----> https://www.adafruit.com/product/802
The 1.44" TFT breakout
  ----> https://www.adafruit.com/product/2088
as well as Adafruit raw 1.8" TFT display
  ----> http://www.adafruit.com/products/618

  Check out the links above for our tutorials and wiring diagrams
  These displays use SPI to communicate, 4 or 5 pins are required to
  interface (RST is optional)
  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!

  Written by Limor Fried/Ladyada for Adafruit Industries.
  MIT license, all text above must be included in any redistribution
 ****************************************************/

/*
 * this library and library functions were derived from the
 * Ada Fruit ST7735 1.8" TFT display library code for Arduino
 * the code was adapted to C, and to the unique HW setup used with the 
 * v25 CPU board interfacing to SPI through a 8255 PPI / AVR combination
 *
 * excellent resource here: https://warmcat.com/embedded/lcd/tft/st7735/2016/08/26/st7735-tdt-lcd-goodness.html
 *
 */

#include    <stdlib.h>
#include    <string.h>

#include    "st7735.h"
#include    "ppispi.h"                          // 8255 PPI to AVR/SPI driver

/* -----------------------------------------
   definitions and types
----------------------------------------- */
#define     ST7735_NOP          0x00            // LCD controller commands
#define     ST7735_SWRESET      0x01
#define     ST7735_RDDID        0x04
#define     ST7735_RDDST        0x09

#define     ST7735_SLPIN        0x10
#define     ST7735_SLPOUT       0x11
#define     ST7735_PTLON        0x12
#define     ST7735_NORON        0x13

#define     ST7735_INVOFF       0x20
#define     ST7735_INVON        0x21
#define     ST7735_DISPOFF      0x28
#define     ST7735_DISPON       0x29
#define     ST7735_CASET        0x2A
#define     ST7735_RASET        0x2B
#define     ST7735_RAMWR        0x2C
#define     ST7735_RAMRD        0x2E

#define     ST7735_PTLAR        0x30
#define     ST7735_COLMOD       0x3A
#define     ST7735_MADCTL       0x36

#define     ST7735_FRMCTR1      0xB1
#define     ST7735_FRMCTR2      0xB2
#define     ST7735_FRMCTR3      0xB3
#define     ST7735_INVCTR       0xB4
#define     ST7735_DISSET5      0xB6

#define     ST7735_PWCTR1       0xC0
#define     ST7735_PWCTR2       0xC1
#define     ST7735_PWCTR3       0xC2
#define     ST7735_PWCTR4       0xC3
#define     ST7735_PWCTR5       0xC4
#define     ST7735_VMCTR1       0xC5

#define     ST7735_RDID1        0xDA
#define     ST7735_RDID2        0xDB
#define     ST7735_RDID3        0xDC
#define     ST7735_RDID4        0xDD

#define     ST7735_PWCTR6       0xFC

#define     ST7735_GMCTRP1      0xE0
#define     ST7735_GMCTRN1      0xE1

#define     MADCTL_MY           0x80            // MAD display control register command bits
#define     MADCTL_MX           0x40
#define     MADCTL_MV           0x20
#define     MADCTL_ML           0x10
#define     MADCTL_RGB          0x00
#define     MADCTL_BGR          0x08
#define     MADCTL_MH           0x04

#define     ST7735_TFTWIDTH     128             // 1.8" LCD pixes geometry
#define     ST7735_TFTHEIGHT    160

#define     DELAY               0x80
#define     ONE_MILI_SEC        260             // loop count for 1mSec (was 210)

/* -----------------------------------------
   globals
----------------------------------------- */
static int             colstart, rowstart;
static int             _width, _height;
static int             rotation;
static unsigned char*  frameBuffer;

// rather than lcdWriteCommand() and lcdWriteData() calls, screen
// initialization commands and arguments are organized in a table.
// table is read, parsed and issues by lcdCommandList()
static const unsigned char
initSeq[] =
    {                                       // consolidated initialization sequence
        21,                                 // 21 commands in list:
        ST7735_SWRESET,   DELAY,            //  1: Software reset, 0 args, w/delay
          150,                              //     150 ms delay
        ST7735_SLPOUT ,   DELAY,            //  2: Out of sleep mode, 0 args, w/delay
          150,                              //     150mSec delay per spec pg.94             *** was 500 ms delay value 255)
        ST7735_FRMCTR1, 3      ,            //  3: Frame rate ctrl - normal mode, 3 args:
          0x01, 0x2C, 0x2D,                 //     Rate = fosc/((1x2+40) * (LINE+2C+2D+2))  *** several opinions here, like  0x00, 0x06, 0x03,
        ST7735_FRMCTR2, 3      ,            //  4: Frame rate control - idle mode, 3 args:
          0x01, 0x2C, 0x2D,                 //     Rate = fosc/(1x2+40) * (LINE+2C+2D)      *** same as above
        ST7735_FRMCTR3, 6      ,            //  5: Frame rate ctrl - partial mode, 6 args:
          0x01, 0x2C, 0x2D,                 //     Dot inversion mode                       *** same as above
          0x01, 0x2C, 0x2D,                 //     Line inversion mode
        ST7735_INVCTR , 1      ,            //  6: Display inversion ctrl, 1 arg, no delay:
          0x07,                             //     No inversion                             *** set to post-reset default value
        ST7735_PWCTR1 , 3      ,            //  7: Power control, 3 args, no delay:
          0xA2,                             //     AVDD = 5v, VRHP = 4.6v
          0x02,                             //     VRHN = -4.6V
          0x84,                             //     AUTO mode
        ST7735_PWCTR2 , 1      ,            //  8: Power control, 1 arg, no delay:
          0xC5,                             //     VGH25 = 2.4C VGSEL = -10 VGH = 3 * AVDD
        ST7735_PWCTR3 , 2      ,            //  9: Power control, 2 args, no delay:
          0x0A,                             //     Opamp current small
          0x00,                             //     Boost frequency
        ST7735_PWCTR4 , 2      ,            // 10: Power control, 2 args, no delay:
          0x8A,                             //     BCLK/2, Opamp current small & Medium low
          0x2A,
        ST7735_PWCTR5 , 2      ,            // 11: Power control, 2 args, no delay:
          0x8A, 0xEE,
        ST7735_VMCTR1 , 1      ,            // 12: Power control, 1 arg, no delay:
          0x0E,
        ST7735_INVOFF , 0      ,            // 13: Don't invert display, no args, no delay
        ST7735_MADCTL , 1      ,            // 14: Memory access control (directions), 1 arg:
          0xC0,                             //     row addr/col addr, bottom to top refresh
        ST7735_COLMOD , 1      ,            // 15: set color mode, 1 arg, no delay:
          0x05,                             //     16-bit color
        ST7735_CASET  , 4      ,            // 16: Column addr set, 4 args, no delay:
          0x00, 0x00,                       //     XSTART = 0
          0x00, 0x7F,                       //     XEND   = 127
        ST7735_RASET  , 4      ,            // 17: Row addr set, 4 args, no delay:
          0x00, 0x00,                       //     YSTART = 0
          0x00, 0x9F,                       //     YEND   = 159
        ST7735_GMCTRP1, 16      ,           // 18: Gamma (‘+’polarity) Correction Characteristics Setting, 16 args, no delay:
          0x02, 0x1c, 0x07, 0x12,
          0x37, 0x32, 0x29, 0x2d,
          0x29, 0x25, 0x2B, 0x39,
          0x00, 0x01, 0x03, 0x10,
        ST7735_GMCTRN1, 16      ,           // 19: Gamma ‘-’polarity Correction Characteristics Setting, 16 args, no delay:
          0x03, 0x1d, 0x07, 0x06,
          0x2E, 0x2C, 0x29, 0x2D,
          0x2E, 0x2E, 0x37, 0x3F,
          0x00, 0x00, 0x02, 0x10,
        ST7735_NORON  ,    DELAY,           // 20: Normal display on, no args, w/delay
          10,                               //     10 ms delay
        ST7735_DISPON ,    DELAY,           // 21: Main screen turn on, no args w/delay
          100
    };

/* -----------------------------------------
   driver functions
----------------------------------------- */

/*------------------------------------------------
 * delay()
 *
 *  software loop delay
 *  hate this method, keeping it here just to get the driver check out
 *
 */
static void delay(unsigned int miliSec)
{
    unsigned int oneMs = ONE_MILI_SEC;

    if ( miliSec < 1 || miliSec > 60000 )
        miliSec = 1;

    while ( miliSec-- )                         // run through number of mili-sec requested
    {
        while ( oneMs-- ) {}                    // run through 1 mili second
        oneMs = ONE_MILI_SEC;                   // replenish the counter
    }
}

/*------------------------------------------------
 * lcdWriteCommand()
 *
 *  write a command byte to the ST7735 LCD
 *  return SPI error codes defined in 'spi.h'
 *
 */
static int lcdWriteCommand(unsigned char byte)
{
    return spiWriteByte(LCD_CMD, byte);
}

/*------------------------------------------------
 * lcdWriteData()
 *
 *  write a data byte to the ST7735 LCD
 *  return SPI error codes defined in 'spi.h'
 *
 */
static int lcdWriteData(unsigned char byte)
{
    return spiWriteByte(LCD_DATA, byte);
}

/*------------------------------------------------
 * lcdCommandList()
 *
 *  reads and issues a series of LCD commands groupd
 *  inside the initialized data tables
 *
 */
static void lcdCommandList(const unsigned char *addr)
{
    int     numCommands, numArgs;
    int     ms;

    numCommands = *(addr++);                    // Number of commands to follow
    while ( numCommands-- )                     // For each command...
    {
        lcdWriteCommand(*(addr++));             // Read, issue command
        numArgs  = *(addr++);                   // Number of args to follow
        ms       = numArgs & DELAY;             // If hibit set, delay follows args
        numArgs &= ~DELAY;                      // Mask out delay bit
        while ( numArgs-- )                     // For each argument...
        {
            lcdWriteData(*(addr++));            // Read, issue argument
        }

        if ( ms )
        {
            ms = *(addr++);                     // Read post-command delay time (ms)
            if ( ms == 255 )
                ms = 500;                       // If 255, delay for 500 ms
            delay(ms);
        }
    }
}


/*------------------------------------------------
 * lcdInit()
 *
 *  initialization of LCD screen
 *
 */
void lcdInit(void)
{
    colstart = 0;
    rowstart = 0;
    _width   = 0;
    _height  = 0;
    rotation = 0;

    lcdCommandList(initSeq);                    // initialization commands to the display
    lcdSetRotation(0);                          // set display rotaiton and size defaults
}

/*------------------------------------------------
 * lcdSetAddrWindow()
 *
 *  set LCD window size in pixes from top left to bottom right
 *  and setup for write to LCD RAM frame buffer
 *  any sunsequent write commands will go to RAN and be frawn on the display
 *
 */
void lcdSetAddrWindow(unsigned char x0, unsigned char y0, unsigned char x1, unsigned char y1)
{
    lcdWriteCommand(ST7735_CASET);              // Column addr set
    lcdWriteData(0x00);
    lcdWriteData(x0+colstart);                  // XSTART 
    lcdWriteData(0x00);
    lcdWriteData(x1+colstart);                  // XEND

    lcdWriteCommand(ST7735_RASET);              // Row addr set
    lcdWriteData(0x00);
    lcdWriteData(y0+rowstart);                  // YSTART
    lcdWriteData(0x00);
    lcdWriteData(y1+rowstart);                  // YEND

    lcdWriteCommand(ST7735_RAMWR);              // write to RAM
}

/*------------------------------------------------
 * lcdPushColor()
 *
 *  send  color pixel to LCD,
 *  must run after lcdSetAddrWindow()
 *
 */
void lcdPushColor(unsigned int color)
{
    lcdWriteData((unsigned char) (color >> 8));
    lcdWriteData((unsigned char) color);
}

/*------------------------------------------------
 * lcdDrawPixel()
 *
 *  drow a pixes in coordinate (x,y) with color
 *
 */
void lcdDrawPixel(int x, int y, unsigned int color)
{
    if ((x < 0) || (x >= _width) || (y < 0) || (y >= _height))
        return;

    lcdSetAddrWindow(x,y,x+1,y+1);
  
    lcdWriteData((unsigned char) (color >> 8));
    lcdWriteData((unsigned char) color);
}

/*------------------------------------------------
 * lcdDrawFastVLine()
 *
 *  draw vertical line from x,y length h and color
 *
 */
void lcdDrawFastVLine(int x, int y, int h, unsigned int color)
{
    unsigned char     hi, lo;

    // Rudimentary clipping
    if ((x >= _width) || (y >= _height))
        return;

    if ((y+h-1) >= _height)
        h = _height-y;

    lcdSetAddrWindow(x, y, x, y+h-1);

    hi = (unsigned char) (color >> 8);
    lo = (unsigned char) color;
    
    while (h--)
    {
        lcdWriteData(hi);
        lcdWriteData(lo);
    }
}

/*------------------------------------------------
 * lcdDrawFastHLine()
 *
 *  draw horizontal line from x,y with length w and color
 *
 */
void lcdDrawFastHLine(int x, int y, int w, unsigned int color)
{
    unsigned char     hi, lo;

    // Rudimentary clipping
    if ((x >= _width) || (y >= _height))
        return;

    if ((x+w-1) >= _width)
        w = _width-x;

    lcdSetAddrWindow(x, y, x+w-1, y);

    hi = (unsigned char) (color >> 8);
    lo = (unsigned char) color;

    while (w--)
    {
        lcdWriteData(hi);
        lcdWriteData(lo);
    }
}


/*------------------------------------------------
 * lcdFillScreen()
 *
 *  fill screen with solid color
 *
 */
void lcdFillScreen(unsigned int color)
{
    lcdFillRect(0, 0,  _width, _height, color);
}

/*------------------------------------------------
 * lcdFillRect()
 *
 *  draw a filled rectangle with solid color
 *
 */
void lcdFillRect(int x, int y, int w, int h, unsigned int color)
{
    unsigned char     hi, lo;

    // rudimentary clipping (drawChar w/big text requires this)
    if ((x >= _width) || (y >= _height))
        return;

    if ((x + w - 1) >= _width)
        w = _width  - x;

    if ((y + h - 1) >= _height)
        h = _height - y;

    lcdSetAddrWindow(x, y, x+w-1, y+h-1);

    hi = (unsigned char) (color >> 8);
    lo = (unsigned char) color;
    
    for (y=h; y>0; y--)
    {
        for (x=w; x>0; x--)
        {
            lcdWriteData(hi);
            lcdWriteData(lo);
        }
    }
}

/*------------------------------------------------
 * lcdColor565()
 *
 *  Pass 8-bit (each) R,G,B, get back 16-bit packed color
 *
 */
unsigned int lcdColor565(unsigned char r, unsigned char g, unsigned char b)
{
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

/*------------------------------------------------
 * lcdSetRotation()
 *
 *  screen rotation
 *
 */
void lcdSetRotation(unsigned char m)
{
    lcdWriteCommand(ST7735_MADCTL);
    if ( m > 3 )
        return;
    switch (rotation)
    {
    case 0:
        lcdWriteData(MADCTL_MX | MADCTL_MY | MADCTL_RGB);
        _width  = ST7735_TFTWIDTH;
        _height = ST7735_TFTHEIGHT;
        break;

    case 1:
        lcdWriteData(MADCTL_MY | MADCTL_MV | MADCTL_RGB);
        _width = ST7735_TFTHEIGHT;
        _height = ST7735_TFTWIDTH;
         break;

    case 2:
        lcdWriteData(MADCTL_RGB);
        _width  = ST7735_TFTWIDTH;
        _height = ST7735_TFTHEIGHT;
        break;

    case 3:
        lcdWriteData(MADCTL_MX | MADCTL_MV | MADCTL_RGB);
        _width = ST7735_TFTHEIGHT;
        _height = ST7735_TFTWIDTH;
        break;
    }
}

/*------------------------------------------------
 * lcdInvertDisplay()
 *
 *  inverse screen colors
 *
 */
void lcdInvertDisplay(int i)
{
    lcdWriteCommand(i ? ST7735_INVON : ST7735_INVOFF);
}

/*------------------------------------------------
 * lcdHeight()
 *
 *  LCD height in pixels
 *
 */
int lcdHeight(void)
{
    return _height;
}

/*------------------------------------------------
 * lcdWidth()
 *
 *  return LCD width in pixels
 *
 */
int lcdWidth(void)
{
    return _width;
}

/*------------------------------------------------
 * lcdFrameBufferInit()
 *
 *  initialize a frame buffer with a color
 *
 */
unsigned char* lcdFrameBufferInit(unsigned int color)
{
    unsigned char*  buffer;
    unsigned int    size;
    unsigned int    i;

    size = _width * _height * sizeof(unsigned int);         // calculate buffer size

    if ( (buffer = (unsigned char*) malloc(size)) == 0 )    // allocate memory and abort if cannot
        return 0;

    for ( i = 0; i < size; i += 2)                          // initialize buffer with color
    {
        buffer[i] = (unsigned char) (color >> 8);           // swap bytes in buffer
        buffer[i+1] = (unsigned char) color;                // so that writes yield correct order
    }

    return buffer;
}

/*------------------------------------------------
 * lcdFrameBufferFree()
 *
 *  release memory reserved for the frame buffer
 *
 */
void lcdFrameBufferFree(unsigned char* frameBufferPointer)
{
    free((void*) frameBufferPointer);                       // free buffer memory
}

/*------------------------------------------------
 * lcdFrameBufferPush()
 *
 *  tranfer frame buffer to LCD using DMA
 *  and call optional call-back function on completion
 *
 */
int lcdFrameBufferPush(unsigned char* frameBufferPointer, void(*completionCallBack)(void))
{
    unsigned int    size;

    size = _width * _height * sizeof(unsigned int);                                 // calculate frame buffer size
    lcdSetAddrWindow(0, 0, _width-1, _height-1);                                    // prepare display area
    return spiWriteBlock(LCD_DATA, frameBufferPointer, size, completionCallBack);   // push buffer data
}

/*------------------------------------------------
 * lcdFrameBufferScroll()
 *
 *  scroll frame buffer by +/- pixels and
 *  fill new lines with color
 *
 */
void lcdFrameBufferScroll(int pixels, unsigned int color)
{
}


