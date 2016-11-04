/* ***************************************************************************

  INTERNAL.H

  Large (model) Multi Task Executive (LMTE) internal header.

  Nov. 4, 2016  - Updated to large model
  July   1 2010 - reduced time to print debug info to 10msec from 100
  March 16 1999 - Created

*************************************************************************** */

#ifndef __INTERNAL_H__
#define __INTERNAL_H__

/* -----------------------------------------
   global macros
----------------------------------------- */

#define MAX_TASKS_EVER    20      // maximum number of tasks
#define T_NAME_LENGTH     8       // task name identifier length
#define DEF_STACK_SIZE    512     // default stack size in words
#define DEF_TASK_TICKS    5       // default task window
#define DEF_MSGQ_SIZE     1       // default queue size in mssages
#define DEF_MSEC_PER_TICK 5       // mili-seconds per tick resolution

#define TRACE_WINDOW      5       // mili-sec allocated to print trace info
#define MAX_TRACE_BUFFER  100     // max trace records

#define ISR_FRAME         8       /* 8 words: ax,si,bx,cx,dx,di,es,bp */

#define HI(word)          (BYTE) ((word & 0xff00) >> 8)
#define LO(word)          (BYTE) (word & 0x00ff)

#define T_READY           0       /* task ready to run                */
#define T_SUSPENDED       1       /* suspend task on next scheduling  */

#define Q_NOWAIT          0       // no-wait message, task suspended if 'dwTicksSuspend' != 0
#define Q_WAIT            Q_NOWAIT + 1 // task suspended and waiting for message. see 'nMsgWait'

#ifdef __V25__

/* -----------------------------------------
   NEC V25 SBC
----------------------------------------- */

#define MILI_SEC          78      /* timer setup for 1ms count
                                     on NEC SBC running 10Mhz         */
#define TIMER_CTL_WORD    0xc0    /* timer0 interval mode             */
#define TIMER_INT_MASK    0x07

#define IRQ_VECT          28      /* timer0 interrupt vector          */

#else

/* -----------------------------------------
   PC
----------------------------------------- */

#define MILI_SEC          1190    /* timer setup for 1ms count
                                     actually 1190.4762 for 1.19Mhz   */
#define TIMER_CTL_REG     0x43
#define TIMER_COUNT       0x40
#define TIMER_CTL_WORD    0x34    /* clock0, LSB & MSB R/W, mode 2    */

#define INT_MASK_REG      0x21    /* interrupt controler addresses    */
#define INT_CMD_REG       0x20
#define TIMER_IRQ_MASK    0xfc

#define IRQ_VECT          0x08    /* clock 0 timer interrupt vector   */

#endif

/* -----------------------------------------
   internal types and enumerations
----------------------------------------- */

typedef struct taskControlBlock_tag
               {
                /* task information */                             //  offset
                int                 nTid;                          //  0
                int                 nState;                        //  2
                WORD                wTicks;                        //  4
                WORD                wTicksLeft;                    //  6
                void                (* task)(void);                //  8

                /* task's stack context */
                WORD                wStackSize;                    //  10
                WORD*               pStackBase;                    //  12
                WORD*               pTopOfStack;                   //  14

                /* task message queue */
                int                  nQin;                         //  16
                int                  nQout;                        //  18
                int                  nQsize;                       //  20
                struct message_tag*  pQbase;                       //  22

                /* form a double linked list */
                struct taskControlBlock_tag*  pNextCb;             //  24
                struct taskControlBlock_tag*  pPrevCb;             //  26

                /* suspention loop count     */
                DWORD                dwTicksSuspend;               //  28

                /* message Q usage in message count */
                int                  nQusage;                      //  32

                /* message to wait                  */
                int                  nWaitType;                    //  34
                int                  nMsgWait;                     //  36

                /* task name                        */
                char                szTaskName[T_NAME_LENGTH + 1]; //  38
               } tasckControlBlock;

typedef struct message_tag
               {
                int         nPayload;
                WORD        wPayload;
                DWORD       dwPayload;
               } message;

/* -----------------------------------------
   reserved debug messages

   NOTE: task ID is set to 0xdb for debug
         trace messages
----------------------------------------- */

typedef struct traceRecord_tag
               {
                DWORD               dwTimeStamp;
                int                 nSourceTid;   /* 0x00 = kernel
                                                     0xdb = debug           */
                int                 nDestTid;
                struct message_tag  msgMessage;
               } traceRecord;

#endif /* __INTERNAL_H__ */
