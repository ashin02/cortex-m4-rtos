#ifndef AERORTOS_STM32F446RE_H
#define AERORTOS_STM32F446RE_H

#include <stdint.h>

typedef struct {
    volatile uint32_t MODER;
    volatile uint32_t OTYPER;
    volatile uint32_t OSPEEDR;
    volatile uint32_t PUPDR;
    volatile const uint32_t IDR;
    volatile uint32_t ODR;
    volatile uint32_t BSRR;
    volatile uint32_t LCKR;
    volatile uint32_t AFR[2];
} stm32_gpio_t;

typedef struct {
    volatile uint32_t CR;
    volatile uint32_t PLLCFGR;
    volatile uint32_t CFGR;
    volatile uint32_t CIR;
    volatile uint32_t AHB1RSTR;
    volatile uint32_t AHB2RSTR;
    volatile uint32_t AHB3RSTR;
    uint32_t RESERVED0;
    volatile uint32_t APB1RSTR;
    volatile uint32_t APB2RSTR;
    uint32_t RESERVED1[2];
    volatile uint32_t AHB1ENR;
} stm32_rcc_t;

typedef struct {
    volatile uint32_t ACR;
} stm32_flash_t;

#define STM32_GPIOA ((stm32_gpio_t *)0x40020000UL)
#define STM32_RCC ((stm32_rcc_t *)0x40023800UL)
#define STM32_FLASH ((stm32_flash_t *)0x40023C00UL)

#define STM32_RCC_CR_HSION_Msk (1UL << 0)
#define STM32_RCC_CR_HSIRDY_Msk (1UL << 1)
#define STM32_RCC_CR_HSEON_Msk (1UL << 16)
#define STM32_RCC_CR_PLLON_Msk (1UL << 24)
#define STM32_RCC_CR_PLLRDY_Msk (1UL << 25)
#define STM32_RCC_CFGR_SW_Msk (3UL << 0)
#define STM32_RCC_CFGR_SWS_Msk (3UL << 2)
#define STM32_RCC_CFGR_HPRE_Msk (15UL << 4)
#define STM32_RCC_CFGR_PPRE1_Msk (7UL << 10)
#define STM32_RCC_CFGR_PPRE2_Msk (7UL << 13)
#define STM32_RCC_AHB1ENR_GPIOAEN_Msk (1UL << 0)

#define STM32_FLASH_ACR_LATENCY_Msk (7UL << 0)

#endif
