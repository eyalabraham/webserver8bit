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

#include    <sys/types.h>

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

#define     FONT_PIX_WIDE       6
#define     FONT_PIX_HIGH       8

/* -----------------------------------------
   function prototypes
----------------------------------------- */

/*------------------------------------------------
 *  general display functions
 *
 */
void        lcdInit(void);                                          // initialize LCD
int         lcdHeight(void);                                        // return display pixel height
int         lcdWidth(void);                                         // return display pixel width
void        lcdSetRotation(uint8_t);                                // screen rotation
void        lcdInvertDisplay(int i);                                // invert display
uint16_t    lcdColor565(uint8_t, uint8_t, uint8_t);                 // Pass 8-bit (each) R,G,B, get back 16-bit packed color

/*------------------------------------------------
 *  direct access display functions
 *
 */
void        lcdSetAddrWindow(uint8_t, uint8_t, uint8_t, uint8_t);   //
void        lcdPushColor(uint16_t);                                 // send color pixel to display
void        lcdFillScreen(uint16_t);                                // fill screen with a solid color
void        lcdDrawPixel(int, int, uint16_t);                       // draw a pixel
void        lcdDrawFastVLine(int, int, int, uint16_t);              // vertical line
void        lcdDrawFastHLine(int, int, int, uint16_t);              // horizontal line
void        lcdFillRect(int, int, int, int, uint16_t);              // color filled rectangle
void        lcdDrawChar(uint16_t, uint16_t, char, uint16_t, uint16_t, int); // write character to LCD

/*------------------------------------------------
 *  frame buffer display functions
 *  (using DMA)
 *
 */
uint8_t*    lcdFrameBufferInit(uint16_t);                           // initialize a frame buffer with a color
void        lcdFrameBufferFree(uint8_t*);                           // release memory reserved for the frame buffer
int         lcdFrameBufferPush(uint8_t*, void(*)(void));            // transfer frame buffer to LCD
void        lcdFrameBufferScroll(int, uint16_t);                    // scroll frame buffer by +/- pixels and fill new lines with color

#endif  /* end __st7735_h__ */

