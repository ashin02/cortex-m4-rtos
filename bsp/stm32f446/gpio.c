#include <stdint.h>

#include "aerortos/platform.h"
#include "aerortos/stm32f446re.h"

enum {
    NUCLEO_LD2_PIN = 5UL,
    GPIO_MODE_BITS = 2UL,
    GPIO_MODE_OUTPUT = 1UL,
    GPIO_SPEED_LOW = 0UL
};

void gpio_led_init(void)
{
    const uint32_t mode_shift = NUCLEO_LD2_PIN * GPIO_MODE_BITS;
    volatile uint32_t clock_enable_readback;

    STM32_RCC->AHB1ENR |= STM32_RCC_AHB1ENR_GPIOAEN_Msk;
    clock_enable_readback = STM32_RCC->AHB1ENR;
    (void)clock_enable_readback;

    STM32_GPIOA->BSRR = 1UL << (NUCLEO_LD2_PIN + 16UL);
    STM32_GPIOA->MODER = (STM32_GPIOA->MODER & ~(3UL << mode_shift)) |
                         (GPIO_MODE_OUTPUT << mode_shift);
    STM32_GPIOA->OTYPER &= ~(1UL << NUCLEO_LD2_PIN);
    STM32_GPIOA->OSPEEDR = (STM32_GPIOA->OSPEEDR & ~(3UL << mode_shift)) |
                           (GPIO_SPEED_LOW << mode_shift);
    STM32_GPIOA->PUPDR &= ~(3UL << mode_shift);
}

void gpio_led_toggle(void)
{
    const uint32_t pin_mask = 1UL << NUCLEO_LD2_PIN;
    const bool led_is_on = (STM32_GPIOA->ODR & pin_mask) != 0UL;

    STM32_GPIOA->BSRR = led_is_on ? (pin_mask << 16UL) : pin_mask;
}
