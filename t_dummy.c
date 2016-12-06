/* ***************************************************************************

  T_DUMMY.C

  Task source code.
  This task only responds to 'ping' messages fr testing

  Nov 27 2016 - Created

*************************************************************************** */

#include	"lmte.h"
#include	"messages.h"
#include	"t_dummy.h"

/* -----------------------------------------
   task function code
----------------------------------------- */

void t_dummy(void)
{
	int		nMsg;
	WORD	wPayload;
	DWORD	dwPayload;

	print("t_dummy(): initialized\r\n");

	while (1)
    {
		// wait for message
		nMsg = waitMsg(__ANY__, 0, &wPayload, &dwPayload);

		// parse message
		switch ( nMsg )
        {
         case ANY_PING:
        	  putMsg((int) wPayload, ANY_PING_RESP, W_DONT_CARE, DW_DONT_CARE);
              break;
         default:
              putDebugMsg(DB_TRACE, DB_BAD_MSG, (DWORD) __LINE__);
		} // switch on bMsg
	} // endless loop
}
