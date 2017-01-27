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


#ifndef     __st7735_h__
#define     __st7735_h__

// some flags for initR()
#define     INITR_GREENTAB      0x0
#define     INITR_REDTAB        0x1
#define     INITR_BLACKTAB      0x2

#define     INITR_18GREENTAB    INITR_GREENTAB
#define     INITR_18REDTAB      INITR_REDTAB
#define     INITR_18BLACKTAB    INITR_BLACKTAB
#define     INITR_144GREENTAB   0x1

#define     ST7735_TFTWIDTH     128
// for 1.44" display
#define     ST7735_TFTHEIGHT_144 128
// for 1.8" display
#define     ST7735_TFTHEIGHT_18  160

#define     ST7735_NOP          0x00
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

// Color definitions
#define	    ST7735_BLACK        0x0000
#define	    ST7735_BLUE         0x001F
#define	    ST7735_RED          0xF800
#define	    ST7735_GREEN        0x07E0
#define     ST7735_CYAN         0x07FF
#define     ST7735_MAGENTA      0xF81F
#define     ST7735_YELLOW       0xFFE0
#define     ST7735_WHITE        0xFFFF

/* -----------------------------------------
   function prototypes
----------------------------------------- */
void initB(void);                                                       // for ST7735B displays
void initR(unsigned char );                                             // for ST7735R, I need options = INITR_GREENTAB
void setAddrWindow(unsigned char, unsigned char, unsigned char, unsigned char); // 
void pushColor(unsigned int);                                           // what does this do
void fillScreen(unsigned int);                                          // fill screen with a solid color
void drawPixel(int, int, unsigned int);                                 // draw a pixel
void drawFastVLine(int, int, int, unsigned int);
void drawFastHLine(int, int, int, unsigned int);
void fillRect(int, int, int, int, unsigned int);
void setRotation(unsigned char);                                        // screen rotation
void invertDisplay(int i);
int  lcdColor565(unsigned char, unsigned char, unsigned char);          // Pass 8-bit (each) R,G,B, get back 16-bit packed color
int  lcdHeight(void);
int  lcdWidth(void);

#endif  /* end __st7735_h__ */

