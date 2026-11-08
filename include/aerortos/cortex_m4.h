#ifndef AERORTOS_CORTEX_M4_H
#define AERORTOS_CORTEX_M4_H

#include <stdint.h>

typedef struct {
    volatile uint32_t CTRL;
    volatile uint32_t LOAD;
    volatile uint32_t VAL;
    volatile const uint32_t CALIB;
} aerortos_systick_t;

typedef struct {
    volatile const uint32_t CPUID;
    volatile uint32_t ICSR;
    volatile uint32_t VTOR;
} aerortos_scb_prefix_t;

#define AERORTOS_SYSTICK ((aerortos_systick_t *)0xE000E010UL)
#define AERORTOS_SCB ((aerortos_scb_prefix_t *)0xE000ED00UL)
#define AERORTOS_DHCSR (*(volatile const uint32_t *)0xE000EDF0UL)

#define AERORTOS_SYSTICK_CTRL_ENABLE_Msk (1UL << 0)
#define AERORTOS_SYSTICK_CTRL_TICKINT_Msk (1UL << 1)
#define AERORTOS_SYSTICK_CTRL_CLKSOURCE_Msk (1UL << 2)
#define AERORTOS_DHCSR_C_DEBUGEN_Msk (1UL << 0)
#define AERORTOS_SCB_ICSR_PENDSVSET_Msk (1UL << 28)
#define AERORTOS_SCB_ICSR_PENDSTSET_Msk (1UL << 26)
#define AERORTOS_SCB_CCR_STKALIGN_Msk (1UL << 9)

#define AERORTOS_SCB_CCR (*(volatile uint32_t *)0xE000ED14UL)
#define AERORTOS_SCB_SHPR2 (*(volatile uint32_t *)0xE000ED1CUL)
#define AERORTOS_SCB_SHPR3 (*(volatile uint32_t *)0xE000ED20UL)

static inline void aerortos_disable_irq(void)
{
    __asm volatile ("cpsid i" ::: "memory");
}

static inline void aerortos_wait_for_interrupt(void)
{
    __asm volatile ("wfi" ::: "memory");
}

#endif
