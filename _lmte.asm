;
;=====================================================
;  file: _lmte.asm
;
; Large (memory model) Multi Task Executive (SMTE) library source code.
;
; Nov. 4, 2016  - Updated to large model
; March 30 2012 - debuging... something strange is happening.
;                 it looks as if code/data alignment is an issue
; March 17 1999 - Created
;
;=====================================================
;
            .8086
            .model     large
;
;----------------------------------------
; external variables
;----------------------------------------
;
            extrn      _dwGlobalTicks : word
            extrn      _pCurrentCb    : word
            extrn      _nCritSecFlag  : word
;
;----------------------------------------
; offsets into CB structure
;----------------------------------------
;
taskControlBlock_tag          STRUC
nTid                DW      ?
nState              DW      ?
wTicks              DW      ?
wTicksLeft          DW      ?
task                DD      ?
wStackSize          DW      ?
pStackBase          DD      ?
pTopOfStack         DD      ?
nQin                DW      ?
nQout               DW      ?
nQsize              DW      ?
pQbase              DD      ?
pNextCb             DD      ?
pPrevCb             DD      ?
dwTicksSuspend      DD      ?
nQusage             DW      ?
nWaitType           DW      ?
nMsgWait            DW      ?
szTaskName          DB      9 DUP (?)
taskControlBlock_tag          ENDS
;
;----------------------------------------
; task state
;----------------------------------------
;
T_READY        equ      0
T_SUSPENDED    equ      1
;
;----------------------------------------
; stack frame size in bytes and flags
;----------------------------------------
;
INTR_FRAME     equ      16
mFLAGS         equ      8200h
;
;----------------------------------------
; End-Of-Interrupt macro
; singal interrupt controller of an EOI
;----------------------------------------
;
FINT        macro
            db         0fh
            db         092h
            endm
;
;
            .code
            assume      ds:DGROUP,ss:nothing,es:nothing
;
;=====================================================
;  isr_()
;
;  Interrupt service routine.
;  Invoked every clock tick and reschedules
;  to a new ready (T_READY state) task, or resumes
;  the current task.
;
;  this 'define' is located in 'internals.h':
;       #define ISR_FRAME         8       // 8 words: ax,si,bx,cx,dx,di,es,ds,bp
;
;  needs to match stack frame size and register push/pop order:
;       top (low memory)        --> bp
;                                   es
;                                   di
;                                   dx
;                                   cx
;                                   bx
;                                   si
;       bottom (high memory)    --> ax
;                                   ip      --+
;                                   cs        | interrupt iret or far return from call
;                                   flags   --+
;
;   ss:sp pairs are saved on the task control block and are 
;   swapped in and out betwen registers by sowtfware
;   whenever a task needs to be switched
;
;   assumes that ds is defined and is the data segment of the main
;   code, holding the global variable!!
;
;=====================================================
;
            public     isr_
;
isr_        proc       far
;
            push       ax
            push       si
;
;----------------------------------------
; increment global tick counter
;----------------------------------------
;
            mov        si, offset _dwGlobalTicks
            add        word ptr [si], 1
            adc        word ptr [si+2], 0
;
;            jmp        short NO_SWITCH                    ; ** for debug **
;
;----------------------------------------
; check for critical section, exit if true
;----------------------------------------
;
            cmp        word ptr _nCritSecFlag, 1          ; interrupted inside a critical section?
            je         short NO_SWITCH                    ; yes, no rask switch
;
;----------------------------------------
; is the current task out of time?
;----------------------------------------
;
            mov        si, word ptr _pCurrentCb           ; get current task
;
            cmp        word ptr [si+wTicksLeft], 0
            jne        short TICKS_LEFT
;
;----------------------------------------
; save machine state before swapping stack
;----------------------------------------
;
            push       bx                                 ; task switch.
            push       cx
            push       dx
            push       di
            push       es
            push       bp
;
;----------------------------------------
; save current stack pointer and segment
;----------------------------------------
;
            mov        word ptr [si+pTopOfStack], sp
            mov        ax, ss
            mov        word ptr [si+pTopOfStack+2], ax
;
;----------------------------------------
; restore task's tick count
;----------------------------------------
;
            mov        ax, word ptr [si+wTicks]
            mov        word ptr [si+wTicksLeft], ax
;
;----------------------------------------
; find next T_READY state task
; this assumes that there is always one task
; that is ready, this is the idle task.
;----------------------------------------
;
NXT_TASK:
            mov        ax, word ptr [si+pNextCb]          ; get next task CB
            mov        si, ax
            cmp        word ptr [si+nState], T_READY      ; is it ready?
            jne        short NXT_TASK                     ; no, go to next
;
            mov        word ptr _pCurrentCb, si           ; yes, swap to this task
;
;----------------------------------------
; swap to new stack
;----------------------------------------
;
            mov        sp, word ptr [si+pTopOfStack]
            mov        ax, word ptr [si+pTopOfStack+2]
            mov        ss, ax
;
;----------------------------------------
; restore new task's machine state
;----------------------------------------
;
            pop        bp
            pop        es
            pop        di
            pop        dx
            pop        cx
            pop        bx
;
;----------------------------------------
; decrement task's tick counter
;----------------------------------------
;
TICKS_LEFT:
            dec        word ptr [si+wTicksLeft]
;
;----------------------------------------
; signal EOI and return to task
;----------------------------------------
;
NO_SWITCH:
            pop        si
            pop        ax
;
            FINT
            iret
;
isr_        endp
;
;
;=====================================================
;  reschedule_()
;
;  Entered when a task is done (performes a 'ret').
;  The function re-establishes the a fake stack frame
;  (pushing its own address and a fake interrupt frame
;   containing the tasks own return address).
;  The function then moves to the next ready
;  (T_READY state) task and 'iret's to start it.
;=====================================================
;
            public     reschedule_
;
reschedule_ proc       far
;
            cli                                           ; disable interrupts
;
;----------------------------------------
; create a fake stack frame.
; At startup, task has a near return frame
; into reschedule(), and an interrupt frame
; to start itself with 'iret'.
;----------------------------------------
;
;----------------------------------------
; a far return address to the rescheduler
;----------------------------------------
;
            push       cs
            mov        ax, offset reschedule_
            push       ax
;
;----------------------------------------
; an interrupt frame for a scheduler startup
; flags with IF='1'.
;----------------------------------------
;
            pushf
            pop        ax
            or         ax, mFLAGS                         ; save the flags with IE=1 (intr. enabled)
            push       ax
;
;----------------------------------------
; far task entry address
;----------------------------------------
;
            mov        si, word ptr _pCurrentCb
;
            mov        ax, word ptr [si+task+2]           ; push task's code segment
            push       ax
            mov        ax, word ptr [si+task]             ; push task's offset
            push       ax
;
;----------------------------------------
; save machine state
;----------------------------------------
;
            push       ax
            push       si
            push       bx
            push       cx
            push       dx
            push       di
            push       es
            push       bp
;
RESCHEDULE:
;
;----------------------------------------
; save current stack pointer
;----------------------------------------
;
            mov        word ptr [si+pTopOfStack], sp
            mov        ax, ss
            mov        word ptr [si+pTopOfStack+2], ax
;
;----------------------------------------
; restore task's ticks value
;----------------------------------------
;
            mov        ax, word ptr [si+wTicks]
            mov        word ptr [si+wTicksLeft], ax
;
;----------------------------------------
; find a runable task
;----------------------------------------
;
FIND_RDY_TASK:
            mov        ax, word ptr [si+pNextCb]
            mov        si, ax
            cmp        word ptr [si+nState], T_READY
            jne        short FIND_RDY_TASK
;
            mov        word ptr _pCurrentCb, si
;
;----------------------------------------
; swap to next task's stack and
; get rid of fake 'push'ed regs
;----------------------------------------
;
            mov        sp, word ptr [si+pTopOfStack]
            mov        ax, word ptr [si+pTopOfStack+2]
            mov        ss, ax
;
;----------------------------------------
; restore new task's machine state
;----------------------------------------
;
            pop        bp
            pop        es
            pop        di
            pop        dx
            pop        cx
            pop        bx
            pop        si
            pop        ax
;
;----------------------------------------
; clear critical section flag to be safe
;----------------------------------------
;
            mov        word ptr _nCritSecFlag, 0
;
;----------------------------------------
; IRET to next task
;----------------------------------------
;
            sti                                           ; reenable interrupts
            iret
;
reschedule_ endp
;
;
;=====================================================
;  block()
;
;  The function swaps to the next T_READY task, after
;  building a fake interrupt frame for the suspended task.
;  The interrupt frame will return control to suspend().
;
;  NOTE:
;     This function is only called from suspend()
;=====================================================
;
            public     block_
;
block_      proc       far
;
            cli                                           ; disable interrupts
;
;----------------------------------------
; create interrupt stack frame out of a far call
; return address.
; At this point the task has a far return
; frame back into the calling code (large model)
; of a suspend() functon call (see below):
;  1. leave room to complete an interrupt frame,
;  2. save all regs.
;  3. build the interupt frame and,
;  4. reschedule the task.
;
;       before         after
;      --------       -------
;                [sp]--> bp
;                        es
;                        di
;                        dx
;                        cx
;                        bx
;                        si
;                        ax
;     ...........................
;                        ip
; [sp]--> ip             cs
;         cs             flags <-- [bp]
;
;----------------------------------------
;
            sub        sp,2
;
            push       ax
            push       si
            push       bx
            push       cx
            push       dx
            push       di
            push       es
            push       bp
;
;----------------------------------------
; rebuild the interrupt frame
;----------------------------------------
;
            mov        bp, sp
            add        bp, (INTR_FRAME+4)                 ; setup a pointed to top of stack frame
;
            mov        dx, word ptr ss:[bp-2]             ; move ip up one stack 'slot'
            mov        word ptr ss:[bp-4], dx
            mov        dx, word ptr ss:[bp]               ; move cs up one stack 'slot'
            mov        word ptr ss:[bp-2], dx
            pushf                                         ; get flags
            pop        ax
            or         ax, mFLAGS                         ; set flag IF=1
            mov        word ptr ss:[bp], ax               ; "push" flags to bottom stack 'slot' of frame
;
;----------------------------------------
; jump to relevant section of reschedule()
;----------------------------------------
;
            mov        si, word ptr _pCurrentCb
            jmp        short RESCHEDULE
;
block_      endp
;
            end

