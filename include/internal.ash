


; top of file "L:\WEBSERVE\SRC\INCLUDE\internal.ash"


__INTERNAL_H__      EQU     1h

MAX_TASKS_EVER      EQU     20

T_NAME_LENGTH       EQU     8

DEF_STACK_SIZE      EQU     512

DEF_TASK_TICKS      EQU     5

DEF_MSGQ_SIZE       EQU     1

DEF_MSEC_PER_TICK   EQU     5

TRACE_WINDOW        EQU     5

MAX_TRACE_BUFFER    EQU     100

ISR_FRAME           EQU     8

T_READY             EQU     0

T_SUSPENDED         EQU     1

Q_NOWAIT            EQU     0

Q_WAIT              EQU     <Q_NOWAIT + 1>

MILI_SEC            EQU     1190

TIMER_CTL_REG       EQU     043h

TIMER_COUNT         EQU     040h

TIMER_CTL_WORD      EQU     034h

INT_MASK_REG        EQU     021h

INT_CMD_REG         EQU     020h

TIMER_IRQ_MASK      EQU     0fch

IRQ_VECT            EQU     008h

BYTE                TYPEDEF BYTE 

WORD                TYPEDEF WORD 

DWORD               TYPEDEF DWORD 

taskControlBlock_tag          STRUC   
nTid                DW      ?
nState              DW      ?
wTicks              DW      ?
wTicksLeft          DW      ?
task                DD      FAR PTR ?
wStackSize          DW      ?
pStackBase          DD      FAR PTR ?
pTopOfStack         DD      FAR PTR ?
nQin                DW      ?
nQout               DW      ?
nQsize              DW      ?
pQbase              DD      FAR PTR ?
pNextCb             DD      FAR PTR ?
pPrevCb             DD      FAR PTR ?
dwTicksSuspend      DD      ?
nQusage             DW      ?
nWaitType           DW      ?
nMsgWait            DW      ?
szTaskName          DB      9 DUP ( ? )

taskControlBlock_tag          ENDS

tasckControlBlock   TYPEDEF taskControlBlock_tag

message_tag         STRUC   
nPayload            DW      ?
wPayload            DW      ?
dwPayload           DD      ?

message_tag         ENDS

message             TYPEDEF message_tag

traceRecord_tag     STRUC   
dwTimeStamp         DD      ?
nSourceTid          DW      ?
nDestTid            DW      ?
msgMessage          message_tag   <>

traceRecord_tag     ENDS

traceRecord         TYPEDEF traceRecord_tag

; end of file "L:\WEBSERVE\SRC\INCLUDE\internal.ash"

