;/*
; * FreeRTOS Kernel V11.0.1
; * Copyright (C) 2021 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
; *
; * SPDX-License-Identifier: MIT
; *
; * Permission is hereby granted, free of charge, to any person obtaining a copy of
; * this software and associated documentation files (the "Software"), to deal in
; * the Software without restriction, including without limitation the rights to
; * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
; * the Software, and to permit persons to whom the Software is furnished to do so,
; * subject to the following conditions:
; *
; * The above copyright notice and this permission notice shall be included in all
; * copies or substantial portions of the Software.
; *
; * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
; * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
; * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
; * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
; * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
; * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
; *
; * https://www.FreeRTOS.org
; * https://github.com/FreeRTOS
; *
; */

;------------------------------------------------------------------------------
; Extern symbols
;------------------------------------------------------------------------------
.extern _uxInterruptNesting
.extern _uxPortMaxInterruptDepth
.extern _xPortScheduleStatus
.extern _vTaskSwitchContext
.extern _pvPortGetCurrentTCB
.extern _common_interrupt_handler

.public _vIrq_Handler
.public _vPortStartFirstTask
.public _vPortYield
.public _vTRAP0_Handler
;------------------------------------------------------------------------------
; Macro definitions
;------------------------------------------------------------------------------
EIPC  .set 0
EIPSW .set 1
PSW   .set 5
FPSR  .set 6
FPEPC .set 7
EIIC  .set 13
CTPC  .set 16
CTPSW .set 17
EIIC_MSK .set 0x00000FFF
FPU_MSK  .set 0x00010000
;------------------------------------------------------------------------------
; portSAVE_CONTEXT
; Context saving
;------------------------------------------------------------------------------
portSAVE_CONTEXT .macro
    prepare lp, 0

    ; Save general-purpose registers and EIPSW, EIPC, EIIC, CTPSW, CTPC into stack.
    pushsp  r5, r30
    $nowarning
    pushsp  r1, r2
    $warning

    stsr    EIPSW, r15
    stsr    EIPC, r16
    stsr    EIIC, r17
    stsr    CTPSW, r18
    stsr    CTPC, r19
    pushsp  r15, r19

    ; Save FPU registers to stack if FPU is enabled
    mov     FPU_MSK, r19
    tst     r15, r19
    bz      12                           ; Jump over next 3 instructions: stsr (4 bytes)*2 + pushsp (4 bytes)
    stsr    FPSR, r18
    stsr    FPEPC, r19
    pushsp  r18, r19

	jarl    _pvPortGetCurrentTCB, lp    ; Get current TCB, the return value is stored in r10 (CCRH compiler)
    st.w    sp, 0[r10]                   ; Save the stack pointer in the TCB

.endm

;------------------------------------------------------------------------------
; portRESTORE_CONTEXT
; Context restoring
;------------------------------------------------------------------------------
portRESTORE_CONTEXT .macro
	jarl    _pvPortGetCurrentTCB, lp   ; Get current TCB, the return value is stored in r10 (CCRH compiler)
    ld.w    0[r10], sp                  ; Restore the stack pointer from the TCB

    ; Restore FPU registers if FPU is enabled
    mov     FPU_MSK, r19
    stsr    PSW, r18
    tst     r18, r19
    bz      12                          ; Jump over next 3 instructions: stsr (4 bytes)*2 + popsp (4 bytes)
    popsp   r18, r19
    ldsr    r18, FPEPC
    ldsr    r19, FPSR

    ;Restore general-purpose registers and EIPSW, EIPC, EIIC, CTPSW, CTPC
    popsp   r15, r19
    ldsr    r19, CTPC
    ldsr    r18, CTPSW
    ldsr    r17, EIIC
    ldsr    r16, EIPC
    ldsr    r15, EIPSW

    $nowarning
    popsp   r1, r2
    $warning
    popsp   r5, r30

    dispose 0, lp
.endm

;------------------------------------------------------------------------------
; Save used registers
;------------------------------------------------------------------------------
SAVE_REGISTER .macro
    ; Save general-purpose registers and EIPSW, EIPC, EIIC, CTPSW, CTPC into stack.
    ; Callee-Save registers (r20 to r30) are not used in interrupt handler and guaranteed no change after function call
    ; So, don't need to save register to optimize the used stack memory.
    pushsp  r5, r19
    $nowarning
    pushsp  r1, r2
    $warning

    stsr    EIPSW, r19
    stsr    EIPC, r18
    stsr    EIIC, r17
    mov     lp, r16
    mov     ep, r15
    stsr    CTPSW, r14
    stsr    CTPC, r13
    pushsp  r13, r19

    mov     FPU_MSK, r16
    tst     r16, r19
    bz      12
    stsr    FPSR, r18
    stsr    FPEPC, r19
    pushsp  r18, r19

.endm
;------------------------------------------------------------------------------
; Restore used registers
;------------------------------------------------------------------------------
RESTORE_REGISTER .macro

    mov     FPU_MSK, r16
    stsr    PSW, r18
    tst     r18, r19
    bz      12
    popsp   r18, r19
    ldsr    r18, FPEPC
    ldsr    r19, FPSR

    popsp   r13, r19
    ldsr    r13, CTPC
    ldsr    r14, CTPSW
    mov     r15, ep
    mov     r16, lp
    ldsr    r17, EIIC
    ldsr    r18, EIPC
    ldsr    r19, EIPSW

    $nowarning
    popsp   r1, r2
    $warning
    popsp   r5, r19
.endm

;------------------------------------------------------------------------------
; Start the first task.
;------------------------------------------------------------------------------
_vPortStartFirstTask:
    portRESTORE_CONTEXT
    eiret

;------------------------------------------------------------------------------
; _vPortYield
;------------------------------------------------------------------------------
_vPortYield:
    trap    0
    jmp     [lp]                        ; Return to caller function

;------------------------------------------------------------------------------
; PortYield handler. This is installed as the TRAP exception handler.
;------------------------------------------------------------------------------
_vTRAP0_Handler:
    ;Save the context of the current task.
    portSAVE_CONTEXT

    ; The use case that portYield() is called from interrupt context as nested interrupt.
    ; Context switch should be executed at the most outer of interrupt tree. In that case, just set xPortScheduleStatus
    ; to flag context switch in interrupt handler.
    stsr    0, r10, 2
    shr     16, r10
    add     -1, r10
    shl     2, r10
    mov     #_uxInterruptNesting, r19
    add     r10, r19
    ld.w	0[r19], r18
    cmp     r0, r18
    be      _vTRAP0_Handler_ContextSwitch

    mov     #_xPortScheduleStatus, r19
    add     r10, r19
    mov     1, r17                             ; Set xPortScheduleStatus[coreID]=PORT_SCHEDULER_TASKSWITCH
    st.w	r17, 0[r19]
    br      _vTRAP0_Handler_Exit

_vTRAP0_Handler_ContextSwitch:
    mov     r10, r6                            ; Pass coreID (r10) as parameter by r6 (CCRH compiler) in SMP support.
    ; Call the scheduler to select the next task.
    ; vPortYeild may be called to current core again at the end of vTaskSwitchContext. This may case nested interrupt,
    ; however, it is not necessary to set uxInterruptNesting (currently 0) for nested trap0 exception. The user interrupt
    ; (EI level interrupt) is not accepted inside of trap0 exception.
    jarl    _vTaskSwitchContext, lp

_vTRAP0_Handler_Exit:
    ; Restore the context of the next task to run.
    portRESTORE_CONTEXT
    eiret

;------------------------------------------------------------------------------
; _Irq_Handler
; Handler interrupt service routine (ISR).
;------------------------------------------------------------------------------
_vIrq_Handler:
    ;Save used registers.
    SAVE_REGISTER

    ;Increment nesting count.
    stsr    0, r17, 2                   ; Get HTCFG0, thread configuration register
    shr     16, r17                     ; Shift right by 16 to get PEID; bit 18 to 16
    add     -1, r17                     ; Sub 1 to the core ID to make it 0 or 1
    shl     2, r17                      ; Multiply core ID by 4 (size of word) to get the correct offset

    mov     #_uxInterruptNesting, r19
    add     r17, r19                    ; Adjust address to uxInterruptNesting[coreID]
    ld.w    0[r19], r18
    addi    0x1, r18, r16
    st.w    r16, 0[r19]

    pushsp  r17, r19

    ;Call the interrupt handler.
    stsr    EIIC, r6
    andi    EIIC_MSK, r6, r6
    mov     #_uxPortMaxInterruptDepth, r15    ; Do not enable interrupt for nesting if it reach maximum depth.
    cmp     r16, r15
    be      4                                 ; Jump over ei instruction
    ei
    jarl    _common_interrupt_handler, lp
    di
    synce

    popsp   r17, r19
    st.w    r18, 0[r19]                  ; Restore the old nesting count.

    ; A context switch if no nesting interrupt.
    cmp     0x0, r18
    bne     _vIrq_Handler_NotSwitchContext

    ; Check if context switch is requested.
    mov     #_xPortScheduleStatus, r19
    add     r17, r19
    ld.w    0[r19], r18
    cmp     r0, r18
    bne     _vIrq_Handler_SwitchContext

_vIrq_Handler_NotSwitchContext:
    ; No context switch.  Restore used registers
    RESTORE_REGISTER
    eiret

;This sequence is executed for primary core only to switch context
_vIrq_Handler_SwitchContext:
    ; Clear the context switch pending flag.
    st.w r0, 0[r19]

    add     -1, r18
    bnz     _vIrq_Handler_StartFirstTask
    ; Restore used registers before saving the context to the task stack.
    RESTORE_REGISTER                   ; Restore context of interrupt
    portSAVE_CONTEXT                   ; Save task context before switching tasks
    stsr    0, r6, 2                   ;
    shr     16, r6                     ; Get Core ID and pass to vTaskSwitchContext as parameter (CCRH compiler)
    add     -1, r6                     ; The parameter is  unused in single core, no problem with this redudant setting

    ; vPortYeild may be called to current core again at the end of vTaskSwitchContext. This may case nested interrupt,
    ; however, it is not necessary to set uxInterruptNesting (currently 0) for  trap0 exception. The user interrupt
    ; (EI level interrupt) is not accepted inside of trap0 exception.
    jarl    _vTaskSwitchContext, lp    ;
    portRESTORE_CONTEXT
    eiret

_vIrq_Handler_StartFirstTask:
    RESTORE_REGISTER
    jr _vPortStartFirstTask
