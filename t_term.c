/* ***************************************************************************

  T_TERM.C

  Terminal Task source code.

  Nov 26, 2016 - Created

*************************************************************************** */

#include	<stdlib.h>
#include	<string.h>
#include	<i86.h>

#include	"v25.h"
#include	"lmte.h"
#include	"names.h"
#include	"messages.h"
#include	"t_term.h"

#pragma		intrinsic ( memset )		// compile memset() as inline

/* -----------------------------------------
   global definitions
----------------------------------------- */

#define		BANNER		"\033[2J"								\
						"8-bit web server\r\n"					\
						"Eyal Abraham, December 2016\r\n"		\
						"(type 'help' for command list)\r\n"	\
						"\r\n"

#define     SER1MODE    0xc9            // 8 bit, 1 start, 1 stop, no parity
#define     SER1CTRL    0x03            // set to Fclk/16 scaler
#define     SER1BAUD    65              // BAUD rate divisor
#define		SER1_IRQ	17				// serial 1 IRQ vector number
#define		SER1RXINT	0x07			// serial 1 Rx interrupt control

#define		SER_BUFF	16				// serial-1 isr receive buffer
#define		CLI_BUFFER	32				// command line text buffer
#define		CR			0x0d			// carriage return
#define		LF			0x0a			// line feed
#define		BELL		0x07			// bell
#define		BS			0x08			// back space
#define		SPACE		0x20			// space
#define		PROMPT		0x3e			// a '>' character
#define		MAX_TOKENS	3				// max number of command line tokens
#define		CMD_DELIM	" \t"			// command line white-space delimiters

#define		NAME_LEN	8				// task name length

#define     SYNTAX_ERR	"syntax error.\r\n"
#define     HELP_TEXT	"task list    - list task names and IDs\r\n"	\
						"help         - help text\r\n"

/* -----------------------------------------
   globals
----------------------------------------- */

BYTE			serBuffer[SER_BUFF];	// serial receive buffer
volatile int	nSerIn;					// input and output buffer pointers and byte count
volatile int	nSerOut;
volatile int	nSerCount;

struct SFR*		pSfr;

/* -----------------------------------------
   static function prototypes
----------------------------------------- */

static int process_cli(char *commandLine);
static void	conout(char*);
static void uart_putchr(BYTE);
static BYTE uart_getchr(void);
static int  uart_ischar(void);
static void _interrupt ser1isr(void);

/* -----------------------------------------
   task function code
----------------------------------------- */
void t_term(void)
{
	int			nMsg;					// message variables
	WORD		wPayload;
	DWORD		dwPayload;

	BYTE		inChar;					// console input character
	int			nCliIndex;
	char		commandLine[CLI_BUFFER] = {0};

	int			dummyTask;				// t_dummy() ID for 'ping' tests
	WORD*		wpVector;				// temp pointer to link interrupt vector

    pSfr = MK_FP(0xf000, 0xff00);       // define SFR

	// terminal initialization on FlashLite V25 board
	// serial-1 interface

	setCSflag();						// enter critical section

    pSfr->scm1 = SER1MODE;              // serial mode register
    pSfr->scc1 = SER1CTRL;              // serial control register
    pSfr->brg1 = SER1BAUD;              // serial baud rate register

    // setup serial interrupt for receive
    nSerIn = 0;							// set up circular receive buffer
    nSerOut = 0;
    nSerCount = 0;
    nCliIndex = 0;

    wpVector      = MK_FP(0, (SER1_IRQ * 4));	// set up interrupt vector
    *wpVector++   = FP_OFF(ser1isr);
    *wpVector     = FP_SEG(ser1isr);

    pSfr->sric1 = SER1RXINT;			// enable serial 1 Rx interrupt

    conout(BANNER);

    print("t_term(): initialized serial-1\r\n");

    clearCSflag();

	// get id of 'dummy' task
	dummyTask = getTidByName(TASK_NAME_DUMMY);
	if ( dummyTask == 0 )
	{
		// if, for some reason the task was not found, exit with a debug error post
		putDebugMsg(DB_TRACE, DB_BAD_TNAME, 0L);
		return;
	}

	uart_putchr(PROMPT);

	// enter an endless loop of ping and wait
	while (1)
    {
        // sample UART input and act on command line
        if ( uart_ischar() )
        {
            inChar = uart_getchr();
            switch ( inChar )
            {
                case CR:
                    uart_putchr(inChar);                        // output a CR-LF
                    uart_putchr(LF);
                    if ( process_cli(commandLine) == -1 )       // -- process command --
                    	conout(SYNTAX_ERR);
                    memset(commandLine, 0, CLI_BUFFER);         // reinitialize command line buffer for next command
                    nCliIndex = 0;
                    uart_putchr(PROMPT);						// output a prompt if required
                    break;

                default:
                    if ( nCliIndex < CLI_BUFFER )
                    {
                        if ( inChar != BS )                     // is character a back-space?
                            commandLine[nCliIndex++] = inChar;  // no, then store in command line buffer
                        else if ( nCliIndex > 0 )               // yes, it is a back-space, but do we have characters to remove?
                        {
                            nCliIndex--;                        // yes, remove the character
                            commandLine[nCliIndex] = 0;
                            uart_putchr(BS);
                            uart_putchr(SPACE);
                        }
                        else
                            inChar = 0;                         // no, so do nothing
                    }
                    else
                        inChar = BELL;

                    uart_putchr(inChar);                        // echo character to console
            }
        } // UART character input
    } // endless loop
}

/* -----------------------------------------
   process_cli()

----------------------------------------- */
static int process_cli(char *commandLine)
{
    char*	tokens[MAX_TOKENS] = {0, 0, 0};
    char*	token;
    char*	tempCli;
    int		numTokens;

    char	pString[NAME_LEN+1] = {0};
    int		i;

    // separate command line into tokens
    tempCli = commandLine;
    for ( numTokens = 0; numTokens < MAX_TOKENS; numTokens++, tempCli = NULL)
    {
        token = strtok(tempCli, CMD_DELIM);
        if ( token == NULL )
            break;
        tokens[numTokens] = token;
    }

    // if nothing found then this is an empty line, just exit
    if ( numTokens == 0 )
        return 0;

    // parse and execute command
    if ( strcmp(tokens[0], "set") == 0 && numTokens == 3 )
    {
    }
    else if ( strcmp(tokens[0], "get") == 0 && numTokens == 2 )
    {
    }
    else if ( strcmp(tokens[0], "task") == 0 && numTokens > 1 )
    {
    	if (strcmp(tokens[1], "list") == 0 )
    	{
    		i = 1;
    		while ( getNameByTid(i, pString, NAME_LEN) != NULL )
    		{
    			conout(" ");
    			conout(pString);
    			conout(" ");
    			tohex((WORD) i, pString, 2);
    			conout(pString);
    			conout("\r\n");
    			i++;
    		}
    	}
    	else
    		return -1;
    }
    else if ( strcmp(tokens[0], "help") == 0 )
    {
        conout(HELP_TEXT);
    }
    else
        return -1;

    return 0;
}

/* -----------------------------------------
   conout()

   print ASCII string to serial-1 console
   string to be printed must be null terminated.

----------------------------------------- */
static void conout(char* szStr)
{
	int	i = 0;

	while ( szStr[i] != 0 )
	{
		uart_putchr(szStr[i]);
		i++;
	}
}

/* -----------------------------------------
   uart_putchr()

   print ASCII string to serial-1 console
   string to be printed must be null terminated.

----------------------------------------- */
static void uart_putchr(BYTE outChar)
{
	while ( (pSfr->scs1 & 0x20) == 0 ) {};	// wait for Tx buffer to be empty
	pSfr->txb1 = outChar;					// send out a character
}

/* -----------------------------------------
   uart_getchr()

   print ASCII string to serial-1 console
   string to be printed must be null terminated.

----------------------------------------- */
static BYTE uart_getchr(void)
{
	int	byte = 0;

	if ( nSerCount > 0 )
	{
		byte = (int) serBuffer[nSerOut];

		nSerOut++;
		if ( nSerOut == SER_BUFF )
			nSerOut = 0;

		nSerCount--;
	}

	return (BYTE) byte;
}

/* -----------------------------------------
   uart_ischar()

   return number of characters in serial
   receive buffer.

----------------------------------------- */
static int uart_ischar(void)
{
	return nSerCount;
}

/* -----------------------------------------
   ser1isr()

   serial interface #1 interrupt handler

----------------------------------------- */
static void _interrupt ser1isr(void)
{
	BYTE	byte;

	byte = pSfr->rxb1;						// read the received byte to clear the buffer

	if ( nSerCount < SER_BUFF )
	{
		serBuffer[nSerIn] = byte;			// if buffer has free space, store byte in buffer

		nSerIn++;
		if ( nSerIn == SER_BUFF )			// update circular buffer indexes
			nSerIn = 0;
		nSerCount++;
	}

	__asm { db  0x0f						// end of interrupt epilog for NEC V25
	        db  0x92
	      }
}
