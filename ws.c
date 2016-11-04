/* ***************************************************************************

  WS.C

  8-bit web server main module
  initializes hardware, registers tasks and starts web server
  running tasks under LMTE:
  -

  created:  4 Nov. 2016

*************************************************************************** */

#include    <dos.h>
#include    <stdio.h>
#include    <stdlib.h>
#include    <string.h>
#include    <assert.h>

#include    "lmte.h"
#include    "names.h"

//#include    "t_term.h"

/* -----------------------------------------
   globals
----------------------------------------- */


/* -----------------------------------------
   startup code
----------------------------------------- */

int main(int argc, char* argv[])
{
    int  i;

    // parse command line parameters:
    // ws [-v]
    //         '-v' print build and verion information
    for ( i = 1; i < argc; i++ )
    {
        if ( strcmp(argv[i], "-v") == 0 )
        {
            printf("BUILD: ws.exe %s %s\n", __DATE__, __TIME__);
            return 0;
        }
        else
        {
            printf("main(), bad command line option '%s'\n", argv[i]);
            return 1;
        }
    }

    // build date
    printf("BUILD: ws.exe %s %s\n", __DATE__, __TIME__);

    return 0;
}
