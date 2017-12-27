/*
 *  vt100tst.c
 *
 *      test program VT100 API for ST7735 LCD
 *
 */

#include    <stdlib.h>
#include    <stdio.h>
#include    <sys/types.h>

#include    "vt100lcd.h"
#include	"xprintf.h"

/*------------------------------------------------
 * main()
 *
 */
int main(int argc, char* argv[])
{
    uint8_t     i;
    int         fg, bg;
    
    printf("Build: vt100tst.exe %s %s\n", __DATE__, __TIME__);

    // initialize with default background and foreground
    vt100_lcd_init(VT100_LANDSCAPE, ST7735_BLACK, ST7735_GREEN);

    // clear the screen
    xfprintf(vt100_lcd_putc, "\033[2J");

    // print a graphic charter border
    xfprintf(vt100_lcd_putc, "\033[37;40m");
    for ( i = 1; i < vt100_lcd_columns()-1; i++ )
    {
        xfprintf(vt100_lcd_putc, "\033[0;%02df\315", i);
        xfprintf(vt100_lcd_putc, "\033[%02d;%02df\315", vt100_lcd_rows()-1, i);
    }

    for ( i = 1; i < vt100_lcd_rows()-1; i++ )
    {
        xfprintf(vt100_lcd_putc, "\033[%02d;0f\272", i);
        xfprintf(vt100_lcd_putc, "\033[%02d;%02df\272", i, vt100_lcd_columns()-1);
    }
    xfprintf(vt100_lcd_putc, "\033[0;0f\311");
    xfprintf(vt100_lcd_putc, "\033[0;%02df\273", vt100_lcd_columns()-1);
    xfprintf(vt100_lcd_putc, "\033[%02d;0f\310", vt100_lcd_rows()-1);
    xfprintf(vt100_lcd_putc, "\033[%02d;%02df\274", vt100_lcd_rows()-1, vt100_lcd_columns()-1);

    // print greeting
    xfprintf(vt100_lcd_putc, "\033[33;40m");
    xfprintf(vt100_lcd_putc, "\033[2;2fVT100 Emulation v1.0\r\n");
    xfprintf(vt100_lcd_putc, "\033[32;40m");
    xfprintf(vt100_lcd_putc, "\033[3;2f%s, %s", __DATE__, __TIME__);

    // print a 7 x 7 matrix of all foreground/background
    // color combinations
    xfprintf(vt100_lcd_putc, "\033[5;5f");
    for ( fg = 30; fg <= 37; fg++ )
    {
        for ( bg = 40; bg <= 47; bg++ )
        {
            xfprintf(vt100_lcd_putc, "\033[%02d;%02dm\*", fg, bg);
        }
        xfprintf(vt100_lcd_putc, "\033[B");
        xfprintf(vt100_lcd_putc, "\033[8D");
    }

    return 0;
}
