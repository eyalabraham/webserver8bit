# Embedded 8-bit CPU Web Server (and other projects)

I started this project with the goal of building and programming
an 8bit CPU to be a simple web server. The original idea was to build the
server around my old NEC v25 single board computer (8086 compatible).
To accomplish this I had to add an Ethernet interface (although I could have
easily use SLIP with the extra serial connection I had available), which in turn
required an SPI serial IO interface. The description of the hardware design
is available here [https://sites.google.com/site/eyalabraham/8bit-cpu-tcp-ip-stack-and-web-server]
This is where the original project started to diverge.

I added a general IO interface using a 8255 parallel IO device, and an AVR
ATmega328P that provided a parallel (from the 8255) to SPI conversion.
These two devices, with some firmware on the AVR, allows a clean and relatively
fast CPU to SPI interface for the Ethernet ENC28J60 device I selected.
The setup provided support for both IN and OUT CPU IO commands, and the ability to
use DMA for bulk transfers.
Now that I had an SPI interface, I through in an LCD display for good measure.

Drivers for the hardware components are included in this repository.
The low level driver for parallel to SPI (*ppispi.c*) is not portable, but I am
sure that the display driver, as well as the Ethernet device driver, can be reused
by replacing the IO calls to *ppispi.c* with your own.

Once I had my hardware platform to working reliably, with simple IO and DMA, I could
start my software projects:

## TCP/IP stack and HTTP server program:
This projects started with LwIP, which I quickly abandoned in favor of writing my
own simple TCP/IP stack. I maintain the stack code here [https://github.com/eyalabraham/8bit-TCPIP].
The HTTP server program is part of this repository as a single module named *httpd.c*
When *httpd.c* is compiled with the TCP/IP stack and the hardware drivers it is a functional
HTTP server.

## LCD driver and VT100 emulator:
The intent was to enable use of the LCD display with full range of cursor and color control
using the VT100 command set. VT100 defines a set of Escape Sequences that are commands embedded
in the character stream being printed.
More on VT100 commands can be found here [http://www.termsys.demon.co.uk/vtansi.htm].
Included in files: *vt100lcd.c* and *vt100lcd.h*. Auxiliary files used as LCD driver
are *st7735.c* and *st7735.h* (based on the Adafruit drivers), and *xprintf.c* and *xprintf.h* are used
as a specialized printf() with character streaming.
The last two files are not needed, and can be replaced with your own xfprintf() driver.

## Adapting my Multi Tasking Executive to an 8086 Large memory model:
I first used the executive in a robotics project a few years ago
[https://sites.google.com/site/eyalabraham/robotics#software]
I originally thought that I would reuse the Executive in this project in order to
create a set of tasks for a server that would have a server task, a display task, a TCP/IP task, and
finally, a simple user console task.
I may still do this, but I prefer to move on to a new project ;-)
My first step of converting the Small Model frame work to a Large Model is done and available here
with *lmte.c* and *_lmte.asm*, and associated header files.
A working console example is included with *ws.c*, *t_term.c*, and *t_dummy.c*.

Other files are included in this project including some Python scripts I used for 
testing the TCP/IP stack.
The repository includes test drivers I used to test the various drivers.
These include: *vt100tst.c*, *ethtest.c*, and *spitest.c*

Final note. All executables are built to compile with the Watcom tool-chain. Both 'c' and
Assembly code alike. Most of the modules and drivers are portable, and should work on other
architectures (I tried to maintain a clean variable type style), but I did not test that here.
All Make files are included as well.
