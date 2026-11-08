#include "aerortos/tcb_offsets.h"

#if AERORTOS_TCB_SAVED_PSP_OFFSET != 0
#error "PendSV assembly requires saved_psp at TCB offset zero"
#endif

.syntax unified
.cpu cortex-m4
.thumb

.equ AERORTOS_EXC_RETURN_THREAD_PSP, 0xFFFFFFFD

.section .text.SVC_Handler, "ax", %progbits
.align 2
.global SVC_Handler
.type SVC_Handler, %function
.thumb_func
SVC_Handler:
    ldr r1, =aerortos_current_task
    ldr r1, [r1]
    cbz r1, 1f
    ldr r0, [r1, #AERORTOS_TCB_SAVED_PSP_OFFSET]
    ldmia r0!, {r4-r11}
    msr psp, r0
    movs r0, #0
    msr basepri, r0
    ldr lr, =AERORTOS_EXC_RETURN_THREAD_PSP
    bx lr
1:
    b Default_Handler
.size SVC_Handler, . - SVC_Handler

.section .text.PendSV_Handler, "ax", %progbits
.align 2
.global PendSV_Handler
.type PendSV_Handler, %function
.thumb_func
PendSV_Handler:
    mrs r0, psp
    ldr r3, =aerortos_current_task
    ldr r2, [r3]
    cbz r2, 1f
    stmdb r0!, {r4-r11}
    str r0, [r2, #AERORTOS_TCB_SAVED_PSP_OFFSET]

    push {r3, lr}
    bl aerortos_scheduler_switch_context
    pop {r3, lr}
    cbz r0, 1f
    ldr r0, [r0, #AERORTOS_TCB_SAVED_PSP_OFFSET]
    ldmia r0!, {r4-r11}
    msr psp, r0
    ldr lr, =AERORTOS_EXC_RETURN_THREAD_PSP
    bx lr
1:
    b Default_Handler
.size PendSV_Handler, . - PendSV_Handler

.section .note.GNU-stack, "", %progbits
