#ifndef AERORTOS_PORT_H
#define AERORTOS_PORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "aerortos/task.h"

#define AERORTOS_INITIAL_XPSR UINT32_C(0x01000000)
#define AERORTOS_INITIAL_FRAME_WORDS 16U

typedef struct {
    uint32_t r4;
    uint32_t r5;
    uint32_t r6;
    uint32_t r7;
    uint32_t r8;
    uint32_t r9;
    uint32_t r10;
    uint32_t r11;
} aerortos_software_frame_t;

typedef struct {
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r12;
    uint32_t lr;
    uint32_t pc;
    uint32_t xpsr;
} aerortos_hardware_frame_t;

bool aerortos_port_build_initial_frame(
    uint32_t *stack_low,
    size_t stack_words,
    aerortos_task_entry_t entry,
    void *argument,
    uint32_t **saved_psp_out);
void aerortos_task_return_trap(void) __attribute__((noreturn));

#endif
