#include <stdbool.h>
#include <stdint.h>

#include "aerortos/cortex_m4.h"
#include "aerortos/platform.h"
#include "aerortos/stm32f446re.h"

enum {
    HSI_FREQUENCY_HZ = 16000000UL,
    CLOCK_READY_TIMEOUT = 1000000UL
};

uint32_t SystemCoreClock = HSI_FREQUENCY_HZ;

bool system_clock_init(void)
{
    uint32_t timeout = CLOCK_READY_TIMEOUT;

    STM32_RCC->CR |= STM32_RCC_CR_HSION_Msk;
    while (((STM32_RCC->CR & STM32_RCC_CR_HSIRDY_Msk) == 0UL) &&
           (timeout > 0UL)) {
        --timeout;
    }
    if (timeout == 0UL) {
        return false;
    }

    STM32_RCC->CFGR &= ~STM32_RCC_CFGR_SW_Msk;

    timeout = CLOCK_READY_TIMEOUT;
    while (((STM32_RCC->CFGR & STM32_RCC_CFGR_SWS_Msk) != 0UL) &&
           (timeout > 0UL)) {
        --timeout;
    }
    if (timeout == 0UL) {
        return false;
    }

    STM32_RCC->CFGR &= ~(STM32_RCC_CFGR_HPRE_Msk |
                         STM32_RCC_CFGR_PPRE1_Msk |
                         STM32_RCC_CFGR_PPRE2_Msk);

    STM32_RCC->CR &= ~(STM32_RCC_CR_PLLON_Msk | STM32_RCC_CR_HSEON_Msk);
    timeout = CLOCK_READY_TIMEOUT;
    while (((STM32_RCC->CR & STM32_RCC_CR_PLLRDY_Msk) != 0UL) &&
           (timeout > 0UL)) {
        --timeout;
    }
    if (timeout == 0UL) {
        return false;
    }

    STM32_FLASH->ACR &= ~STM32_FLASH_ACR_LATENCY_Msk;
    AERORTOS_SCB->VTOR = 0x08000000UL;
    SystemCoreClock = HSI_FREQUENCY_HZ;
    return true;
}
