/* ***************************************************************************

  STARTUP.C

  Startup code to initialize hardware.
  Executable should be included after boot in AUTOEXEC.BAT

  October 29 2017 - Created

*************************************************************************** */

#include    <stdlib.h>
#include    <stdio.h>
#include    <string.h>

#include    "v25.h"

#define     USAGE                   "startup -b [9600 | 19200 | 38400] [-l]\n"  \
                                    "   -b set ser0 baud rate\n"                \
                                    "   -l clear LCD (does nothing)\n"
#define     CON_COMM_CTRL_2         2           // base rate Fclk/512
#define     CON_COMM_BAUD_9600      130
#define     CON_COMM_BAUD_19200     65

#define     CON_COMM_CTRL_1         1
#define     CON_COMM_BAUD_38400     65

typedef enum
{
    BAUD_9600,
    BAUD_19200,
    BAUD_38400,
    NOT_DEFINED
} rate_t;

/* -----------------------------------------
   main module initialization code
----------------------------------------- */

/*------------------------------------------------
 * main()
 *
 *
 */
int main(int argc, char* argv[])
{
    int             i;
    rate_t          baudRateFlag = NOT_DEFINED;
    struct SFR     *pSfr;

    /* parse command line variables
     * for baud rate selection and LCD initialization
     */
    if ( argc == 1 )
    {
        printf("%s\n", USAGE);
        return -1;
    }

    for ( i = 1; i < argc; i++ )
    {
        if ( (strcmp(argv[i], "-b") == 0) &&
             ((i + 1) < argc) )
        {
            i++;
            if ( strcmp(argv[i], "9600") == 0 )
            {
                baudRateFlag = BAUD_9600;
            }
            else if ( strcmp(argv[i], "19200") == 0 )
            {
                baudRateFlag = BAUD_19200;
            }
            else if ( strcmp(argv[i], "38400") == 0 )
            {
                baudRateFlag = BAUD_38400;
            }
            else
            {
                baudRateFlag = NOT_DEFINED;
            }
        }
        else if ( strcmp(argv[i], "-l") == 0 )
        {
        }
        else
        {
            printf("%s\n", USAGE);
            return -1;
        }
    }

    /* setup ser0 baud rate
     */
    pSfr = MK_FP(0xf000, 0xff00);
    switch ( baudRateFlag )
    {
        case BAUD_9600:
            pSfr->scc0  = CON_COMM_CTRL_2;
            pSfr->brg0  = CON_COMM_BAUD_9600;
            printf("\033[2J\n\n@@@@\n\n");
            printf("BAUD Rate = 9600 BAUD.\n");
            break;

        case BAUD_19200:
            pSfr->scc0  = CON_COMM_CTRL_2;
            pSfr->brg0  = CON_COMM_BAUD_19200;
            printf("\033[2J\n\n@@@@\n\n");
            printf("BAUD Rate = 19200 BAUD.\n");
            break;

        case BAUD_38400:
            pSfr->scc0  = CON_COMM_CTRL_1;
            pSfr->brg0  = CON_COMM_BAUD_38400;
            printf("\033[2J\n\n@@@@\n\n");
            printf("BAUD Rate = 38400 BAUD.\n");
            break;

        default:
            printf("%s\n", USAGE);
            return -1;
    }

    /* build date
     */
    printf("build: startup.exe %s %s\n", __DATE__, __TIME__);

    return 0;
}
