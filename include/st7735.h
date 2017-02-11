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
void         lcdInit(void);                                                     // LCD initialization
void         setAddrWindow(unsigned char, unsigned char, unsigned char, unsigned char); //
void         pushColor(unsigned int);                                           // what does this do
void         fillScreen(unsigned int);                                          // fill screen with a solid color
void         drawPixel(int, int, unsigned int);                                 // draw a pixel
void         drawFastVLine(int, int, int, unsigned int);
void         drawFastHLine(int, int, int, unsigned int);
void         fillRect(int, int, int, int, unsigned int);
void         setRotation(unsigned char);                                        // screen rotation
void         invertDisplay(int i);
unsigned int lcdColor565(unsigned char, unsigned char, unsigned char);          // Pass 8-bit (each) R,G,B, get back 16-bit packed color
int          lcdHeight(void);
int          lcdWidth(void);

#endif  /* end __st7735_h__ */

