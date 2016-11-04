/* ***************************************************************************

  LMTE.C

  Large (model) Multi Task Executive (LMTE) library source code.

  Nov. 4, 2016  - Updated to large model
  March 17 1999 - Created

*************************************************************************** */

#include <dos.h>
#include <stdio.h>
#include <string.h>
#include <alloc.h>

#include "lmte.h"
#include "internal.h"
#include "v25.h"

#pragma  intrinsic memcpy     /* compile memcpy() as inline */
#pragma  intrinsic strcmp     /* compile strcmp() as inline */

/* -----------------------------------------
   static function prototypes
----------------------------------------- */

char   nibtohex(BYTE);
struct taskControlBlock_tag* getCbByID(int);
void   smteUtil(void);
void   printSegments(void);

/* -----------------------------------------
   local definitions
----------------------------------------- */

#define  LOG_BUFFER_SIZE     500 // 5 data points, 5 times per sec for 20 sec

typedef  struct dataLog_tag
                {
                 DWORD  dwClockTicks;
                 WORD   wData;
                };


/* -----------------------------------------
   volatile globals
----------------------------------------- */

volatile DWORD                         dwGlobalTicks = 0L;
volatile struct taskControlBlock_tag*  pCurrentCb    = NULL;

/* -----------------------------------------
   globals
----------------------------------------- */

struct taskControlBlock_tag*  pCbListTop       = NULL;

int                           nCritSecFlag     = 0;
WORD                          wSaveStackPointer;
WORD                          wOldVectorSeg;
WORD                          wOldVectorOff;
WORD                          wTimerService    = 0;
int                           nTaskCount       = 0;
WORD                          wMiliSecPerTick  = DEF_MSEC_PER_TICK;

WORD                          wStackSeg;
WORD                          wExtraSeg;
WORD                          wDataSeg;
WORD                          wCodeSeg;

/* message trace buffer
*/
enum tag_traceLevel           traceLevel       = __TRC_LVL0__;
struct traceRecord_tag*       pTraceBuffer     = NULL;
WORD                          wTraceIn         = 0;
WORD                          wTraceOut        = 0;
WORD                          wTraceRecCount   = 0;

/* data trace buffer
*/
struct dataLog_tag            dataLogBuffer[LOG_BUFFER_SIZE];
int                           nDataLogCount    = 0;

/* ---------------------------------------------------------
   isr()

   Interrupt service routine.
   Invoked every clock tick and reschedules to a new
   ready (T_READY) task, or resumes the current task.
--------------------------------------------------------- */
extern void _far isr(void);

/* ---------------------------------------------------------
   reschedule()

   This function is reached when a task is done and
   performes a normal 'ret'.
   The function re-establishes the task's fake stack frame
   (pushing its own address and a fake interrupt frame
    with the tasks own return address).
   The function then moves to the next available task
   and 'iret's to start it.
--------------------------------------------------------- */
extern void reschedule(void);

/* ---------------------------------------------------------
   block()

   This function is called from suspend().
   The function swaps to the next T_READY task, after
   building a fake interrupt frame for the suspended task.
   The interrupt frame will return control to suspend().

   NOTE:
      This function only runs if suspend() is called
      with bTid == ME.
--------------------------------------------------------- */
extern void block(void);

/* ---------------------------------------------------------
   print()

   customized printing of ASCII string to 'stdout'.
   string to be printed must be null terminated.
--------------------------------------------------------- */
extern void print(char* szStr);

/* ---------------------------------------------------------
   setCSflag()

   This function sets the critical section flag.
   The flag should be set to start a critical code section,
   in which task switching is inhibited.
--------------------------------------------------------- */
void
setCSflag(void)
{
 nCritSecFlag = 1;
}

/* ---------------------------------------------------------
   clearCSflag()

   This function clears the critical section flag, and
   reenables task switching.
--------------------------------------------------------- */
void
clearCSflag(void)
{
 nCritSecFlag = 0;
}

/* ---------------------------------------------------------
   nibtohex()

   convert a nibble to single hex digit
--------------------------------------------------------- */
static char
nibtohex(BYTE  bNibble)
{
 return ( (bNibble > 9) ? (bNibble - 0x0a + 'a') : (bNibble + '0'));
}

/* ---------------------------------------------------------
   tosz()

   convert to hex.
   return pointer to string, or NULL if error
--------------------------------------------------------- */
char*
tohex(DWORD   dwNum,
      char*   szHex,
      int     nSize)
{
 register int   i;

 if ( nSize > 8 )
    return NULL;

 szHex[nSize] = '\0';

 for ( i = nSize ; i > 0; i--)
    {
     szHex[i - 1] = nibtohex((BYTE) dwNum & 0x0f);
     dwNum >>= 4;
    }

 return szHex;
}

/* ---------------------------------------------------------
   idle()

   idle task.
   SMTE 'house keeping' and idle wait, never suspended.
--------------------------------------------------------- */
static void
smteUtil(void)
{
 register struct taskControlBlock_tag* pCb;
 register struct traceRecord_tag*      pTraceRecord;
 char                                  szHex[10];

 //print("smteUtil\r\n");

 // find suspended tasks and process suspend time
 pCb = pCbListTop;
 do
 {
    if ( pCb->nState == T_SUSPENDED )
    {
       if ( (pCb->dwTicksSuspend != 0) && (dwGlobalTicks >= pCb->dwTicksSuspend) )
          pCb->nState = T_READY;
    }
    pCb = pCb->pNextCb;
 }
 while ( pCb != pCbListTop );

 // trace messages from buffer
 while ( (pTraceBuffer != NULL) && (wTraceRecCount > 0) )
    {
     pTraceRecord = pTraceBuffer + wTraceOut;

     print("TR ");

     tohex(pTraceRecord->dwTimeStamp, szHex, 8);
     print(szHex);
     print(" ");

     tohex(pTraceRecord->nSourceTid, szHex, 2);
     print(szHex);
     print(" ");

     tohex(pTraceRecord->nDestTid, szHex, 2);
     print(szHex);
     print(" ");

     tohex(pTraceRecord->msgMessage.nPayload, szHex, 2);
     print(szHex);
     print(" ");

     tohex(pTraceRecord->msgMessage.wPayload, szHex, 4);
     print(szHex);
     print(" ");

     tohex(pTraceRecord->msgMessage.dwPayload, szHex, 8);
     print(szHex);
     print("\r\n");

     wTraceOut++;
     if ( wTraceOut >= MAX_TRACE_BUFFER )
        wTraceOut = 0;

     wTraceRecCount--;
    }
}

/* ---------------------------------------------------------
   timer()

   timer service task.
--------------------------------------------------------- */
static void
timer(void)
{
}

/* ---------------------------------------------------------
   setisrvect()

   This function will set interrupt vector nVectId,
   to point to function fpIsrAdd.
   the original code for setvect() enables interrupts
   within the int 21H call.
   setisrvect() will do the same without re-enabling
   interrupts.
--------------------------------------------------------- */
static void
setisrvect(int            nVectId,
           void (_far* fpIsrAdd) () )
         /*  void interrupt fpIsrAdd () ) */
{
 WORD _far* wpVector;

 wpVector      = MK_FP(0, (nVectId * 4));

 wOldVectorOff = *wpVector;
 *wpVector++   = FP_OFF(fpIsrAdd);

 wOldVectorSeg = *wpVector;
 *wpVector     = FP_SEG(fpIsrAdd);
}

/* ---------------------------------------------------------
   setupTimer()

   This function sets up the timer and inrerrupt vectors.
   The timer is set to interrupt every 'wMiliSecInterval'
   miliseconds.
--------------------------------------------------------- */
static void
setupTimer(WORD wMiliSecInterval)
{
 WORD              wCount;

 #ifdef __V25__
 struct SFR _far*  pSfr;
 #endif

 /* -----------------------------------------
    setup clock timer channel 0
 ----------------------------------------- */

 asm { cli }

 /* -----------------------------------------
    setup interrupt vector
 ----------------------------------------- */

 setisrvect(IRQ_VECT, isr);
 
 /* -----------------------------------------
    setup interrupt controler and timer
 ----------------------------------------- */

 wCount = ( (wMiliSecInterval > 50) ? 50 : wMiliSecInterval ) * MILI_SEC;

 #ifdef __V25__

 pSfr = MK_FP(0xf000, 0xff00);

 pSfr->tmc0   = (TIMER_CTL_WORD & 0x7f); /* interval timer, now stopped */
 pSfr->md0    = wCount;
 pSfr->tmic0 &= TIMER_INT_MASK;          /* timer0/int0 vector int.     */
 pSfr->tmc0   = TIMER_CTL_WORD;          /* start timer                 */

 #else

 outp(TIMER_CTL_REG, TIMER_CTL_WORD);
 outp(TIMER_COUNT, LO(wCount));
 outp(TIMER_COUNT, HI(wCount));

 outp(INT_MASK_REG, TIMER_IRQ_MASK);

 #endif
}

/* ---------------------------------------------------------
   escape()

   rerurn control to calling control program, effectivly
   exiting the scheduler.
   The function performs task clean up and returns from
   after the 'iret' point in startScheduler().

   NOTE: The function assumes that no malloc()'s where done
         inside a task, these will NOT be free()'d!
---------------------------------------------------------
static void
escape(void)
{
 asm { cli }

 stop timer

 #ifdef __V25__

 #else

 #endif

 restore original timer interrupt handler

 setisrvect(IRQ_VECT, (void (_far*) ()) MK_FP(wOldVectorSeg, wOldVectorOff));

 restore original stack pointer

 _SP = wSaveStackPointer;

 for every task: remove queue, local stack, CB

 exit code

 asm { sti             re-enable interrupts, and...
       pop si          restore si register
       mov sp,bp       get rid of 'szHex'
       pop bp    };    fake startScheduler()' exit code
}
*/

/* ---------------------------------------------------------
   registerTask()

   This API registers a new task into the task list,
   initializing the list as required.
   The function returns a valid task ID,
   or '0' for failure
--------------------------------------------------------- */
int
registerTask(void   (* func)(void),    /* pointer to task               */
             WORD   wStackSize,        /* task stack size '0' = default */
             WORD   wTicks,            /* task tick count '0' = default */
             int    nMsgQsize,         /* message Q size  '0' = default */
             char*  szTaskName)        /* task name string              */
{
 static struct taskControlBlock_tag* pCbList = NULL;

 struct taskControlBlock_tag*  pNewCbNode;
 WORD*                         pNewStack;
 struct message_tag*           pNewMsgQ;
 WORD                          wStack;
 int                           nQsize;

 if ( nTaskCount == MAX_TASKS_EVER )
    return 0;

 /* -----------------------------------------
    allocate task's CB on near heap
 ----------------------------------------- */

 pNewCbNode = malloc(sizeof(struct taskControlBlock_tag));
 if ( pNewCbNode == NULL )
    {
     printf("ERR: registerTask() - CB malloc() for %s\n", szTaskName);
     return 0;
    }

 memset((void*) pNewCbNode, 0, sizeof(struct taskControlBlock_tag));

 strncpy(pNewCbNode->szTaskName, szTaskName, T_NAME_LENGTH);
 pNewCbNode->nState     = T_READY;
 pNewCbNode->wTicks     = wTicks ? wTicks : DEF_TASK_TICKS;
 pNewCbNode->wTicksLeft = pNewCbNode->wTicks;
 pNewCbNode->task       = func;
 pNewCbNode->pNextCb    = pNewCbNode;
 pNewCbNode->pPrevCb    = pNewCbNode;
 pNewCbNode->nWaitType  = Q_NOWAIT;
 pNewCbNode->nMsgWait   = __ANY__;

 printf("INFO: registerTask() - task entry point 0x%x\n", (WORD) func);

 /* -----------------------------------------
    allocate task's message Q on near heap
 ----------------------------------------- */

 nQsize = nMsgQsize ? nMsgQsize : DEF_MSGQ_SIZE;

 pNewMsgQ = malloc(nQsize * sizeof(struct message_tag));
 if ( pNewMsgQ == NULL )
    {
     printf("ERR: registerTask() - message Q malloc() for %s\n", szTaskName);
     free(pNewCbNode);
     return 0;
    }

 memset(pNewMsgQ, 0, (nQsize * sizeof(struct message_tag)));
 pNewCbNode->nQin    = 0;
 pNewCbNode->nQout   = 0;
 pNewCbNode->nQsize  = nQsize;
 pNewCbNode->nQusage = 0;
 pNewCbNode->pQbase  = pNewMsgQ;

 printf("INFO: registerTask() - msg queue base 0x%x\n", (WORD) pNewCbNode->pQbase);

 /* -----------------------------------------
    allocate task's stack on near heap
    NOTE:
          stack size is in WORDs
 ----------------------------------------- */

 wStack = wStackSize ? wStackSize : DEF_STACK_SIZE;
 pNewStack = malloc(2 * wStack);
 if ( pNewStack == NULL )
    {
     printf("ERR: registerTask() - stack malloc() for %s\n", szTaskName);
     free(pNewMsgQ);
     free(pNewCbNode);
     return 0;
    }

 memset(pNewStack, 0, (2 * wStack));
 pNewCbNode->wStackSize  = 2 * wStack;
 pNewCbNode->pStackBase  = pNewStack;
 pNewCbNode->pTopOfStack = pNewStack + wStack - 1;

 printf("INFO: registerTask() - stack base:0x%x top:0x%x\n", (WORD) pNewCbNode->pStackBase, (WORD) pNewCbNode->pTopOfStack);

 /* -----------------------------------------
    create a fake stack frame
    at startup task has an interrupt frame
    to start itself upon 'iret', and a
    short return frame into reschedule()
 ----------------------------------------- */

 /* a return address to the rescheduler        */

 *(--pNewCbNode->pTopOfStack) = (WORD) reschedule;

 /* an interrupt frame for a scheduler startup */

 *(--pNewCbNode->pTopOfStack) = 0xf202;       /* setup fake flags w/ IF true */

 *(--pNewCbNode->pTopOfStack) = _CS;          /* get code segment            */

 *(--pNewCbNode->pTopOfStack) = (WORD) pNewCbNode->task; /* get task address */

 pNewCbNode->pTopOfStack -= ISR_FRAME;       /* fake 'push' of ax,si,bx,cx,
                                                dx,di,es,bp                  */
 /* -----------------------------------------
    chain task CB into CB list
 ----------------------------------------- */

 if ( pCbList == NULL )
    {
     pCbList          = pNewCbNode;
     pCbListTop       = pNewCbNode;
     pNewCbNode->nTid = 1;
    }
 else
    {
     // chain last node to be top's previous
     pCbListTop->pPrevCb = pNewCbNode;

     // chain current node's next to new node
     pCbList->pNextCb    = pNewCbNode;

     // set new node's next and previous
     pNewCbNode->pNextCb = pCbListTop;
     pNewCbNode->pPrevCb = pCbList;

     // set task ID
     pNewCbNode->nTid    = pCbList->nTid + 1;

     // adjust current node
     pCbList             = pNewCbNode;
    }

 printf("INFO: registerTask() - name = %s tid = %02x\n", pNewCbNode->szTaskName, pNewCbNode->nTid);

 printf("INFO: registerTask() - task cb 0x%x\n", (WORD) pNewCbNode);

 nTaskCount++;

 return (pNewCbNode->nTid);
}

/* ---------------------------------------------------------
   startScheduler()

   This function starts the scheduler.
   The function will return only if no tasks are present
   in the task list.
   If at least one task is resent in the task list the function
   will setup the schduling timer and start task scheduling.

   This function never ruturns.
--------------------------------------------------------- */
void
startScheduler(enum tag_traceLevel traceLvl,
               WORD wTimerFlag,
               WORD wPerTick)
{
 asm { cli }

 if ( nTaskCount == 0 )
    {
     printf("INFO: startScheduler() - task queue empty\n");
     return;
    }

 traceLevel      = traceLvl;
 wTimerService   = wTimerFlag;
 wMiliSecPerTick = wPerTick ? wPerTick : DEF_MSEC_PER_TICK;

 /* -----------------------------------------
    setup trace buffer and debug trace task
 ----------------------------------------- */

 if ( traceLevel > __TRC_LVL0__ )
    {
     /* allocate trace buffer */
     pTraceBuffer = malloc(MAX_TRACE_BUFFER * sizeof(struct traceRecord_tag));
     if ( pTraceBuffer == NULL )
        {
         printf("ERR: startScheduler() - trace buffer malloc()\n");
         return;
        }

     memset((void*) pTraceBuffer, 0, MAX_TRACE_BUFFER * sizeof(struct traceRecord_tag));
    }

 printf("INFO: startScheduler() - DB_LEVEL(%d)\n", traceLevel);

 if ( registerTask(smteUtil, 64, (WORD) ((TRACE_WINDOW / wMiliSecPerTick) + 1), 0, "smteUtl") == 0 )
 {
    printf("ERR: startScheduler() - smteUtil task registration failed\n");
    return;
 }

 /* -----------------------------------------
    register timer task
 ----------------------------------------- */

 if ( wTimerService )
    if ( !registerTask(timer, 0, 0, 0, "timer") )
       {
        printf("ERR: startScheduler() - timer task registration failed\n");
        return;
       }

 printf("INFO: startScheduler() - TIMER(%d)\n", wTimerService);

 /* -----------------------------------------
    print segments register values
 ----------------------------------------- */

 wStackSeg = _SS;
 wExtraSeg = _ES;
 wDataSeg  = _DS;
 wCodeSeg  = _CS;

 printf("INFO: startScheduler() - CS=0x%04x DS=0x%04x\n", wCodeSeg, wDataSeg);
 printf("INFO: startScheduler() - SS=0x%04x ES=0x%04x\n", wStackSeg, wExtraSeg);

 /* -----------------------------------------
    start executive
 ----------------------------------------- */

 printf("INFO: startScheduler() - mSec/tick = %02x\n", wMiliSecPerTick);

 pCurrentCb = pCbListTop;              /* point to first task           */

 wSaveStackPointer = _SP;              /* save original stack pointer   */
 pCurrentCb->pTopOfStack += ISR_FRAME; /* get rid of fake 'push'ed regs */

 setupTimer(wMiliSecPerTick);          /* setup timer interrupt         */

 _SP = (WORD) pCurrentCb->pTopOfStack; /* swap to task's stack          */

 #ifdef __V25__
 asm {
      sti
      iret                             // a 'fake' iret into task
     }
 #else
 asm {
      out 020H, 020H
      sti                              /* re-enable interrupts          */
      iret                             /* fake an interrupt return      */
     }
 #endif
}

/* ---------------------------------------------------------
   getTidByName()

   This function returns a task id that matches the
   input task name 'szTaskName'.

   Function returns the task id, or 0'' if name match
   failed.
--------------------------------------------------------- */
int
getTidByName(char* szTaskName)     /* pointer to task name          */
{
 struct taskControlBlock_tag* pCb;

 pCb  = pCbListTop;

 do
    {
     if ( !strcmp(szTaskName, pCb->szTaskName) )
        return pCb->nTid;

     pCb = pCb->pNextCb;
    }
 while (pCb != pCbListTop);

 return 0;
}

/* ---------------------------------------------------------
   getNameByTid()

   This function returns a task name that matches the
   input task id 'bTid'.
   Task name is returned in 'szTaskName', that has space
   allocated for a maximum of 'nNameLen' characters.

   Function returns pointer to 'szTaskName', or NULL if
   failed.
--------------------------------------------------------- */
char*
getNameByTid(int    nTid,           /* task ID to search for         */
             char*  szTaskName,     /* task name return buffer       */
             int    nNameLen)       /* task name buffer size         */
{
 struct taskControlBlock_tag*  pCb;

 pCb = getCbByID(nTid);

 if ( pCb != NULL )
 {
    szTaskName[0] = '\0';
    strncat(szTaskName, pCb->szTaskName, nNameLen - 1);
    return szTaskName;
 }

 return NULL;
}

/* ---------------------------------------------------------
   suspend()

   This function suspends calling task for the duration of
   'wTime' mili-seconds.
   Effectively setting its state to T_SUSPENDED for a
   calculated amount of ticks, based on 'wTime'.

   NOTE:
   to force an indefinite suspention use 'wTime' = 0
--------------------------------------------------------- */
void
suspend(WORD  wTime)                     /* suspened time in mili-seconds */
{
 DWORD  dwTickCount = 0;

 if ( wTime > 0 )
    {
     // calculate tick count during which the task will remain suspended
     dwTickCount = wTime / wMiliSecPerTick;

     // make sure integer division does not end up as an infinite suspend
     if ( dwTickCount == 0 )
        dwTickCount = 1L;

     // the future tick count that will release the task
     dwTickCount += dwGlobalTicks;
    }

 // update task CB
 CRIT_SEC_START;
 pCurrentCb->nState         = T_SUSPENDED;
 pCurrentCb->dwTicksSuspend = dwTickCount;
 CRIT_SEC_END;

 block();

 return;
}

/* ---------------------------------------------------------
   release()

   This function unconditionally releases task 'bTid'.
   Effectively setting its state to T_READY.
   The released task will return to the runable task pool
   on the closest switch to it.

   Function returns the released task's ID, or '-1' if
   failed.
--------------------------------------------------------- */
int
release(int nTid)                       /* task ID to release            */
{
 struct taskControlBlock_tag*  pCb;
 int                           nReturn = -1;

 if ( nTid == __ME__ ) /* can't release '__ME__' */
    return -1;

 pCb = getCbByID(nTid);

 if ( pCb != NULL && pCb->nState == T_SUSPENDED )
 {
    CRIT_SEC_START;
    pCb->nState = T_READY;
    CRIT_SEC_END;
    nReturn = nTid;
 }

 return nReturn;
}

/* ---------------------------------------------------------
   putMsg()

   This function sends a message to the destination
   task ID 'bTid'.

   Function returns the destination task's ID, or '0' if
   failed.
--------------------------------------------------------- */
int
putMsg(int   nTid,
       int   nPayload,
       WORD  wPayload,
       DWORD dwPayload)
{
 struct traceRecord_tag        trcMessage;
 struct taskControlBlock_tag*  pCb;
 int                           nReturn    = 0;
 int                           nPutTheMsg = 1;

 /* build message block */

 trcMessage.msgMessage.nPayload   = nPayload;
 trcMessage.msgMessage.wPayload   = wPayload;
 trcMessage.msgMessage.dwPayload  = dwPayload;

 /* move the message to the input msg buffer of the destination task */

 pCb = getCbByID(nTid);

 if ( pCb != NULL )
 {
    /* is destination task suspended and waiting for message?
       do not put message if it is not expected by dest. task       */
    if ( (pCb->nState == T_SUSPENDED) && (pCb->nWaitType == Q_WAIT) )
    {
       if ( (pCb->nMsgWait == __ANY__)  || (pCb->nMsgWait == nPayload) )
       {
          /* release waiting task */
          pCb->nState         = T_READY;
          pCb->dwTicksSuspend = 0;
          nPutTheMsg = 1;
       }
       else
       {
          nPutTheMsg = 0;
       }
    } /* end if dest. task suspended and waiting for msg. */

    /* is message queue not overrun? */
    if ( nPutTheMsg && (pCb->nQusage < pCb->nQsize) )
    {
       CRIT_SEC_START;

       /* copy message to task's message Q*/
       memcpy((void*) (pCb->pQbase + pCb->nQin),
              (void*) &(trcMessage.msgMessage),
               sizeof(struct message_tag));

       pCb->nQin++;

       if ( pCb->nQin >= pCb->nQsize )
          pCb->nQin = 0;

       pCb->nQusage++;

       /* copy message to trace buffer */
       if ( traceLevel > __TRC_LVL1__ )
       {
          trcMessage.dwTimeStamp = dwGlobalTicks;
          trcMessage.nSourceTid  = pCurrentCb->nTid;
          trcMessage.nDestTid    = nTid;
          memcpy((void*) (pTraceBuffer + wTraceIn),
                 (void*) &trcMessage,
                 sizeof(struct traceRecord_tag));

          wTraceIn++;

          if ( wTraceIn >= MAX_TRACE_BUFFER )
             wTraceIn = 0;

          wTraceRecCount++;
       }  /* end trace on */

       CRIT_SEC_END;

       nReturn = nTid;

    } /* end put message if not overrun */
 }

 return nReturn;
}

/* ---------------------------------------------------------
   getMsg()

   This function returns a message from the task's msg
   queue.

   Function returns Q_EMPTY if failed, bPayload if not.
--------------------------------------------------------- */
int
getMsg(int*    pnPayload,
       WORD*   pwPayload,
       DWORD*  pdwPayload)
{
 struct message_tag*  pMsg;

 if ( pCurrentCb->nQusage == 0 )
    {
     *pnPayload = Q_EMPTY;
     return Q_EMPTY;
    }

 pMsg = pCurrentCb->pQbase + pCurrentCb->nQout;

 /* get msaage info */
 *pnPayload  = pMsg->nPayload;
 *pwPayload  = pMsg->wPayload;
 *pdwPayload = pMsg->dwPayload;

 /* advance 'out' index */
 pCurrentCb->nQout++;

 if ( pCurrentCb->nQout >= pCurrentCb->nQsize )
    pCurrentCb->nQout = 0;

 /* decrement queue contents count */
 pCurrentCb->nQusage--;

 return *pnPayload;
}

/* ---------------------------------------------------------
   waitMsg()

   This function blocks until a message 'bMsg' has been received
   or time-out reached.
   if 'bMsg' is __ANY__ the function will return the first message trapped.
   upon return, 'pwPayload' and 'pdwPayload' contain message
   payloads and function returns 'bMsg'.

   block forever with 'dwTimeOut' = 0

   Function returns Q_EMPTY if function timed out before
   message has been received, or bMsg received if not failed and
   not timed out.
--------------------------------------------------------- */
int                                      /* returns Q_EMPTY if timed out     */
waitMsg(int     nMsg,                    /* message type to wait for         */
        WORD    wTimeOut,                /* wait time out value              */
        WORD*   pwPayload,               /* word payload                     */
        DWORD*  pdwPayload)              /* double word payload              */
{
 int    nPayload;

 /* scan queue for existing messages */
 while ( (nPayload = getMsg(&nPayload, pwPayload, pdwPayload)) != Q_EMPTY )
 {
     /* is massage matching __ANY__ or specific request?                  */
     if ( (nMsg == __ANY__) || (nMsg == nPayload) )
     {
         return nPayload; /* yes, return it                               */
     }                    /* no, keep scanning until queue is empty       */
 }

 pCurrentCb->nWaitType = Q_WAIT;    /* queue is empty so suspend and wait */
 pCurrentCb->nMsgWait = nMsg;

 suspend(wTimeOut);

 pCurrentCb->nWaitType = Q_NOWAIT;
 pCurrentCb->nMsgWait  = __ANY__;

 return ( getMsg(&nPayload, pwPayload, pdwPayload) );
}

/* ---------------------------------------------------------
   flushMsgQ()

   This function will flush all messges from the calling
   task's message queue.
--------------------------------------------------------- */
void
flushMsgQ(void)
{
 CRIT_SEC_START;

 pCurrentCb->nQout   = 0;
 pCurrentCb->nQin    = 0;
 pCurrentCb->nQusage = 0;;

 CRIT_SEC_END;
}

/* ---------------------------------------------------------
   putDebugMsg()

   This function sends a message to the trace buffer
   for debug purposes.

   Function returns '0', or '-1' if failed.
--------------------------------------------------------- */
int
putDebugMsg(int    nPayload,
            WORD   wPayload,
            DWORD  dwPayload)
{
 struct traceRecord_tag trcMessage;

 /* check debug level */

 if ( traceLevel == __TRC_LVL0__ )
    return -1;

 if ( (traceLevel < __TRC_LVL3__) &&
      (nPayload   == DB_TRACE) &&
      (wPayload   == DB_INFO) )
    return -1;

 CRIT_SEC_START;

 /* build trace message record */
 trcMessage.dwTimeStamp          = dwGlobalTicks;
 trcMessage.nSourceTid           = pCurrentCb->nTid;
 trcMessage.nDestTid             = 0xdb;             /* debug */
 trcMessage.msgMessage.nPayload  = nPayload;
 trcMessage.msgMessage.wPayload  = wPayload;
 trcMessage.msgMessage.dwPayload = dwPayload;

 memcpy((void*) (pTraceBuffer + wTraceIn), (void*) &trcMessage, sizeof(struct traceRecord_tag));
 wTraceIn++;
 if ( wTraceIn >= MAX_TRACE_BUFFER )
    wTraceIn = 0;
 wTraceRecCount++;

 CRIT_SEC_END;

 return 0;
}

/* ---------------------------------------------------------
   putDataLog()

   function stores trace data into a buffer as data
   pairs <clock ticks> <word data>.
   returns -1 when buffer is full.
   data can be dumped with call dumpDataLog() from any task.
--------------------------------------------------------- */
int
putDataLog(WORD wData)
{
 if (nDataLogCount == LOG_BUFFER_SIZE)
    {
     return -1;
    }

 CRIT_SEC_START;

 dataLogBuffer[nDataLogCount].dwClockTicks = getGlobalTicks();
 dataLogBuffer[nDataLogCount].wData        = wData;
 nDataLogCount++;

 CRIT_SEC_END;

 return 0;
}

/* ---------------------------------------------------------
   dumpDataLog()

   dump content of trace buffer and reset.
   data is printed to stdout in hex format:
   <clock ticks>:<word data>
   nGroupDelimiter pairs per output line with each pair
   delimited by "-" character
--------------------------------------------------------- */
void
dumpDataLog(int nGroupDelimiter)
{
 char  szHex[10];
 int   i;
 
 if ( nGroupDelimiter == 0 )
    {
     nDataLogCount = 0;
     print("<dump-reset>\r\n");
     return;
    }
    
 print("<dump>\r\n");

 for (i = 0; i < nDataLogCount; i++)
    {
     tohex(dataLogBuffer[i].dwClockTicks, szHex, 8);
     print(szHex);

     print(":");

     tohex(dataLogBuffer[i].wData, szHex, 4);
     print(szHex);

     if ((i % nGroupDelimiter) == 0)
        {
         print("\r\n");
        }
     else
        {
         print("-");
        }
    } // next trace buffer element

  nDataLogCount = 0; // reset trace buffer

  print("</dump>\r\n");
}

/* ---------------------------------------------------------
   getGlobalTicks()

   This function returns the 'dwGlobalTicks' count.
--------------------------------------------------------- */
DWORD
getGlobalTicks(void)
{
 return dwGlobalTicks;
}

/* ---------------------------------------------------------
   getMsecPerTick()

   This function returns the 'MSEC_PER_TICK' value.
--------------------------------------------------------- */
WORD
getMsecPerTick(void)
{
 return wMiliSecPerTick;
}

/* ---------------------------------------------------------
   getCbByID()

   Get pointer to task CB by task ID.
   Retun NULL if not found.
--------------------------------------------------------- */
static struct taskControlBlock_tag*
getCbByID(int nTid)
{
 struct taskControlBlock_tag*  pCb;
 int    nFound = 0;

 pCb = pCbListTop;

 // walk linked list to find task ID
 do
 {
  if ( pCb->nTid == nTid )
     nFound = 1;
  else
     pCb = pCb->pNextCb;
 }
 while ( pCb != pCbListTop && nFound == 0 );

 // return pointer to CB or NULL
 if ( nFound == 1 )
    return pCb;
 else
    return NULL;
}

