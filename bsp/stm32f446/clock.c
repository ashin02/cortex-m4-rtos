/*
 * AeroRT - STM32F446 clock support
 *
 * Purpose:
 *   Eventually establish a deterministic MCU/peripheral clock tree and publish
 *   verified clock frequencies to BSP clients.
 *
 * Responsibilities:
 *   Safe oscillator/PLL/flash setup, peripheral clock gates, and explicit core,
 *   bus, and timer frequency queries.
 *
 * Expected dependencies:
 *   STM32F446 register definitions, board oscillator configuration, startup code,
 *   and documented voltage/flash limits from the reference manual/datasheet.
 *
 * TODO:
 *   [ ] Select and document the STM32F446RE board oscillator and target clock plan.
 *   [ ] Sequence voltage, flash wait states, oscillator, PLL, and bus prescalers with
 *       bounded readiness timeouts and explicit failure behavior.
 *   [ ] Account for APB timer clock multiplication when exposing TIM2/TIM5 clocks.
 *   [ ] Provide idempotent peripheral clock-enable helpers or a clear ownership API.
 *   [ ] Publish compile-time/runtime checked frequencies for timer, UART, SPI, PWM,
 *       and DWT conversion consumers.
 *
 * Non-goals:
 *   Kernel time policy, timer scheduling, or unrelated board drivers.
 *
 * Validation:
 *   Verify register fields and clock outputs against reference documentation and
 *   measure selected clock pins/timer periods on real hardware.
 */
