/*------------------------------------------------------------------------/
/  Universal string handler for user console interface
/-------------------------------------------------------------------------/
/
/  Copyright (C) 2011, ChaN, all right reserved.
/
/ * This software is a free software and there is NO WARRANTY.
/ * No restriction on use. You can use, modify and redistribute it for
/   personal, non-profit or commercial products UNDER YOUR RESPONSIBILITY.
/ * Redistributions of source code must retain the above copyright notice.
/
/-------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------

  heavily modified to remove all the functions I do not need.
  kept on the core function xvprintf() with some modifications.

--------------------------------------------------------------------------*/

#include	<stdarg.h>

#include	"xprintf.h"

#define xputc(c)  xfunc_out((unsigned char)c)           // macro for readability

/*------------------------------------------------

  xvprintf()

  a printf() like formatted output function.
  accepts a format string and argument list,
  and outputs the result through a hooked user provided
  character stream output function
  
------------------------------------------------*/

/*----------------------------------------------*/
/* Formatted string output                      */
/*----------------------------------------------*/
/*  xprintf("%d", 1234);			"1234"
    xprintf("%6d,%3d%%", -200, 5);	"  -200,  5%"
    xprintf("%-6u", 100);			"100   "
    xprintf("%ld", 12345678L);		"12345678"
    xprintf("%04x", 0xA3);			"00a3"
    xprintf("%08LX", 0x123ABC);		"00123ABC"
    xprintf("%016b", 0x550F);		"0101010100001111"
    xprintf("%s", "String");		"String"
    xprintf("%-4s", "abc");			"abc "
    xprintf("%4s", "abc");			" abc"
    xprintf("%c", 'a');				"a"
    xprintf("%f", 10.0);            <xprintf lacks floating point support>
*/

static void xvprintf(void(*xfunc_out)(unsigned char),   // pointer to character stream handler function
                     const char* fmt,                   // Pointer to the format string
                     va_list arp)                       // Pointer to arguments
{
	unsigned int r, i, j, w, f;
	unsigned long v;
	char s[16], c, d, *p, *tmp;

	for (;;) {
		c = *fmt++;					                    // Get a char
		if (!c) break;				                    // End of format?
		if (c != '%') {				                    // Pass through it if not a % sequence
			xputc(c); continue;
		}
		f = 0;
		c = *fmt++;					                    // Get first char of the sequence
		if (c == '0') {				                    // Flag: '0' padded
			f = 1; c = *fmt++;
		} else {
			if (c == '-') {			                    // Flag: left justified
				f = 2; c = *fmt++;
			}
		}
		for (w = 0; c >= '0' && c <= '9'; c = *fmt++)	// Minimum width
			w = w * 10 + c - '0';
		if (c == 'l' || c == 'L') {	                    // Prefix: Size is long int
			f |= 4; c = *fmt++;
		}
		if (!c) break;				                    // End of format?
		d = c;
		if (d >= 'a') d -= 0x20;
		switch (d) {				                    // Type is...
		case 'S' :					                    // String
			p = va_arg(arp, char*);
			for (j = 0; p[j]; j++) ;
			while (!(f & 2) && j++ < w) xputc(' ');
			tmp = p;
			while (*tmp) xputc(*tmp++);
			while (j++ < w) xputc(' ');
			continue;
		case 'C' :					                    // Character
			xputc((char)va_arg(arp, int)); continue;
		case 'B' :					                    // Binary
			r = 2; break;
		case 'O' :					                    // Octal
			r = 8; break;
		case 'D' :					                    // Signed decimal
		case 'U' :					                    // Unsigned decimal
			r = 10; break;
		case 'X' :					                    // Hexadecimal
			r = 16; break;
		default:					                    // Unknown type (pass through)
			xputc(c); continue;
		}

		// Get an argument and put it in numeral
        //
		v = (f & 4) ? va_arg(arp, long) : ((d == 'D') ? (long)va_arg(arp, int) : (long)va_arg(arp, unsigned int));
		if (d == 'D' && (v & 0x80000000)) {
			v = 0 - v;
			f |= 8;
		}
		i = 0;
		do {
			d = (char)(v % r); v /= r;
			if (d > 9) d += (c == 'x') ? 0x27 : 0x07;
			s[i++] = d + '0';
		} while (v && i < sizeof(s));
		if (f & 8) s[i++] = '-';
		j = i; d = (f & 1) ? '0' : ' ';
		while (!(f & 2) && j++ < w) xputc(d);
		do xputc(s[--i]); while(i);
		while (j++ < w) xputc(' ');
	}
}

/*------------------------------------------------
 
  xfprintf()

  a 'driver' for a printf() like formatted output function.
  accepts a format string and optional arguments,
  and outputs the result through a hooked user provided
  character stream output function

------------------------------------------------*/
void xfprintf(void(*func)(unsigned char),               // Pointer to the output function
              const char* fmt, ...)                     // Pointer to the format string and optional arguments
{
	va_list arp;

	va_start(arp, fmt);
	xvprintf(func,fmt, arp);
	va_end(arp);
}
