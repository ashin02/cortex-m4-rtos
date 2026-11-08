.syntax unified
.cpu cortex-m4
.thumb

.section .text.aerortos_m5_register_probe, "ax", %progbits
.align 2
.global aerortos_m5_register_probe
.type aerortos_m5_register_probe, %function
.thumb_func
aerortos_m5_register_probe:
    stmdb sp!, {r4-r11, lr}
    sub sp, sp, #4
    movw r4, #0x4404
    movw r5, #0x5505
    movw r6, #0x6606
    movw r7, #0x7707
    movw r8, #0x8808
    movw r9, #0x9909
    movw r10, #0xAA0A
    movw r11, #0xBB0B
    bl aerortos_scheduler_yield
    movw r0, #0x4404
    cmp r4, r0
    bne 1f
    movw r0, #0x5505
    cmp r5, r0
    bne 1f
    movw r0, #0x6606
    cmp r6, r0
    bne 1f
    movw r0, #0x7707
    cmp r7, r0
    bne 1f
    movw r0, #0x8808
    cmp r8, r0
    bne 1f
    movw r0, #0x9909
    cmp r9, r0
    bne 1f
    movw r0, #0xAA0A
    cmp r10, r0
    bne 1f
    movw r0, #0xBB0B
    cmp r11, r0
    bne 1f
    movs r0, #1
    b 2f
1:
    movs r0, #0
2:
    add sp, sp, #4
    ldmia sp!, {r4-r11, pc}
.size aerortos_m5_register_probe, . - aerortos_m5_register_probe

.section .note.GNU-stack, "", %progbits
