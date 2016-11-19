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
            extrn      _pCbListTop    : word
            extrn      _nCritSecFlag  : word
;
            extrn      setCSflag_     : proc far
            extrn      clearCSflag_   : proc far
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
;nTid           equ      0
;nState         equ      2
;wTicks         equ      4
;wTicksLeft     equ      6
;task           equ      8
;wStackSize     equ      10
;pStackBase     equ      12
;pTopOfStack    equ      14
;pNextCb        equ      24
;dwTicksSuspend equ      28
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
; Serial 0 IO port definitions
;----------------------------------------
;
RXB0           equ      060h      ; receive buffer
TXB0           equ      062h      ; transmit buffer
SCS0           equ      06Bh      ; serial status reg.
;
;----------------------------------------
; timer-0 register definitions
;----------------------------------------
;
TMIC0          equ      09ch      ; timer interrupt control
mTMI0EN        equ      0bfh      ; enable AND mask
mTMI0DIS       equ      040h      ; disable OR mask
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
            .CODE
;            assume  cs:_TEXT,ds:DGROUP
;
;=====================================================
;  isr_()
;
;  Interrupt service routine.
;  Invoked every clock tick and reschedules
;  to a new ready (T_READY state) task, or resumes
;  the current task.
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
; save current stack pointer
;----------------------------------------
;
            mov        word ptr [si+pTopOfStack], sp
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
; a return address to the rescheduler
;----------------------------------------
;
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
; code segment
;----------------------------------------
;
            mov        ax, cs                             ; this is ok in 'SMALL' model
            push       ax
;
;----------------------------------------
; task entry address
;----------------------------------------
;
            mov        si, word ptr _pCurrentCb
;
            mov        ax, word ptr [si+task]
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
; create interrupt stack frame.
; At this point task has a short return
; frame back into the calling code.
; so,
; 1. leave room for an interrupt frame,
; 2. save all regs.
; 3. build the interupt frame and,
; 4. reschedule the task.
;----------------------------------------
;
            sub        sp,4
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
            add        bp, (INTR_FRAME+4)
;
            mov        dx, word ptr ss:[bp]               ; get short return add. out of the way
            pushf
            pop        ax
            or         ax, mFLAGS                         ; set flag IF=1
            mov        word ptr ss:[bp], ax               ; "push" flags.
;
            mov        ax, cs
            mov        word ptr ss:[bp-2], ax             ; "push" CS, this is ok in 'SMALL' model
;
            mov        word ptr ss:[bp-4], dx             ; "push" return address
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
;=====================================================
;  print_()
;
;  customized printing of ASCII string to 'stdout'.
;  string to be printed must be null terminated,
;  and passed as a pointer on the stack.
;=====================================================
;
            public     print_
;
print_      proc       far
;
            push       bp
            mov        bp,sp
;
            push       si
            push       bx
            push       es
;
            mov        bx, 0f000h                         ; setup pointer to V25 register/ports
            mov        es, bx
            mov        bx, 0ff00h
;
            call       setCSflag_                         ; enter critical section, to prevent task switch.
            mov        si, word ptr [bp+4]                ; get pointer to string

NXTC:
            mov        al, byte ptr [si]                  ; get character from string
            cmp        al, 0                              ; '0' char is end of string?
            je	       short EXIT                         ; exit if end of string.
;
            mov        byte ptr es:[bx+TXB0], al          ; send byte to TxR reg.
TX_NOT_RDY:
            mov        al, byte ptr es:[bx+SCS0]          ; read serial status reg.
            and        al, 020h                           ; wait for TxR reg. empty
            jz         short TX_NOT_RDY
;
            inc        si                                 ; next character.
            jmp        short NXTC
EXIT:
            call       clearCSflag_                       ; exit critical section.
;
            pop        es
            pop        bx
            pop        si
            pop        bp
;
            retf
;
print_      endp
;
;=====================================================
;  sendn_()
;
;  customized printing of ASCII string to 'stdout'.
;  use printn() to send 'n' bytes to serial console.
;=====================================================
;
            public     sendn
;
sendn_      proc       far
;
            push       bp
            mov        bp,sp
;
            push       si
            push       bx
            push       cx
            push       es
;
            mov        bx, 0f000h                         ; setup pointer to V25 register/ports
            mov        es, bx
            mov        bx, 0ff00h
;
            call       setCSflag_                         ; enter critical section, to prevent task switch
;
            mov        si, word ptr [bp+4]                ; get pointer to bytes
            mov        cx, word ptr [bp+6]                ; get byte count
            cmp        cx, 0                              ; did we pass a 0 count?
            je         EXIT_SENDN                         ; yes, then exit
;
NXT_BYTE:
            mov        al, byte ptr [si]                  ; get character from string
;
            mov        byte ptr es:[bx+TXB0], al          ; send byte to TxR reg.
WAIT_TX_RDY:
            mov        al, byte ptr es:[bx+SCS0]          ; read serial status reg.
            and        al, 020h                           ; wait for TxR reg. empty
            jz         short WAIT_TX_RDY
;
            inc        si                                 ; next character
            dec        cx
            jnz        short NXT_BYTE
EXIT_SENDN:
            call       clearCSflag_                       ; exit critical section.
;
            pop        es
            pop        cx
            pop        bx
            pop        si
            pop        bp
;
            retf
;
sendn_      endp
;
            end

