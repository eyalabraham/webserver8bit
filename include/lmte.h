/* ***************************************************************************

  LMTE.H

  Large (model) Multi Task Executive (LMTE) header.

  Nov. 4, 2016  - Updated to large model
  March 16 1999 - Created

*************************************************************************** */

#ifndef __LMTE_H__
#define __LMTE_H__


#ifdef __V25__
  #pragma message "**** V25 object code! ****"
#endif

#include    "internal.h"                /* for basic types */

/* -----------------------------------------
   types
----------------------------------------- */

typedef enum tag_traceLevel
             {
              __TRC_LVL0__ = 0,          /* no trace information             */
              __TRC_LVL1__,              /* trace DB_BAD_xxx debug messages  */
              __TRC_LVL2__,              /* trace inter-task messages        */
              __TRC_LVL3__               /* trace DB_INFO mesages            */
             };

/* -----------------------------------------
   function prototypes
----------------------------------------- */

void
print(char*);                            /* print string to 'stdout'         */

void
sendn(void*,                             /* send string of bytes to 'stdout' */
      int);                              /* byte count                       */

char*                                    /* convert number to hex string     */
tohex(DWORD   dwNum,                     /* number to convert                */
      char*   szHex,                     /* pointer to string buffer         */
      int     nSize);                    /* string buffer size               */

void
setCSflag(void);

void
clearCSflag(void);

int
registerTask(void   (* func)(void),      /* pointer to task                  */
             WORD   wStackSize,          /* task stack size in WORD count
                                            '0' = default (512 words)        */
             WORD   wTicks,              /* task tick count
                                            '0' = default (5)                */
             int    nMsgQsize,           /* message Q size
                                            '0' = default (1 message)        */
             char*  szTaskName);         /* task name string                 */

void
startScheduler(enum tag_traceLevel traceLvl,   // trace sevices level
               WORD                wTimerFlag, // '1' to register timer sevices
               WORD                wPerTick);  // mili-sec per timer tick
                                               // '0' = DEF_MSEC_PER_TICK (5)

int
getTidByName(char* szTaskName);          /* pointer to task name             */

char*
getNameByTid(int     nTid,               /* task ID to search for            */
             char*   szTaskName,         /* task name return buffer          */
             int     nNameLen);          /* task name buffer size            */

void
suspend(WORD  wTime);                    /* suspened time in mili-seconds    */

int
release(int  nTid);                      /* task ID to release               */

int
putMsg(int   nTid,                       /* destination task                 */
       int   nPayload,                   /* message type identifier          */
       WORD  wPayload,                   /* word payload                     */
       DWORD dwPayload);                 /* double word payload              */

int                                      /* returns Q_EMPTY if no messages,
                                            or bPayload if got message       */
getMsg(int*    pnPayload,                /* message type                     */
       WORD*   pwPayload,                /* word payload                     */
       DWORD*  pdwPayload);              /* double word payload              */

int                                      /* returns Q_EMPTY if timed out,
                                            or bPayload if got message       */
waitMsg(int     nMsg,                    /* message to wait for or '__ANY__' */
        WORD    wTimeOut,                /* wait time out value, 0 = forever */
        WORD*   pwPayload,               /* word payload                     */
        DWORD*  pdwPayload);             /* double word payload              */

void                                     /* flush all pending messages       */
flushMsgQ(void);                         /* of calling task                  */

int
putDebugMsg(int    nPayload,             /* message type identifier          */
            WORD   wPayload,             /* word payload                     */
            DWORD  dwPayload);           /* double word payload              */

int                                      /* trace data                       */
putDataLog(WORD wTraceData);             /* byte/word                        */

void                                     /* dump trace data                  */
dumpDataLog(const int nGroupDelimiter);  /* data count row delimiter         */

DWORD
getGlobalTicks(void);                    /* get global ticks                 */

WORD
getMsecPerTick(void);                    /* get tick interval                */

/* -----------------------------------------
   global macro definitions
----------------------------------------- */

#define CRIT_SEC_START    setCSflag()    /* inhibit task switch on entrance  */
#define CRIT_SEC_END      clearCSflag()  /* enable task switch on exit       */

#define PACK_DW(DOUBLE, WORD1, WORD2)      \
           {                               \
            DOUBLE  = (DWORD) WORD1 << 16; \
            DOUBLE |= (WORD) WORD2;        \
           }

#define MSEC2TICKS(MSEC)  ((WORD) MSEC / getMsecPerTick())

/* -----------------------------------------
   global flag definitions
----------------------------------------- */

#define Q_EMPTY           0              /* message queue empty flag         */

#define __ME__            0              /* for suspend() API                */
#define __ANY__           0              /* for waitMsg()                    */

#define __TIMER_OFF__     0
#define __TIME_ON__       1

#ifndef TRUE
 #define TRUE             -1
 #define FALSE            0
#endif

/* -----------------------------------------
   debug message definitions
----------------------------------------- */

/* < bPayload >              */
#define    DB_TRACE          0xff

/* < wPayload >              */
#define    DB_INFO           0
#define    DB_BAD_TNAME      1
#define    DB_BAD_MSG        2
#define    DB_BAD_PARAM      3
#define    DB_USER           4

/* < dwPayload >            */
/*  error location
    -----------------------
    name = qualifier
    data = source line
    type = DWORD            */

#endif  /* __LMTE_H__ */
