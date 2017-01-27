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
 */

#include    "st7735.h"
#include    "ppispi.h"                          // 8255 PPI to AVR/SPI driver

/*
inline int swapcolor(int x) { 
  return (x << 11) | (x & 0x07E0) | (x >> 11);
}

#if defined (SPI_HAS_TRANSACTION)
  static SPISettings mySPISettings;
#elif defined (__AVR__)
  static unsigned char SPCRbackup;
  static unsigned char mySPCR;
#endif
*/

/* -----------------------------------------
   definitions and types
----------------------------------------- */
#define     DELAY           0x80
#define     ONE_MILI_SEC    210

/* -----------------------------------------
   globals
----------------------------------------- */
int     colstart, rowstart;
int     tabcolor;
int     _width, _height;
int     rotation;

// Rather than a bazillion writecommand() and writedata() calls, screen
// initialization commands and arguments are organized in these tables
// stored in PROGMEM.  The table may look bulky, but that's mostly the
// formatting -- storage-wise this is hundreds of bytes more compact
// than the equivalent code.  Companion function follows.
static const unsigned char
  Bcmd[] = {                  // Initialization commands for 7735B screens
    18,                       // 18 commands in list:
    ST7735_SWRESET,   DELAY,  //  1: Software reset, no args, w/delay
      50,                     //     50 ms delay
    ST7735_SLPOUT ,   DELAY,  //  2: Out of sleep mode, no args, w/delay
      255,                    //     255 = 500 ms delay
    ST7735_COLMOD , 1+DELAY,  //  3: Set color mode, 1 arg + delay:
      0x05,                   //     16-bit color
      10,                     //     10 ms delay
    ST7735_FRMCTR1, 3+DELAY,  //  4: Frame rate control, 3 args + delay:
      0x00,                   //     fastest refresh
      0x06,                   //     6 lines front porch
      0x03,                   //     3 lines back porch
      10,                     //     10 ms delay
    ST7735_MADCTL , 1      ,  //  5: Memory access ctrl (directions), 1 arg:
      0x08,                   //     Row addr/col addr, bottom to top refresh
    ST7735_DISSET5, 2      ,  //  6: Display settings #5, 2 args, no delay:
      0x15,                   //     1 clk cycle nonoverlap, 2 cycle gate
                              //     rise, 3 cycle osc equalize
      0x02,                   //     Fix on VTL
    ST7735_INVCTR , 1      ,  //  7: Display inversion control, 1 arg:
      0x0,                    //     Line inversion
    ST7735_PWCTR1 , 2+DELAY,  //  8: Power control, 2 args + delay:
      0x02,                   //     GVDD = 4.7V
      0x70,                   //     1.0uA
      10,                     //     10 ms delay
    ST7735_PWCTR2 , 1      ,  //  9: Power control, 1 arg, no delay:
      0x05,                   //     VGH = 14.7V, VGL = -7.35V
    ST7735_PWCTR3 , 2      ,  // 10: Power control, 2 args, no delay:
      0x01,                   //     Opamp current small
      0x02,                   //     Boost frequency
    ST7735_VMCTR1 , 2+DELAY,  // 11: Power control, 2 args + delay:
      0x3C,                   //     VCOMH = 4V
      0x38,                   //     VCOML = -1.1V
      10,                     //     10 ms delay
    ST7735_PWCTR6 , 2      ,  // 12: Power control, 2 args, no delay:
      0x11, 0x15,
    ST7735_GMCTRP1,16      ,  // 13: Magical unicorn dust, 16 args, no delay:
      0x09, 0x16, 0x09, 0x20, //     (seriously though, not sure what
      0x21, 0x1B, 0x13, 0x19, //      these config values represent)
      0x17, 0x15, 0x1E, 0x2B,
      0x04, 0x05, 0x02, 0x0E,
    ST7735_GMCTRN1,16+DELAY,  // 14: Sparkles and rainbows, 16 args + delay:
      0x0B, 0x14, 0x08, 0x1E, //     (ditto)
      0x22, 0x1D, 0x18, 0x1E,
      0x1B, 0x1A, 0x24, 0x2B,
      0x06, 0x06, 0x02, 0x0F,
      10,                     //     10 ms delay
    ST7735_CASET  , 4      ,  // 15: Column addr set, 4 args, no delay:
      0x00, 0x02,             //     XSTART = 2
      0x00, 0x81,             //     XEND = 129
    ST7735_RASET  , 4      ,  // 16: Row addr set, 4 args, no delay:
      0x00, 0x02,             //     XSTART = 1
      0x00, 0x81,             //     XEND = 160
    ST7735_NORON  ,   DELAY,  // 17: Normal display on, no args, w/delay
      10,                     //     10 ms delay
    ST7735_DISPON ,   DELAY,  // 18: Main screen turn on, no args, w/delay
      255 },                  //     255 = 500 ms delay

  Rcmd1[] = {                 // Init for 7735R, part 1 (red or green tab)
    15,                       // 15 commands in list:
    ST7735_SWRESET,   DELAY,  //  1: Software reset, 0 args, w/delay
      150,                    //     150 ms delay
    ST7735_SLPOUT ,   DELAY,  //  2: Out of sleep mode, 0 args, w/delay
      255,                    //     500 ms delay
    ST7735_FRMCTR1, 3      ,  //  3: Frame rate ctrl - normal mode, 3 args:
      0x01, 0x2C, 0x2D,       //     Rate = fosc/(1x2+40) * (LINE+2C+2D)
    ST7735_FRMCTR2, 3      ,  //  4: Frame rate control - idle mode, 3 args:
      0x01, 0x2C, 0x2D,       //     Rate = fosc/(1x2+40) * (LINE+2C+2D)
    ST7735_FRMCTR3, 6      ,  //  5: Frame rate ctrl - partial mode, 6 args:
      0x01, 0x2C, 0x2D,       //     Dot inversion mode
      0x01, 0x2C, 0x2D,       //     Line inversion mode
    ST7735_INVCTR , 1      ,  //  6: Display inversion ctrl, 1 arg, no delay:
      0x07,                   //     No inversion
    ST7735_PWCTR1 , 3      ,  //  7: Power control, 3 args, no delay:
      0xA2,
      0x02,                   //     -4.6V
      0x84,                   //     AUTO mode
    ST7735_PWCTR2 , 1      ,  //  8: Power control, 1 arg, no delay:
      0xC5,                   //     VGH25 = 2.4C VGSEL = -10 VGH = 3 * AVDD
    ST7735_PWCTR3 , 2      ,  //  9: Power control, 2 args, no delay:
      0x0A,                   //     Opamp current small
      0x00,                   //     Boost frequency
    ST7735_PWCTR4 , 2      ,  // 10: Power control, 2 args, no delay:
      0x8A,                   //     BCLK/2, Opamp current small & Medium low
      0x2A,  
    ST7735_PWCTR5 , 2      ,  // 11: Power control, 2 args, no delay:
      0x8A, 0xEE,
    ST7735_VMCTR1 , 1      ,  // 12: Power control, 1 arg, no delay:
      0x0E,
    ST7735_INVOFF , 0      ,  // 13: Don't invert display, no args, no delay
    ST7735_MADCTL , 1      ,  // 14: Memory access control (directions), 1 arg:
      0xC8,                   //     row addr/col addr, bottom to top refresh
    ST7735_COLMOD , 1      ,  // 15: set color mode, 1 arg, no delay:
      0x05 },                 //     16-bit color

  Rcmd2green[] = {            // Init for 7735R, part 2 (green tab only)
    2,                        //  2 commands in list:
    ST7735_CASET  , 4      ,  //  1: Column addr set, 4 args, no delay:
      0x00, 0x02,             //     XSTART = 0
      0x00, 0x7F+0x02,        //     XEND = 127
    ST7735_RASET  , 4      ,  //  2: Row addr set, 4 args, no delay:
      0x00, 0x01,             //     XSTART = 0
      0x00, 0x9F+0x01 },      //     XEND = 159
  Rcmd2red[] = {              // Init for 7735R, part 2 (red tab only)
    2,                        //  2 commands in list:
    ST7735_CASET  , 4      ,  //  1: Column addr set, 4 args, no delay:
      0x00, 0x00,             //     XSTART = 0
      0x00, 0x7F,             //     XEND = 127
    ST7735_RASET  , 4      ,  //  2: Row addr set, 4 args, no delay:
      0x00, 0x00,             //     XSTART = 0
      0x00, 0x9F },           //     XEND = 159

  Rcmd2green144[] = {              // Init for 7735R, part 2 (green 1.44 tab)
    2,                        //  2 commands in list:
    ST7735_CASET  , 4      ,  //  1: Column addr set, 4 args, no delay:
      0x00, 0x00,             //     XSTART = 0
      0x00, 0x7F,             //     XEND = 127
    ST7735_RASET  , 4      ,  //  2: Row addr set, 4 args, no delay:
      0x00, 0x00,             //     XSTART = 0
      0x00, 0x7F },           //     XEND = 127

  Rcmd3[] = {                 // Init for 7735R, part 3 (red or green tab)
    4,                        //  4 commands in list:
    ST7735_GMCTRP1, 16      , //  1: Magical unicorn dust, 16 args, no delay:
      0x02, 0x1c, 0x07, 0x12,
      0x37, 0x32, 0x29, 0x2d,
      0x29, 0x25, 0x2B, 0x39,
      0x00, 0x01, 0x03, 0x10,
    ST7735_GMCTRN1, 16      , //  2: Sparkles and rainbows, 16 args, no delay:
      0x03, 0x1d, 0x07, 0x06,
      0x2E, 0x2C, 0x29, 0x2D,
      0x2E, 0x2E, 0x37, 0x3F,
      0x00, 0x00, 0x02, 0x10,
    ST7735_NORON  ,    DELAY, //  3: Normal display on, no args, w/delay
      10,                     //     10 ms delay
    ST7735_DISPON ,    DELAY, //  4: Main screen turn on, no args w/delay
      100 };                  //     100 ms delay

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
 *  reads and issues a series of LCD commands groupd
 *  inside the initialized data tables
 *  Initialization code common to both 'B' and 'R' type displays
 *
 */
static void lcdInit(const unsigned char *cmdList)
{
    colstart = 0;
    rowstart = 0;


    // *** interesting, seems that in order to reset the LCD, CS needs to be low (active) ***
    // did not find anywhere in the data sheet that states CS needs to be low in order for RESX to be acknowledged
    // RESX down need to be min 120mSec.

    // toggle RST low to reset; CS low so it'll listen to us
    /*
    *csport &= ~cspinmask;
    if ( _rst )
    {
        pinMode(_rst, OUTPUT);
        digitalWrite(_rst, HIGH);
        delay(500);
        digitalWrite(_rst, LOW);
        delay(500);
        digitalWrite(_rst, HIGH);
        delay(500);
    }
    */

    if ( cmdList )
        lcdCommandList(cmdList);
}


// Initialization for ST7735B screens
void initB(void)
{
    lcdInit(Bcmd);
}


// *** what I probably need is only this function with options == INITR_GREENTAB ***
// *** potential to reduce initialization code to my specific LCD model          ***
// Initialization for ST7735R screens (green or red tabs)
void initR(unsigned char options)
{
    lcdInit(Rcmd1);
    if ( options == INITR_GREENTAB )
    {
        lcdCommandList(Rcmd2green);
        colstart = 2;
        rowstart = 1;
    } 
    else if ( options == INITR_144GREENTAB )
    {
        _height = ST7735_TFTHEIGHT_144;
        lcdCommandList(Rcmd2green144);
        colstart = 2;
        rowstart = 3;
    }
    else
    {
        // colstart, rowstart left at default '0' values
        lcdCommandList(Rcmd2red);
    }

    lcdCommandList(Rcmd3);

    // if black, change MADCTL color filter
    if ( options == INITR_BLACKTAB )
    {
        lcdWriteCommand(ST7735_MADCTL);
        lcdWriteData(0xC0);
    }

    tabcolor = options;
}

/*------------------------------------------------
 * setAddrWindow()
 *
 *  set LCD window size in pixes from top left to bottom right
 *  and setup for write to LCD RAM frame buffer
 *  any sunsequent write commands will go to RAN and be frawn on the display
 *
 */
void setAddrWindow(unsigned char x0, unsigned char y0, unsigned char x1, unsigned char y1)
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
 * pushColor()
 *
 *  what does this do?
 *
 */
void pushColor(unsigned int color)
{
    lcdWriteData((unsigned char) (color >> 8));
    lcdWriteData((unsigned char) color);
}

/*------------------------------------------------
 * drawPixel()
 *
 *  drow a pixes in coordinate (x,y) with color
 *
 */
void drawPixel(int x, int y, unsigned int color)
{
    if ((x < 0) || (x >= _width) || (y < 0) || (y >= _height))
        return;

    setAddrWindow(x,y,x+1,y+1);
  
    lcdWriteData((unsigned char) color >> 8);
    lcdWriteData((unsigned char) color);
}

/*------------------------------------------------
 * drawFastVLine()
 *
 *  draw vertical line from x,y length h and color
 *
 */
void drawFastVLine(int x, int y, int h, unsigned int color)
{
    unsigned char     hi, lo;

    // Rudimentary clipping
    if ((x >= _width) || (y >= _height))
        return;

    if ((y+h-1) >= _height)
        h = _height-y;

    setAddrWindow(x, y, x, y+h-1);

    hi = (unsigned char) color >> 8;
    lo = (unsigned char) color;
    
    while (h--)
    {
        lcdWriteData(hi);
        lcdWriteData(lo);
    }
}

/*------------------------------------------------
 * drawFastHLine()
 *
 *  draw horizontal line from x,y with length w and color
 *
 */
void drawFastHLine(int x, int y, int w, unsigned int color)
{
    unsigned char     hi, lo;

    // Rudimentary clipping
    if ((x >= _width) || (y >= _height))
        return;

    if ((x+w-1) >= _width)
        w = _width-x;

    setAddrWindow(x, y, x+w-1, y);

    hi = (unsigned char) color >> 8;
    lo = (unsigned char) color;

    while (w--)
    {
        lcdWriteData(hi);
        lcdWriteData(lo);
    }
}


/*------------------------------------------------
 * fillScreen()
 *
 *  fill screen with solid color
 *
 */
void fillScreen(unsigned int color)
{
    fillRect(0, 0,  _width, _height, color);
}

/*------------------------------------------------
 * fillRect()
 *
 *  draw a filled rectangle with solid color
 *
 */
void fillRect(int x, int y, int w, int h, unsigned int color)
{
    unsigned char     hi, lo;

    // rudimentary clipping (drawChar w/big text requires this)
    if ((x >= _width) || (y >= _height))
        return;

    if ((x + w - 1) >= _width)
        w = _width  - x;

    if ((y + h - 1) >= _height)
        h = _height - y;

    setAddrWindow(x, y, x+w-1, y+h-1);

    hi = (unsigned char) color >> 8;
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
int lcdColor565(unsigned char r, unsigned char g, unsigned char b)
{
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

/*------------------------------------------------
 * setRotation()
 *
 *  screen rotation
 *
 */
#define     MADCTL_MY   0x80
#define     MADCTL_MX   0x40
#define     MADCTL_MV   0x20
#define     MADCTL_ML   0x10
#define     MADCTL_RGB  0x00
#define     MADCTL_BGR  0x08
#define     MADCTL_MH   0x04

void setRotation(unsigned char m)                     // *** room for improvement by removing code for LCD tab models other than green ***
{
    lcdWriteCommand(ST7735_MADCTL);
    rotation = m % 4;                           // can't be higher than 3
    switch (rotation)
    {
    case 0:
        if (tabcolor == INITR_BLACKTAB)
            lcdWriteData(MADCTL_MX | MADCTL_MY | MADCTL_RGB);
        else
            lcdWriteData(MADCTL_MX | MADCTL_MY | MADCTL_BGR);
     
        _width  = ST7735_TFTWIDTH;

        if (tabcolor == INITR_144GREENTAB) 
            _height = ST7735_TFTHEIGHT_144;
        else
            _height = ST7735_TFTHEIGHT_18;
        break;

    case 1:
        if (tabcolor == INITR_BLACKTAB)
            lcdWriteData(MADCTL_MY | MADCTL_MV | MADCTL_RGB);
        else
            lcdWriteData(MADCTL_MY | MADCTL_MV | MADCTL_BGR);

        if (tabcolor == INITR_144GREENTAB) 
            _width = ST7735_TFTHEIGHT_144;
        else
            _width = ST7735_TFTHEIGHT_18;

        _height = ST7735_TFTWIDTH;

         break;

    case 2:
        if (tabcolor == INITR_BLACKTAB)
            lcdWriteData(MADCTL_RGB);
        else
            lcdWriteData(MADCTL_BGR);

        _width  = ST7735_TFTWIDTH;

        if (tabcolor == INITR_144GREENTAB) 
            _height = ST7735_TFTHEIGHT_144;
        else
            _height = ST7735_TFTHEIGHT_18;

        break;

    case 3:
        if (tabcolor == INITR_BLACKTAB)
            lcdWriteData(MADCTL_MX | MADCTL_MV | MADCTL_RGB);
        else
            lcdWriteData(MADCTL_MX | MADCTL_MV | MADCTL_BGR);

        if (tabcolor == INITR_144GREENTAB) 
            _width = ST7735_TFTHEIGHT_144;
        else
            _width = ST7735_TFTHEIGHT_18;

        _height = ST7735_TFTWIDTH;

        break;
    }
}

/*------------------------------------------------
 * invertDisplay()
 *
 *  inverse screen colors
 *
 */
void invertDisplay(int i)
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
