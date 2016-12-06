/* ***************************************************************************

  WS.C

  8-bit web server main module
  initializes hardware, registers tasks and starts web server

  ws [-v] [-d <debug level: 0..3>]

  running tasks under LMTE:
  -

  created:  4 Nov. 2016

*************************************************************************** */

//#include    <dos.h>
#include    <stdio.h>
#include    <stdlib.h>
#include    <string.h>
#include    <assert.h>

#include    "lmte.h"
#include    "names.h"

#include    "t_term.h"
#include	"t_dummy.h"

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
    // ws [-v] [-d <debug level>]
    //         '-v' print build and version information
    //         '-d' debug level 0..3
    for ( i = 1; i < argc; i++ )
    {
        if ( strcmp(argv[i], "-v") == 0 )
        {
            printf("Build: ws.exe %s %s\n", __DATE__, __TIME__);
            return 0;
        }
        else
        {
            printf("main(), bad command line option '%s'\n", argv[i]);
            return 1;
        }
    }

    // build date
    printf("Build: ws.exe %s %s\n", __DATE__, __TIME__);

    /* tasks:           task        stack ticks    msg  name                  */
    assert(registerTask(t_term,      128,   10,     1,  TASK_NAME_TERM));
    assert(registerTask(t_dummy,      64,   10,     1,  TASK_NAME_DUMMY));

    // ***  add 5 mili sec for lmte utility task  ***

    // start scheduler
    startScheduler(__TRC_LVL3__, __TIMER_OFF__, 1);

    return 0;
}
