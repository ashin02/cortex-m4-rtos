#include <stddef.h>
#include <stdint.h>

#include "aerortos/cortex_m4.h"
#include "aerortos/critical_section.h"
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
_Static_assert(AERORTOS_NVIC_PRIORITY_BITS == 4U,
               "STM32F446 implements four NVIC priority bits");
_Static_assert((AERORTOS_SVC_PRIORITY & UINT32_C(0x0F)) == 0U,
               "SVC priority must use implemented priority bits");
_Static_assert((AERORTOS_KERNEL_BASEPRI & UINT32_C(0x0F)) == 0U,
               "BASEPRI must use implemented priority bits");
_Static_assert((AERORTOS_PENDSV_PRIORITY & UINT32_C(0x0F)) == 0U,
               "PendSV priority must use implemented priority bits");
_Static_assert(AERORTOS_SVC_PRIORITY < AERORTOS_KERNEL_BASEPRI,
               "SVC must remain above the kernel-aware IRQ ceiling");
_Static_assert(AERORTOS_KERNEL_BASEPRI < AERORTOS_PENDSV_PRIORITY,
               "PendSV must be masked by kernel critical sections");
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

void aerortos_port_init(void)
{
#if !defined(AERORTOS_HOST_TEST)
    AERORTOS_SCB_CCR |= AERORTOS_SCB_CCR_STKALIGN_Msk;
    AERORTOS_SCB_SHPR2 = (AERORTOS_SCB_SHPR2 & UINT32_C(0x00FFFFFF)) |
                         (AERORTOS_SVC_PRIORITY << 24);
    AERORTOS_SCB_SHPR3 = (AERORTOS_SCB_SHPR3 & UINT32_C(0xFF00FFFF)) |
                         (AERORTOS_PENDSV_PRIORITY << 16);
    __asm volatile ("dsb\n isb" ::: "memory");
#endif
}

void aerortos_port_request_context_switch(void)
{
#if defined(AERORTOS_HOST_TEST)
    extern uint32_t aerortos_host_pendsv_requests;
    ++aerortos_host_pendsv_requests;
#else
    AERORTOS_SCB->ICSR = AERORTOS_SCB_ICSR_PENDSVSET_Msk;
    __asm volatile ("dsb\n isb" ::: "memory");
#endif
}

void aerortos_port_start_first_task(void)
{
#if defined(AERORTOS_HOST_TEST)
    platform_panic(UINT32_C(0x7A5C0003));
#else
    __asm volatile ("svc 0" ::: "memory");
    __builtin_unreachable();
#endif
}

bool aerortos_port_in_handler_mode(void)
{
#if defined(AERORTOS_HOST_TEST)
    extern bool aerortos_host_handler_mode;
    return aerortos_host_handler_mode;
#else
    uint32_t ipsr;

    __asm volatile ("mrs %0, ipsr" : "=r" (ipsr));
    return ipsr != 0U;
#endif
}
