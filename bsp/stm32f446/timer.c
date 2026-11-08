#include <stdbool.h>
#include <stdint.h>

#include "aerortos/cortex_m4.h"
#include "aerortos/critical_section.h"
#include "aerortos/platform.h"
#include "aerortos/timer.h"

volatile uint32_t aerortos_systick_irq_count;

bool aerortos_systick_init(void)
{
    const uint32_t reload = SystemCoreClock / AERORTOS_TICK_HZ;

    if ((SystemCoreClock % AERORTOS_TICK_HZ) != 0U ||
        (reload == 0U) || (reload > UINT32_C(0x01000000))) {
        return false;
    }

    AERORTOS_SCB_SHPR3 = (AERORTOS_SCB_SHPR3 & UINT32_C(0x00FFFFFF)) |
                         (AERORTOS_KERNEL_BASEPRI << 24);
    aerortos_systick_irq_count = 0U;
    AERORTOS_SYSTICK->CTRL = 0U;
    AERORTOS_SYSTICK->LOAD = reload - 1U;
    AERORTOS_SYSTICK->VAL = 0U;
    AERORTOS_SYSTICK->CTRL = AERORTOS_SYSTICK_CTRL_CLKSOURCE_Msk |
                             AERORTOS_SYSTICK_CTRL_TICKINT_Msk |
                             AERORTOS_SYSTICK_CTRL_ENABLE_Msk;
    return true;
}

bool systick_heartbeat_init(void)
{
    aerortos_time_system_init(0U);
    return aerortos_systick_init();
}

uint32_t bsp_millis(void)
{
    return aerortos_tick_now();
}

void SysTick_Handler(void)
{
    ++aerortos_systick_irq_count;
    aerortos_timer_tick_isr();
}
