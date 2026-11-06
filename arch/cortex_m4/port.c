#include <stddef.h>
#include <stdint.h>

#include "aerortos/platform.h"
#include "aerortos/port.h"
#include "aerortos/task.h"
#include "aerortos/tcb_offsets.h"

enum {
    TASK_RETURN_FAULT = 0x7A5C0001UL
};

_Static_assert(sizeof(aerortos_software_frame_t) == (8U * sizeof(uint32_t)),
               "software frame must contain R4-R11");
_Static_assert(sizeof(aerortos_hardware_frame_t) == (8U * sizeof(uint32_t)),
               "hardware exception frame must contain eight words");
_Static_assert(AERORTOS_INITIAL_FRAME_WORDS == 16U,
               "initial integer frame must contain sixteen words");
_Static_assert(offsetof(aerortos_task_t, saved_psp) ==
                   AERORTOS_TCB_SAVED_PSP_OFFSET,
               "saved PSP offset must match assembly ABI");

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(offsetof(aerortos_task_t, stack_low) ==
                   AERORTOS_TCB_STACK_LOW_OFFSET,
               "stack-low offset must match target ABI");
_Static_assert(offsetof(aerortos_task_t, stack_high) ==
                   AERORTOS_TCB_STACK_HIGH_OFFSET,
               "stack-high offset must match target ABI");
_Static_assert(offsetof(aerortos_task_t, entry) == AERORTOS_TCB_ENTRY_OFFSET,
               "entry offset must match target ABI");
_Static_assert(offsetof(aerortos_task_t, argument) == AERORTOS_TCB_ARGUMENT_OFFSET,
               "argument offset must match target ABI");
_Static_assert(sizeof(aerortos_task_t) == AERORTOS_TCB_TARGET_SIZE,
               "TCB size must match target ABI");
#endif

bool aerortos_port_build_initial_frame(
    uint32_t *stack_low,
    size_t stack_words,
    aerortos_task_entry_t entry,
    void *argument,
    uint32_t **saved_psp_out)
{
    aerortos_software_frame_t *software_frame;
    aerortos_hardware_frame_t *hardware_frame;
    uint32_t *frame_base;

    if ((stack_low == NULL) || (entry == NULL) || (saved_psp_out == NULL) ||
        (stack_words < AERORTOS_INITIAL_FRAME_WORDS)) {
        return false;
    }

    frame_base = &stack_low[stack_words - AERORTOS_INITIAL_FRAME_WORDS];
    if ((((uintptr_t)frame_base) & (AERORTOS_TASK_STACK_ALIGNMENT - 1U)) != 0U) {
        return false;
    }

    software_frame = (aerortos_software_frame_t *)frame_base;
    hardware_frame = (aerortos_hardware_frame_t *)(void *)(
        &frame_base[sizeof(*software_frame) / sizeof(uint32_t)]);

    software_frame->r4 = 0U;
    software_frame->r5 = 0U;
    software_frame->r6 = 0U;
    software_frame->r7 = 0U;
    software_frame->r8 = 0U;
    software_frame->r9 = 0U;
    software_frame->r10 = 0U;
    software_frame->r11 = 0U;

    hardware_frame->r0 = (uint32_t)(uintptr_t)argument;
    hardware_frame->r1 = 0U;
    hardware_frame->r2 = 0U;
    hardware_frame->r3 = 0U;
    hardware_frame->r12 = 0U;
    hardware_frame->lr = ((uint32_t)(uintptr_t)aerortos_task_return_trap) | 1U;
    hardware_frame->pc = ((uint32_t)(uintptr_t)entry) & ~1U;
    hardware_frame->xpsr = AERORTOS_INITIAL_XPSR;

    *saved_psp_out = frame_base;
    return true;
}

void aerortos_task_return_trap(void)
{
    platform_panic(TASK_RETURN_FAULT);
}
