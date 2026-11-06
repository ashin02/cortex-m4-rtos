#include <stdint.h>

#include "aerortos/cortex_m4.h"
#include "aerortos/platform.h"

enum {
    FAULT_NMI = 2UL,
    FAULT_HARD = 3UL,
    FAULT_MEMMANAGE = 4UL,
    FAULT_BUS = 5UL,
    FAULT_USAGE = 6UL,
    FAULT_UNHANDLED_IRQ = 0xFFFFFFFFUL
};

volatile uint32_t aerortos_fault_code;

void platform_panic(uint32_t code)
{
    aerortos_disable_irq();
    aerortos_fault_code = code;

    if ((AERORTOS_DHCSR & AERORTOS_DHCSR_C_DEBUGEN_Msk) != 0UL) {
        __asm volatile ("bkpt 0");
    }

    for (;;) {
        aerortos_wait_for_interrupt();
    }
}

void Default_Handler(void)
{
    platform_panic(FAULT_UNHANDLED_IRQ);
}

void NMI_Handler(void)
{
    platform_panic(FAULT_NMI);
}

void HardFault_Handler(void)
{
    platform_panic(FAULT_HARD);
}

void MemManage_Handler(void)
{
    platform_panic(FAULT_MEMMANAGE);
}

void BusFault_Handler(void)
{
    platform_panic(FAULT_BUS);
}

void UsageFault_Handler(void)
{
    platform_panic(FAULT_USAGE);
}
