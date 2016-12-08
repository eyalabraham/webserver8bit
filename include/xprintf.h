/*------------------------------------------------------------------------*/
/* Universal string handler for user console interface  (C)ChaN, 2011     */
/*------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------

  heavily modified to remove all the functions I do not need.
  kept on the core function xvprintf() with some modifications.

  format string follow this format per variable and the types listed below

  %[flags][width][length]specifier

  specifier	Output
  ---------	--------------------------------------
  d			Signed decimal integer
  u			Unsigned decimal integer
  o			Unsigned octal
  b			Unsigned binary
  x			Unsigned hexadecimal integer
  X			Unsigned hexadecimal integer (upper case)
  c			Character
  s			String of characters

  flags		description
  ---------	--------------------------------------
  -			Left-justify within the given field width
  0			Left-pads the number with zeroes (0) instead of spaces when padding is specified

  width
  ---------
  [number] 	representing the width of the field

  length
  ---------
  'l'		represent 'long int' 32-bit for 'x', 'X' or 'd' specifier

--------------------------------------------------------------------------*/

#ifndef	__XPRINTF_H__
#define	__XPRINTF_H__

void xfprintf(void (*func)(unsigned char), const char* fmt, ...);

#define	fprintf	xfprintf

#endif /* end ifndef __XPRINTF_H__ */
