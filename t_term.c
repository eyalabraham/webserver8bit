/* ***************************************************************************

  T_TERM.C

  Terminal Task source code.

  Nov 26, 2016 - Created

*************************************************************************** */

#include	"lmte.h"
#include	"names.h"
#include	"messages.h"
#include	"t_term.h"

/* -----------------------------------------
   task function code
----------------------------------------- */

void t_term(void)
{
	int		nMsg;
	WORD	wPayload;
	DWORD	dwPayload;

	int		dummyTask;

	// get id of 'dummy' task
	dummyTask = getTidByName(TASK_NAME_DUMMY);
	if ( dummyTask == 0 )
	{
		// if, for some reason the task was not found, exit with a debug error post
		putDebugMsg(DB_TRACE, DB_BAD_TNAME, 0L);
		return;
	}

	// enter an endless loop of ping and wait
	while (1)
    {
		// send ping to 'dummy' task
		putMsg(dummyTask, ANY_PING, W_DONT_CARE, DW_DONT_CARE);

		// wait for message
		wPayload = (WORD) myTid();
		nMsg = waitMsg(__ANY__, 50, &wPayload, &dwPayload);

		// parse message
		switch ( nMsg )
        {
		 case Q_EMPTY:
			  putDebugMsg(DB_TRACE, DB_TIME_OUT, 0L);
			  break;
         case ANY_PING_RESP:
              break;
         default:
              putDebugMsg(DB_TRACE, DB_BAD_MSG, 0L);
        } // switch on bMsg

		suspend(1000);

    } // endless loop
}
